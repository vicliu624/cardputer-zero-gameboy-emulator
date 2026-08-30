#include "platform/sdl_audio.hpp"

#include <SDL.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <limits>

namespace czgba::platform {

const char* sdl_audio_state_name(SdlAudioState state)
{
    switch (state) {
    case SdlAudioState::Prebuffering:
        return "prebuffering";
    case SdlAudioState::Playing:
        return "playing";
    case SdlAudioState::Unavailable:
        return "unavailable";
    case SdlAudioState::Closed:
    default:
        return "closed";
    }
}

SdlAudio::~SdlAudio()
{
    shutdown();
}

bool SdlAudio::init(audio::PcmRing& ring, const SdlAudioConfig& config)
{
    shutdown();

    configured_ring_ = &ring;
    config_ = config;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        std::cerr << "SDL audio init failed: " << SDL_GetError() << '\n';
        state_ = SdlAudioState::Unavailable;
        return false;
    }

    sample_rate_ = std::max(8000, config.sample_rate);
    channels_ = std::max(1, config.channels);

    const int callback_buffer_frames = std::clamp(config.callback_buffer_frames, 128, 2048);
    const int start_buffer_frames = std::clamp(
        config.start_buffer_frames,
        callback_buffer_frames,
        static_cast<int>(ring.capacity_frames()));

    SDL_AudioSpec desired{};
    desired.freq = sample_rate_;
    desired.format = AUDIO_S16SYS;
    desired.channels = static_cast<Uint8>(channels_);
    desired.samples = static_cast<Uint16>(callback_buffer_frames);
    desired.callback = &SdlAudio::sdl_audio_callback;
    desired.userdata = this;

    SDL_AudioSpec obtained{};
    device_id_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (device_id_ == 0) {
        std::cerr << "SDL audio open failed: " << SDL_GetError() << '\n';
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        state_ = SdlAudioState::Unavailable;
        return false;
    }

    sample_rate_ = obtained.freq;
    channels_ = obtained.channels;
    callback_buffer_frames_ = static_cast<int>(obtained.samples);
    start_buffer_frames_ = start_buffer_frames;
    ring_ = &ring;
    callback_count_.store(0, std::memory_order_relaxed);
    callback_frames_.store(0, std::memory_order_relaxed);
    underrun_frames_.store(0, std::memory_order_relaxed);
    reset_callback_jitter();
    expected_callback_period_us_ = std::max<std::int64_t>(
        1,
        (static_cast<std::int64_t>(callback_buffer_frames_) * 1000000LL) /
            std::max(1, sample_rate_));
    state_ = SdlAudioState::Prebuffering;
    initialized_ = true;

    SDL_PauseAudioDevice(device_id_, 1);

    const char* driver = SDL_GetCurrentAudioDriver();
    driver_name_ = driver != nullptr ? driver : "unknown";
    std::cout << "SDL audio driver: " << driver_name_ << '\n';
    std::cout << "SDL audio: " << sample_rate_ << " Hz, " << channels_
              << " channels, callback " << callback_buffer_frames_
              << " frames, prebuffer " << start_buffer_frames_ << " frames\n";
    return true;
}

void SdlAudio::shutdown()
{
    if (device_id_ != 0) {
        SDL_PauseAudioDevice(device_id_, 1);
        SDL_CloseAudioDevice(device_id_);
        device_id_ = 0;
    }

    if (initialized_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        initialized_ = false;
    }

    ring_ = nullptr;
    callback_buffer_frames_ = 0;
    start_buffer_frames_ = 0;
    driver_name_.clear();
    expected_callback_period_us_ = 0;
    state_ = SdlAudioState::Closed;
}

void SdlAudio::begin_prebuffering()
{
    if (device_id_ == 0) {
        return;
    }

    SDL_PauseAudioDevice(device_id_, 1);
    SDL_LockAudioDevice(device_id_);
    // callback_frames is the audio media-clock position, not a lifetime
    // diagnostic. The emulation worker is the only SPSC producer and clears
    // its own queue after the UI has stopped this callback; the UI must not
    // become a second ring consumer by changing its read index here.
    callback_frames_.store(0, std::memory_order_relaxed);
    reset_callback_jitter();
    SDL_UnlockAudioDevice(device_id_);
    state_ = SdlAudioState::Prebuffering;
}

void SdlAudio::recover_from_underrun()
{
    recovery_count_.fetch_add(1, std::memory_order_relaxed);
    begin_prebuffering();
}

bool SdlAudio::reopen()
{
    auto* ring = configured_ring_;
    const auto config = config_;
    if (ring == nullptr) {
        state_ = SdlAudioState::Unavailable;
        return false;
    }

    // A PulseAudio restart can surface as an SDL device removal. Recreate the
    // SDL device rather than continuing to fill a callback that has lost its
    // real sink. init() always opens paused, so the caller can atomically
    // reset the emulation clock before audio resumes.
    shutdown();
    reopen_count_.fetch_add(1, std::memory_order_relaxed);
    return init(*ring, config);
}

