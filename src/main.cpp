#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>

#include <SDL.h>

#include "app/app.hpp"
#include "core/mgba_core.hpp"
#include "input/input_frame.hpp"
#include "platform/sdl_audio.hpp"
#include "platform/sdl_platform.hpp"
#include "render/renderer.hpp"
#include "storage/paths.hpp"
#include "util/cli.hpp"

namespace {

constexpr double kGbaFramesPerSecond = 59.727500569606;
constexpr int kAudioMaxFramesPerTick = 2048;
constexpr int kAudioDeviceBufferFrames = 1024;
constexpr int kAudioStartBufferFrames = 4096;
constexpr int kAudioQueueLimitFrames = 8192;

std::filesystem::path current_working_directory()
{
    try {
        return std::filesystem::current_path();
    } catch (...) {
        return {};
    }
}

czgba::core::MgbaLogLevel mgba_log_level_from_cli(const std::string& value)
{
    if (value == "debug") {
        return czgba::core::MgbaLogLevel::Debug;
    }
    if (value == "info") {
        return czgba::core::MgbaLogLevel::Info;
    }
    if (value == "warn") {
        return czgba::core::MgbaLogLevel::Warn;
    }
    return czgba::core::MgbaLogLevel::Error;
}

} // namespace

int main(int argc, char** argv)
{
    czgba::util::CliOptions options;
    std::string cli_error;
    if (!czgba::util::parse_cli(argc, argv, options, cli_error)) {
        std::cerr << cli_error << '\n';
        czgba::util::print_help(argv[0]);
        return 2;
    }

    if (options.help) {
        czgba::util::print_help(argv[0]);
        return 0;
    }

    if (options.version) {
        czgba::util::print_version();
        return 0;
    }

    czgba::platform::SdlPlatform platform;
    (void)options.scale;
    if (!platform.init({options.kiosk, options.fullscreen})) {
        return 1;
    }

    czgba::platform::SdlAudio audio;
    czgba::platform::SdlAudioConfig audio_config;
    audio_config.sample_rate = 48000;
    audio_config.channels = 2;
    audio_config.device_buffer_frames = kAudioDeviceBufferFrames;
    audio_config.start_buffer_frames = kAudioStartBufferFrames;
    audio_config.buffer_limit_frames = kAudioQueueLimitFrames;
    const bool audio_ok = !options.no_audio && audio.init(audio_config);
    if (options.no_audio) {
        std::cerr << "Audio disabled; continuing muted.\n";
    } else if (!audio_ok) {
        std::cerr << "Audio unavailable; continuing muted.\n";
    }

    auto core = std::make_unique<czgba::core::MgbaCore>(mgba_log_level_from_cli(options.log_level));
    czgba::app::App app(std::move(core), current_working_directory());
    if (!options.rom_path.empty()) {
        app.start_with_rom(czgba::storage::path_from_utf8(options.rom_path));
    }

    czgba::render::Renderer renderer;
    czgba::input::InputFrame input;

    using clock = std::chrono::steady_clock;
    const auto frame_duration = std::chrono::duration_cast<clock::duration>(
        std::chrono::duration<double>(1.0 / kGbaFramesPerSecond));
    auto next_frame = clock::now();
    auto previous_tick = next_frame;
    int presented_frames = 0;

    while (!platform.should_quit() && !app.should_quit()) {
        const auto now = clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - previous_tick);
        previous_tick = now;

        platform.poll_events(input);
        app.tick(elapsed);
        app.update(input);

        if (audio.active()) {
            const auto samples = app.read_audio_samples(audio.sample_rate(), kAudioMaxFramesPerTick);
            audio.write(samples.samples());
        } else if (!audio.active()) {
            app.read_audio_samples(48000, kAudioMaxFramesPerTick);
        }

        renderer.draw(app.render_state());
        platform.present(renderer.canvas().data(), renderer.canvas().pitch_bytes());
        ++presented_frames;

        if (options.max_frames > 0 && presented_frames >= options.max_frames) {
            break;
        }

        next_frame += frame_duration;
        std::this_thread::sleep_until(next_frame);
        if (clock::now() > next_frame + std::chrono::milliseconds(100)) {
            next_frame = clock::now();
        }
    }

    return 0;
}
