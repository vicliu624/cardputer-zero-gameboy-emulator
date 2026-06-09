#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include <SDL.h>

namespace czgba::platform {

struct SdlAudioConfig {
    int sample_rate = 48000;
    int channels = 2;
    int device_buffer_frames = 1024;
    int buffer_limit_frames = 8192;
    int start_buffer_frames = 4096;
};

class SdlAudio {
public:
    SdlAudio() = default;
    ~SdlAudio();

    SdlAudio(const SdlAudio&) = delete;
    SdlAudio& operator=(const SdlAudio&) = delete;

    bool init(const SdlAudioConfig& config = {});
    void shutdown();
    void pause(bool paused);
    bool active() const;
    int sample_rate() const;
    std::size_t buffered_samples() const;
    std::size_t writable_samples() const;
    std::size_t write(std::span<const std::int16_t> samples);

private:
    std::size_t frames_to_samples(int frames) const;

    SDL_AudioDeviceID device_id_ = 0;
    int sample_rate_ = 48000;
    int channels_ = 2;
    std::size_t queue_limit_samples_ = 0;
    std::size_t start_buffer_samples_ = 0;
    bool playback_started_ = false;
    bool initialized_ = false;
};

} // namespace czgba::platform
