#pragma once

#include <array>
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
    // Zero keeps SDL2's upstream PulseAudio buffering policy. Positive values
    // request an explicit per-stream playback queue from TDVP's SDL2 runtime.
    // This is deliberately separate from the callback size: a small callback
    // remains useful for the emulation clock while the server-side queue
    // absorbs scheduling jitter before it reaches the ALSA sink.
    int pulse_playback_buffer_frames = 0;
};

enum class SdlAudioState {
    Closed,
    Prebuffering,
    Playing,
    Unavailable,
};

const char* sdl_audio_state_name(SdlAudioState state);

struct SdlAudioMetrics {
    std::uint64_t callback_count = 0;
    std::uint64_t callback_frames = 0;
    std::uint64_t underrun_frames = 0;
    std::uint64_t rejected_frames = 0;
    std::size_t queued_frames = 0;
    std::size_t low_queued_frames = 0;
    std::size_t high_queued_frames = 0;
    std::uint64_t recovery_count = 0;
    std::uint64_t reopen_count = 0;
    std::uint32_t callback_jitter_p50_us = 0;
    std::uint32_t callback_jitter_p95_us = 0;
    std::uint32_t callback_jitter_p99_us = 0;
};

class SdlAudio {
public:
    SdlAudio() = default;
    ~SdlAudio();

    SdlAudio(const SdlAudio&) = delete;
    SdlAudio& operator=(const SdlAudio&) = delete;

    bool init(audio::PcmRing& ring, const SdlAudioConfig& config = {});
    void shutdown();
    // Enter a new, silent media epoch. This is used for pause, underrun
    // recovery, and device replacement; every resume must pass through the
    // normal complete-prebuffer gate rather than toggling a boolean back on.
    void begin_prebuffering();
    void recover_from_underrun();
    bool reopen();
    bool start_if_prebuffered();
    bool active() const;
    bool playing() const;
    SdlAudioState state() const;
    int sample_rate() const;
    int channels() const;
    int callback_buffer_frames() const;
    int start_buffer_frames() const;
    std::string driver_name() const;
    SdlAudioMetrics metrics() const;

private:
    static constexpr std::size_t kJitterBucketWidthUs = 100;
    static constexpr std::size_t kJitterBucketCount = 257;

    static void sdl_audio_callback(void* userdata, Uint8* stream, int bytes);
    void consume_audio(Uint8* stream, int bytes);
    void reset_callback_jitter();
    void record_callback_timing();
    std::uint32_t callback_jitter_quantile_us(std::uint32_t permille) const;

    SDL_AudioDeviceID device_id_ = 0;
    audio::PcmRing* ring_ = nullptr;
    // Retained even while a device is unavailable so the UI thread can retry
    // PulseAudio/ALSA recovery without reconstructing the emulator process.
    audio::PcmRing* configured_ring_ = nullptr;
    SdlAudioConfig config_;
    int sample_rate_ = 48000;
    int channels_ = 2;
    int callback_buffer_frames_ = 0;
    int start_buffer_frames_ = 0;
    std::string driver_name_;
    std::atomic<std::uint64_t> callback_count_{0};
    std::atomic<std::uint64_t> callback_frames_{0};
    std::atomic<std::uint64_t> underrun_frames_{0};
    std::atomic<std::uint64_t> recovery_count_{0};
    std::atomic<std::uint64_t> reopen_count_{0};
    std::atomic<std::int64_t> last_callback_time_us_{0};
    std::array<std::atomic<std::uint64_t>, kJitterBucketCount> callback_jitter_buckets_{};
    std::int64_t expected_callback_period_us_ = 0;
    SdlAudioState state_ = SdlAudioState::Closed;
    bool initialized_ = false;
};

} // namespace czgba::platform
