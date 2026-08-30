#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

#include <SDL.h>

#include "audio/pcm_ring.hpp"

namespace czgba::platform {

struct SdlAudioConfig {
    int sample_rate = 44100;
    int channels = 2;
    int callback_buffer_frames = 512;
    int start_buffer_frames = 3072;
};

struct SdlAudioMetrics {
    std::uint64_t callback_count = 0;
    std::uint64_t callback_frames = 0;
    std::uint64_t underrun_frames = 0;
    std::uint64_t rejected_frames = 0;
    std::size_t queued_frames = 0;
};

class SdlAudio {
public:
    SdlAudio() = default;
    ~SdlAudio();

    SdlAudio(const SdlAudio&) = delete;
    SdlAudio& operator=(const SdlAudio&) = delete;

    bool init(audio::PcmRing& ring, const SdlAudioConfig& config = {});
    void shutdown();
    void pause(bool paused);
    bool start_if_prebuffered();
    bool active() const;
    bool playing() const;
    int sample_rate() const;
    int channels() const;
    int callback_buffer_frames() const;
    int start_buffer_frames() const;
    std::string driver_name() const;
    SdlAudioMetrics metrics() const;

private:
    static void sdl_audio_callback(void* userdata, Uint8* stream, int bytes);
    void consume_audio(Uint8* stream, int bytes);

    SDL_AudioDeviceID device_id_ = 0;
    audio::PcmRing* ring_ = nullptr;
    int sample_rate_ = 48000;
    int channels_ = 2;
    int callback_buffer_frames_ = 0;
    int start_buffer_frames_ = 0;
    std::string driver_name_;
    std::atomic<std::uint64_t> callback_count_{0};
    std::atomic<std::uint64_t> callback_frames_{0};
    std::atomic<std::uint64_t> underrun_frames_{0};
    bool playback_started_ = false;
    bool initialized_ = false;
};

} // namespace czgba::platform
