#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <thread>

#include "audio/pcm_ring.hpp"
#include "emulation/emulation_runtime.hpp"

namespace {

class FakeCore final : public czgba::core::GbaCore {
public:
    bool load_rom(const std::string&) override
    {
        loaded_ = true;
        return true;
    }

    void unload_rom() override
    {
        loaded_ = false;
    }

    void reset() override {}

    void run_until_next_frame() override
    {
        if (loaded_) {
            pending_frames_ += 735;
        }
    }

    void set_input(const czgba::core::GbaInputState&) override {}
    czgba::core::GbaVideoFrame video_frame() const override { return {}; }

    czgba::audio::AudioSampleBatch read_audio_samples(int sample_rate, int max_frames) override
    {
        const int frames = std::min(pending_frames_, max_frames);
        pending_frames_ -= frames;
        czgba::audio::AudioSampleBatch batch;
        batch.sample_rate = sample_rate;
        batch.interleaved_s16.resize(static_cast<std::size_t>(frames) * 2);
        for (int frame = 0; frame < frames; ++frame) {
            const auto value = static_cast<std::int16_t>((sample_cursor_ + frame) & 0x7fff);
            batch.interleaved_s16[static_cast<std::size_t>(frame) * 2] = value;
            batch.interleaved_s16[static_cast<std::size_t>(frame) * 2 + 1] = value;
        }
        sample_cursor_ += frames;
        return batch;
    }

    bool load_sram(const std::string&) override { return true; }
    bool save_sram(const std::string&) override { return true; }
    bool load_state(const std::string&) override { return true; }
    bool save_state(const std::string&) override { return true; }
    bool load_cheats(const std::string&) override { return true; }
    bool set_cheat_enabled(const std::string&, bool) override { return true; }
    std::string game_title() const override { return "RUNTIME"; }
    std::string game_code() const override { return "TEST"; }

private:
    bool loaded_ = false;
    int pending_frames_ = 0;
    int sample_cursor_ = 0;
};

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "emulation runtime smoke failed: " << message << '\n';
        std::exit(1);
    }
}

bool wait_until(const std::function<bool()>& predicate, std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return predicate();
}

} // namespace

int main()
{
    czgba::audio::PcmRing ring(4096);
    czgba::emulation::RuntimeConfig config;
    config.working_directory = std::filesystem::temp_directory_path() / "cardputer-zero-gba-runtime-smoke";
    config.rom_path = "fake.gba";
    config.sample_rate = 44100;
    config.target_audio_frames = 1000;
    config.start_audio_frames = 500;
    config.max_audio_read_frames = 512;

    czgba::emulation::EmulationRuntime runtime(
        config,
        ring,
        [] { return std::make_unique<FakeCore>(); });
    runtime.start();

    require(wait_until([&] { return runtime.audio_ready(); }, std::chrono::milliseconds(1000)),
            "worker prebuffers PCM before enabling playback");
    require(ring.queued_frames() >= config.start_audio_frames, "ring reaches the configured start water level");

    std::int16_t drained[2000]{};
    const auto read = ring.read_some(drained);
    require(read == 2000, "consumer can drain complete interleaved samples");
    require(wait_until([&] { return ring.queued_frames() >= config.start_audio_frames; }, std::chrono::milliseconds(1000)),
            "worker refills after consumer progress");

    const auto metrics = runtime.metrics();
    require(metrics.emulated_frames > 0, "mGBA owner advanced frames independently of UI presentation");
    require(metrics.audio_produced_frames > 0, "worker produced PCM frames");
    require(metrics.audio_write_failures == 0, "PCM batches are never silently short-written");
    require(runtime.take_latest_snapshot() != nullptr, "runtime publishes a render snapshot without sharing the core");
    require(runtime.take_snapshot_for_audio(0, 512) != nullptr,
            "audio-clocked presentation has a bounded initial snapshot");

    runtime.stop();
    return 0;
}
