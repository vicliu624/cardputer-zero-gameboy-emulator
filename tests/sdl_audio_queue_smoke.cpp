#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "platform/sdl_audio.hpp"

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "SDL audio queue smoke failed: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main()
{
    // Exercise the queued-audio state machine without opening a host audio
    // device. The hint remains a no-op on unpatched upstream SDL2, which is
    // exactly the compatibility contract needed by non-TDVP builds.
    require(SDL_setenv("SDL_AUDIODRIVER", "dummy", 1) == 0,
            "can select SDL's dummy audio driver");

    czgba::platform::SdlAudio audio;
    czgba::platform::SdlAudioConfig config;
    config.sample_rate = 48000;
    config.channels = 2;
    config.device_buffer_frames = 128;
    config.start_buffer_frames = 256;
    config.buffer_limit_frames = 512;
    config.pulse_playback_buffer_frames = 1024;
    require(audio.init(config), "opens a queued dummy-audio device");
    require(std::string_view(SDL_GetHint("SDL_AUDIO_PULSEAUDIO_BUFFER_FRAMES")) == "1024",
            "TDVP PulseAudio stream buffer request is set before opening audio");

    std::vector<std::int16_t> one_fragment(128 * config.channels, 0);
    require(audio.write(one_fragment) == one_fragment.size(),
            "first fragment is accepted");
    auto stats = audio.stats();
    require(stats.samples_submitted == one_fragment.size() &&
                stats.samples_queued == one_fragment.size() &&
                stats.samples_dropped == 0,
            "first fragment accounting is exact");
    require(!stats.playback_started,
            "playback remains paused until the complete prebuffer exists");

    require(audio.write(one_fragment) == one_fragment.size(),
            "second fragment is accepted");
    stats = audio.stats();
    require(stats.samples_submitted == one_fragment.size() * 2 &&
                stats.samples_queued == one_fragment.size() * 2 &&
                stats.samples_dropped == 0,
            "prebuffer accounting remains exact at playback start");
    require(stats.playback_started,
            "playback begins once the configured prebuffer is queued");

    audio.pause(true);
    stats = audio.stats();
    require(!stats.playback_started && stats.queued_samples == 0 && stats.pending_samples == 0,
            "pausing clears the queued-audio epoch");

    // One producer tick can exceed the currently writable SDL queue. It must
    // be retained for a later service() call rather than silently truncating
    // the emulated PCM stream.
    std::vector<std::int16_t> oversized_fragment(
        static_cast<std::size_t>(config.buffer_limit_frames * config.channels * 2), 0);
    require(audio.write(oversized_fragment) == oversized_fragment.size(),
            "an oversized producer fragment is accepted in full");
    stats = audio.stats();
    require(stats.samples_submitted == one_fragment.size() * 2 + oversized_fragment.size() &&
                stats.samples_dropped == 0 && stats.queue_failures == 0,
            "overflow is retained without PCM loss or a queue failure");

    audio.pause(true);
    stats = audio.stats();
    require(stats.queued_samples == 0 && stats.pending_samples == 0,
            "pausing clears both SDL and retained PCM queues");
    return 0;
}