bool SdlAudio::start_if_prebuffered()
{
    if (!active() || state_ == SdlAudioState::Playing || ring_ == nullptr ||
        ring_->queued_frames() < static_cast<std::size_t>(start_buffer_frames_)) {
        return state_ == SdlAudioState::Playing;
    }

    // Make the media epoch visible before the device can enter the callback.
    SDL_LockAudioDevice(device_id_);
    callback_frames_.store(0, std::memory_order_relaxed);
    reset_callback_jitter();
    ring_->reset_watermarks();
    SDL_UnlockAudioDevice(device_id_);
    SDL_PauseAudioDevice(device_id_, 0);
    state_ = SdlAudioState::Playing;
    return true;
}

bool SdlAudio::active() const
{
    return initialized_ && device_id_ != 0;
}

bool SdlAudio::playing() const
{
    return active() && state_ == SdlAudioState::Playing;
}

SdlAudioState SdlAudio::state() const
{
    return state_;
}

int SdlAudio::sample_rate() const
{
    return sample_rate_;
}

int SdlAudio::channels() const
{
    return channels_;
}

int SdlAudio::callback_buffer_frames() const
{
    return callback_buffer_frames_;
}

int SdlAudio::start_buffer_frames() const
{
    return start_buffer_frames_;
}

std::string SdlAudio::driver_name() const
{
    return driver_name_;
}

SdlAudioMetrics SdlAudio::metrics() const
{
    SdlAudioMetrics result;
    result.callback_count = callback_count_.load(std::memory_order_relaxed);
    result.callback_frames = callback_frames_.load(std::memory_order_relaxed);
    result.underrun_frames = underrun_frames_.load(std::memory_order_relaxed);
    result.rejected_frames = ring_ != nullptr ? ring_->rejected_samples() / static_cast<std::uint64_t>(std::max(1, channels_)) : 0;
    if (ring_ != nullptr) {
        const auto watermarks = ring_->watermarks();
        result.queued_frames = watermarks.queued_samples / static_cast<std::size_t>(std::max(1, channels_));
        result.low_queued_frames = watermarks.low_queued_samples / static_cast<std::size_t>(std::max(1, channels_));
        result.high_queued_frames = watermarks.high_queued_samples / static_cast<std::size_t>(std::max(1, channels_));
    }
    result.recovery_count = recovery_count_.load(std::memory_order_relaxed);
    result.reopen_count = reopen_count_.load(std::memory_order_relaxed);
    result.callback_jitter_p50_us = callback_jitter_quantile_us(500);
    result.callback_jitter_p95_us = callback_jitter_quantile_us(950);
    result.callback_jitter_p99_us = callback_jitter_quantile_us(990);
    return result;
}

void SdlAudio::sdl_audio_callback(void* userdata, Uint8* stream, int bytes)
{
    static_cast<SdlAudio*>(userdata)->consume_audio(stream, bytes);
}

void SdlAudio::consume_audio(Uint8* stream, int bytes)
{
    if (stream == nullptr || bytes <= 0) {
        return;
    }

    record_callback_timing();
    const auto requested_samples = static_cast<std::size_t>(bytes) / sizeof(std::int16_t);
    auto* destination = reinterpret_cast<std::int16_t*>(stream);
    const auto copied = ring_ != nullptr
        ? ring_->read_some(std::span<std::int16_t>(destination, requested_samples))
        : 0;
    if (copied < requested_samples) {
        std::memset(destination + copied, 0, (requested_samples - copied) * sizeof(std::int16_t));
        underrun_frames_.fetch_add(
            (requested_samples - copied) / static_cast<std::size_t>(std::max(1, channels_)),
            std::memory_order_relaxed);
    }
    callback_count_.fetch_add(1, std::memory_order_relaxed);
    callback_frames_.fetch_add(
        requested_samples / static_cast<std::size_t>(std::max(1, channels_)),
        std::memory_order_relaxed);
}

void SdlAudio::reset_callback_jitter()
{
    last_callback_time_us_.store(0, std::memory_order_relaxed);
    for (auto& bucket : callback_jitter_buckets_) {
        bucket.store(0, std::memory_order_relaxed);
    }
}

void SdlAudio::record_callback_timing()
{
    const auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    const auto previous = last_callback_time_us_.exchange(now, std::memory_order_relaxed);
    if (previous == 0 || expected_callback_period_us_ <= 0) {
        return;
    }

    const auto actual_period = now - previous;
    const auto jitter = actual_period >= expected_callback_period_us_
        ? actual_period - expected_callback_period_us_
        : expected_callback_period_us_ - actual_period;
    const auto bucket = std::min<std::size_t>(
        kJitterBucketCount - 1,
        static_cast<std::size_t>(jitter) / kJitterBucketWidthUs);
    callback_jitter_buckets_[bucket].fetch_add(1, std::memory_order_relaxed);
}

std::uint32_t SdlAudio::callback_jitter_quantile_us(std::uint32_t permille) const
{
    std::uint64_t total = 0;
    for (const auto& bucket : callback_jitter_buckets_) {
        total += bucket.load(std::memory_order_relaxed);
    }
    if (total == 0) {
        return 0;
    }

    const auto rank = (total * std::min<std::uint32_t>(1000, permille) + 999) / 1000;
    std::uint64_t seen = 0;
    for (std::size_t index = 0; index < callback_jitter_buckets_.size(); ++index) {
        seen += callback_jitter_buckets_[index].load(std::memory_order_relaxed);
        if (seen >= rank) {
            return static_cast<std::uint32_t>(index * kJitterBucketWidthUs);
        }
    }
    return std::numeric_limits<std::uint32_t>::max();
}

} // namespace czgba::platform
