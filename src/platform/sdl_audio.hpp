#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <SDL.h>

namespace czgba::platform {

struct SdlAudioConfig {
    int sample_rate = 48000;
    int channels = 2;
    int device_buffer_frames = 1024;
    int buffer_limit_frames = 8192;
    int start_buffer_frames = 4096;
    // Zero keeps upstream SDL's PulseAudio buffering policy. A positive value
    // is an opt-in request understood by TDVP's patched SDL2 runtime; it is
    // deliberately independent of the SDL callback/queue fragment size.
    int pulse_playback_buffer_frames = 0;
};

struct SdlAudioStats {
    std::uint64_t samples_submitted = 0;
    std::uint64_t samples_queued = 0;
    std::uint64_t samples_dropped = 0;
    std::uint64_t queue_failures = 0;
    std::size_t queued_samples = 0;
    std::size_t pending_samples = 0;
    std::size_t total_buffered_samples = 0;
    bool high_watermark_reached = false;
    bool playback_started = false;
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
    // Flush any PCM retained while SDL's device queue was full. This never
    // discards a partial emulation frame; callers can use should_throttle()
    // to keep the producer from outrunning the playback device.
    void service();
    bool should_throttle() const;
    std::size_t write(std::span<const std::int16_t> samples);
    SdlAudioStats stats() const;

private:
    std::size_t frames_to_samples(int frames) const;
    std::size_t pending_samples() const;
    void clear_pending_samples();
    void compact_pending_samples();
    void start_playback_if_ready();

    SDL_AudioDeviceID device_id_ = 0;
    int sample_rate_ = 48000;
    int channels_ = 2;
    std::size_t queue_limit_samples_ = 0;
    std::size_t start_buffer_samples_ = 0;
    std::size_t high_watermark_samples_ = 0;
    std::vector<std::int16_t> pending_samples_;
    std::size_t pending_offset_ = 0;
    std::uint64_t samples_submitted_ = 0;
    std::uint64_t samples_queued_ = 0;
    std::uint64_t samples_dropped_ = 0;
    std::uint64_t queue_failures_ = 0;
    bool playback_started_ = false;
    bool initialized_ = false;
};

} // namespace czgba::platform
