#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>

#include <SDL.h>

#include "audio/pcm_ring.hpp"
#include "core/mgba_core.hpp"
#include "emulation/emulation_runtime.hpp"
#include "input/input_frame.hpp"
#include "platform/presentation_profile.hpp"
#include "platform/sdl_audio.hpp"
#include "platform/sdl_platform.hpp"
#include "render/renderer.hpp"
#include "util/cli.hpp"

namespace {

constexpr double kPresentationFramesPerSecond = 59.727500569606;
constexpr int kK230PreferredSampleRate = 44100;
constexpr int kDefaultPreferredSampleRate = 48000;
constexpr int kK230CallbackFrames = 512;
constexpr int kDefaultCallbackFrames = 1024;
constexpr int kAudioTargetMilliseconds = 80;
constexpr int kAudioStartMilliseconds = 60;
constexpr int kAudioCapacityMilliseconds = 160;

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

czgba::platform::PresentationProfile presentation_profile_from_cli(const std::string& value)
{
    if (value == "tdvp-k230") {
        return czgba::platform::PresentationProfile::TdvpK230;
    }
    return czgba::platform::PresentationProfile::CardputerZero;
}

czgba::render::RenderLayoutProfile render_layout_profile_from_presentation(
    czgba::platform::PresentationProfile presentation_profile)
{
    return presentation_profile == czgba::platform::PresentationProfile::TdvpK230
        ? czgba::render::RenderLayoutProfile::TdvpK230
        : czgba::render::RenderLayoutProfile::CardputerZero;
}

int frames_for_milliseconds(int sample_rate, int milliseconds)
{
    return std::max(1, (sample_rate * milliseconds) / 1000);
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

    const auto presentation_profile = presentation_profile_from_cli(options.device_profile);
    const auto render_layout_profile = render_layout_profile_from_presentation(presentation_profile);
    const auto canvas_spec = czgba::render::canvas_spec(render_layout_profile);
    const bool k230_profile = presentation_profile == czgba::platform::PresentationProfile::TdvpK230;
    const int preferred_sample_rate = k230_profile ? kK230PreferredSampleRate : kDefaultPreferredSampleRate;

    czgba::platform::SdlPlatform platform;
    (void)options.scale;
    if (!platform.init({options.kiosk, options.fullscreen, presentation_profile, canvas_spec.width, canvas_spec.height})) {
        return 1;
    }

    const auto ring_capacity_frames = frames_for_milliseconds(preferred_sample_rate, kAudioCapacityMilliseconds);
    czgba::audio::PcmRing pcm_ring(static_cast<std::size_t>(ring_capacity_frames) * 2);
    czgba::platform::SdlAudio audio;
    czgba::platform::SdlAudioConfig audio_config;
    audio_config.sample_rate = preferred_sample_rate;
    audio_config.channels = 2;
    audio_config.callback_buffer_frames = k230_profile ? kK230CallbackFrames : kDefaultCallbackFrames;
    audio_config.start_buffer_frames = frames_for_milliseconds(preferred_sample_rate, kAudioStartMilliseconds);
    const bool audio_ok = !options.no_audio && audio.init(pcm_ring, audio_config);
    if (options.no_audio) {
        std::cerr << "Audio disabled; continuing muted.\n";
    } else if (!audio_ok) {
        std::cerr << "Audio unavailable; continuing muted.\n";
    }

    const int runtime_sample_rate = audio_ok ? audio.sample_rate() : preferred_sample_rate;
    const auto make_audio_clock_config = [&](int sample_rate, bool use_audio_clock) {
        czgba::emulation::AudioClockConfig config;
        config.sample_rate = sample_rate;
        config.use_audio_clock = use_audio_clock;
        config.target_audio_frames = static_cast<std::size_t>(frames_for_milliseconds(sample_rate, kAudioTargetMilliseconds));
        config.start_audio_frames = static_cast<std::size_t>(frames_for_milliseconds(sample_rate, kAudioStartMilliseconds));
        return config;
    };
    const auto initial_audio_clock = make_audio_clock_config(runtime_sample_rate, audio_ok);
    czgba::emulation::RuntimeConfig runtime_config;
    runtime_config.working_directory = current_working_directory();
    runtime_config.rom_path = options.rom_path;
    runtime_config.log_level = mgba_log_level_from_cli(options.log_level);
    runtime_config.sample_rate = runtime_sample_rate;
    runtime_config.use_audio_clock = initial_audio_clock.use_audio_clock;
    runtime_config.target_audio_frames = initial_audio_clock.target_audio_frames;
    runtime_config.start_audio_frames = initial_audio_clock.start_audio_frames;
    runtime_config.max_audio_read_frames = 4096;
    const auto core_log_level = runtime_config.log_level;
    czgba::emulation::EmulationRuntime runtime(
        std::move(runtime_config),
        pcm_ring,
        [core_log_level] { return std::make_unique<czgba::core::MgbaCore>(core_log_level); });
    runtime.start();

    czgba::render::Renderer renderer(render_layout_profile);
    czgba::input::InputFrame input;
    std::shared_ptr<const czgba::emulation::RenderSnapshot> displayed_snapshot;

    using clock = std::chrono::steady_clock;
    const auto presentation_duration = std::chrono::duration_cast<clock::duration>(
        std::chrono::duration<double>(1.0 / kPresentationFramesPerSecond));
    auto next_presentation = clock::now();
    auto next_telemetry = next_presentation + std::chrono::seconds(1);
    auto next_audio_retry = next_presentation;
    std::uint64_t observed_underruns = 0;
    int presented_frames = 0;

    while (!platform.should_quit()) {
        platform.poll_events(input);
        runtime.submit_input(input);

        const auto loop_now = clock::now();
        if (!options.no_audio && platform.take_audio_device_removed()) {
            std::cerr << "SDL playback device removed; rebuilding the audio sink\n";
            if (audio.reopen()) {
                observed_underruns = 0;
                runtime.request_audio_reconfigure(make_audio_clock_config(audio.sample_rate(), true));
            } else {
                runtime.request_audio_reconfigure(make_audio_clock_config(preferred_sample_rate, false));
                next_audio_retry = loop_now + std::chrono::seconds(2);
            }
        }

        // A PulseAudio restart does not reliably produce an SDL removal event
        // on every backend. Retry an unavailable device at a bounded cadence;
        // while unavailable, keep mGBA running in its explicit muted mode
        // rather than filling a PCM ring that no callback can consume.
        if (!options.no_audio && !audio.active() && loop_now >= next_audio_retry) {
            if (audio.reopen()) {
                std::cerr << "SDL audio sink recovered: " << audio.driver_name()
                          << " at " << audio.sample_rate() << " Hz\n";
                observed_underruns = 0;
                runtime.request_audio_reconfigure(make_audio_clock_config(audio.sample_rate(), true));
            } else {
                runtime.request_audio_reconfigure(make_audio_clock_config(preferred_sample_rate, false));
            }
            next_audio_retry = loop_now + std::chrono::seconds(2);
        }

        czgba::platform::SdlAudioMetrics audio_metrics;
        if (audio.active()) {
            audio_metrics = audio.metrics();
            if (audio_metrics.underrun_frames > observed_underruns && audio.playing()) {
                // Recover on the next UI iteration rather than waiting for
                // periodic telemetry. The callback has already inserted
                // silence; pausing now prevents a repeated underrun burst.
                std::cerr << "AUDIO underrun detected; pausing and rebuilding the PCM prebuffer\n";
                audio.recover_from_underrun();
                runtime.notify_audio_paused();
                audio_metrics = audio.metrics();
            }
            observed_underruns = audio_metrics.underrun_frames;

            if (runtime.audio_ready()) {
                (void)audio.start_if_prebuffered();
            } else if (audio.playing()) {
                // SDL_PauseAudioDevice stops the callback before PcmRing is
                // cleared, so resume always starts from a complete prebuffer.
                audio.begin_prebuffering();
                runtime.notify_audio_paused();
            }
        }

        if (audio.playing()) {
            if (auto snapshot = runtime.take_snapshot_for_audio(
                    audio_metrics.callback_frames,
                    static_cast<std::size_t>(audio.callback_buffer_frames()))) {
                displayed_snapshot = std::move(snapshot);
            }
        } else if (auto snapshot = runtime.take_latest_snapshot()) {
            displayed_snapshot = std::move(snapshot);
        }

        if (displayed_snapshot) {
            renderer.draw(displayed_snapshot->state);
            if (options.present_delay_ms != 0) {
                // Test-only synthetic compositor/UI pressure. The emulation
                // worker must still keep PCM above the low water level while
                // this thread is deliberately late.
                std::this_thread::sleep_for(std::chrono::milliseconds(options.present_delay_ms));
            }
            platform.present(
                renderer.canvas().data(),
                renderer.canvas().width(),
                renderer.canvas().height(),
                renderer.canvas().pitch_bytes());
            ++presented_frames;
        }

        const auto now = clock::now();
        if (audio.active() && now >= next_telemetry) {
            audio_metrics = audio.metrics();
            const auto runtime_metrics = runtime.metrics();
            std::cout << "AUDIO telemetry: driver=" << audio.driver_name()
                      << " state=" << czgba::platform::sdl_audio_state_name(audio.state())
                      << " rate=" << audio.sample_rate()
                      << " callback=" << audio.callback_buffer_frames()
                      << " queued=" << audio_metrics.queued_frames
                      << " low=" << audio_metrics.low_queued_frames
                      << " high=" << audio_metrics.high_queued_frames
                      << " underrun=" << audio_metrics.underrun_frames
                      << " rejected=" << audio_metrics.rejected_frames
                      << " recoveries=" << audio_metrics.recovery_count
                      << " reopens=" << audio_metrics.reopen_count
                      << " jitter-us(p50/p95/p99)=" << audio_metrics.callback_jitter_p50_us
                      << '/' << audio_metrics.callback_jitter_p95_us
                      << '/' << audio_metrics.callback_jitter_p99_us
                      << " emulated=" << runtime_metrics.emulated_frames
                      << " video-drop=" << runtime_metrics.video_frames_dropped << '\n';

            next_telemetry = now + std::chrono::seconds(1);
        }

        if (options.max_frames > 0 && presented_frames >= options.max_frames) {
            break;
        }

        next_presentation += presentation_duration;
        std::this_thread::sleep_until(next_presentation);
        if (clock::now() > next_presentation + std::chrono::milliseconds(100)) {
            next_presentation = clock::now();
        }
    }

    runtime.stop();
    return 0;
}
