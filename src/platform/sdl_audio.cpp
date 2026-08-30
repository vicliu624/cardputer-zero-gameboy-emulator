#include "platform/sdl_audio.hpp"

#include <SDL.h>

#include <algorithm>
#include <cstring>
#include <iostream>

namespace czgba::platform {
namespace {

} // namespace

SdlAudio::~SdlAudio()
{
    shutdown();
}

bool SdlAudio::init(audio::PcmRing& ring, const SdlAudioConfig& config)
{
    shutdown();

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        std::cerr << "SDL audio init failed: " << SDL_GetError() << '\n';
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
    playback_started_ = false;
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
    playback_started_ = false;
}

void SdlAudio::pause(bool paused)
{
    if (device_id_ == 0) {
        return;
    }

    SDL_PauseAudioDevice(device_id_, paused ? 1 : 0);
    if (paused) {
        SDL_LockAudioDevice(device_id_);
        if (ring_ != nullptr) {
            ring_->discard_all_while_paused();
        }
        // callback_frames is the audio media-clock position, not a lifetime
        // diagnostic. A new prebuffer starts a new A/V epoch.
        callback_frames_.store(0, std::memory_order_relaxed);
        SDL_UnlockAudioDevice(device_id_);
    }
    playback_started_ = !paused;
}

bool SdlAudio::start_if_prebuffered()
{
    if (!active() || playback_started_ || ring_ == nullptr || ring_->queued_frames() < static_cast<std::size_t>(start_buffer_frames_)) {
        return playback_started_;
    }

    // Make the media epoch visible before the device can enter the callback.
    SDL_LockAudioDevice(device_id_);
    callback_frames_.store(0, std::memory_order_relaxed);
    SDL_UnlockAudioDevice(device_id_);
    SDL_PauseAudioDevice(device_id_, 0);
    playback_started_ = true;
    return true;
}

bool SdlAudio::active() const
{
    return initialized_ && device_id_ != 0;
}

bool SdlAudio::playing() const
{
    return active() && playback_started_;
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
    result.queued_frames = ring_ != nullptr ? ring_->queued_frames() : 0;
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

} // namespace czgba::platform
