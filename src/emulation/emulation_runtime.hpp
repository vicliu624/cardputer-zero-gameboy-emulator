#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "app/app.hpp"
#include "audio/pcm_ring.hpp"
#include "core/mgba_core.hpp"
#include "input/input_frame.hpp"

namespace czgba::emulation {

struct RuntimeConfig {
    std::filesystem::path working_directory;
    std::string rom_path;
    core::MgbaLogLevel log_level = core::MgbaLogLevel::Error;
    int sample_rate = 44100;
    bool use_audio_clock = true;
    std::size_t target_audio_frames = 3072;
    std::size_t start_audio_frames = 2646;
    int max_audio_read_frames = 4096;
};

struct RenderSnapshot {
    app::RenderState state;
    core::GbaVideoFrame video_frame;
    std::vector<std::uint32_t> pixels;
    std::uint64_t emulation_frame = 0;
    std::uint64_t audio_frame_end = 0;
};

struct RuntimeMetrics {
    std::uint64_t emulated_frames = 0;
    std::uint64_t audio_produced_frames = 0;
    std::uint64_t audio_write_failures = 0;
    std::uint64_t video_frames_dropped = 0;
};

// Owns App and therefore the mGBA core on one worker thread. SDL/Wayland live
// on the UI thread and cannot block this producer. PCM crosses the boundary
// only through PcmRing; render snapshots are a bounded latest-wins timeline.
class EmulationRuntime {
public:
    using CoreFactory = std::function<std::unique_ptr<core::GbaCore>()>;

    EmulationRuntime(RuntimeConfig config, audio::PcmRing& pcm_ring, CoreFactory core_factory);
    ~EmulationRuntime();

    EmulationRuntime(const EmulationRuntime&) = delete;
    EmulationRuntime& operator=(const EmulationRuntime&) = delete;

    void start();
    void stop();

    void submit_input(const input::InputFrame& input);
    void notify_audio_paused();

    bool audio_ready() const;
    RuntimeMetrics metrics() const;

    // Returns the newest snapshot that should be shown now without an audio
    // clock (menus, mute mode, and initial setup). When the display thread
    // falls behind, intermediate snapshots are discarded rather than forcing
    // the emulation worker to wait.
    std::shared_ptr<const RenderSnapshot> take_latest_snapshot();

    // Choose a video frame against the SDL callback's monotonically advancing
    // playback position. `presentation_lead_frames` is normally one callback
    // quantum, which prevents a just-finished callback from making video lag
    // behind the audio it already handed to the kernel. The method never
    // blocks emulation while the compositor is late.
    std::shared_ptr<const RenderSnapshot> take_snapshot_for_audio(
        std::uint64_t playback_frame,
        std::size_t presentation_lead_frames);

private:
    void run(std::stop_token stop_token);
    input::InputFrame take_input();
    void drain_pending_audio(app::App& app);
    void publish_snapshot(const app::App& app);
    std::shared_ptr<RenderSnapshot> make_snapshot(const app::App& app) const;
    void set_audio_ready(bool ready);

    RuntimeConfig config_;
    audio::PcmRing& pcm_ring_;
    CoreFactory core_factory_;
    std::jthread worker_;
    std::condition_variable_any wake_;

    mutable std::mutex input_mutex_;
    core::GbaInputState latest_input_{};
    std::deque<app::AppAction> pending_actions_;
    bool quit_requested_ = false;

    mutable std::mutex snapshot_mutex_;
    std::deque<std::shared_ptr<const RenderSnapshot>> render_timeline_;
    std::shared_ptr<const RenderSnapshot> latest_snapshot_;

    std::atomic<bool> audio_ready_{false};
    std::atomic<bool> audio_reset_requested_{false};
    std::atomic<std::uint64_t> emulated_frames_{0};
    std::atomic<std::uint64_t> audio_produced_frames_{0};
    std::atomic<std::uint64_t> audio_write_failures_{0};
    std::atomic<std::uint64_t> video_frames_dropped_{0};
};

} // namespace czgba::emulation
