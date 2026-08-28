#include "platform/sdl_audio.hpp"

#include <SDL.h>

#include <algorithm>
#include <iostream>
#include <limits>

namespace czgba::platform {
namespace {

constexpr int kBytesPerSample = static_cast<int>(sizeof(std::int16_t));

std::size_t bytes_to_samples(Uint32 bytes)
{
    return static_cast<std::size_t>(bytes / static_cast<Uint32>(kBytesPerSample));
}

} // namespace

SdlAudio::~SdlAudio()
{
    shutdown();
}

bool SdlAudio::init(const SdlAudioConfig& config)
{
    shutdown();

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        std::cerr << "SDL audio init failed: " << SDL_GetError() << '\n';
        return false;
    }

    sample_rate_ = std::max(8000, config.sample_rate);
    channels_ = std::max(1, config.channels);

    const int device_buffer_frames = std::clamp(config.device_buffer_frames, 128, 8192);
    const int queue_limit_frames =
        std::max({config.buffer_limit_frames, config.start_buffer_frames, device_buffer_frames * 4});
    const int start_buffer_frames =
        std::clamp(config.start_buffer_frames, device_buffer_frames, queue_limit_frames);

    SDL_AudioSpec desired{};
    desired.freq = sample_rate_;
    desired.format = AUDIO_S16SYS;
    desired.channels = static_cast<Uint8>(channels_);
    desired.samples = static_cast<Uint16>(device_buffer_frames);
    desired.callback = nullptr;

    SDL_AudioSpec obtained{};
    device_id_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (device_id_ == 0) {
        std::cerr << "SDL audio open failed: " << SDL_GetError() << '\n';
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    sample_rate_ = obtained.freq;
    channels_ = obtained.channels;
    queue_limit_samples_ = frames_to_samples(queue_limit_frames);
    start_buffer_samples_ = frames_to_samples(start_buffer_frames);
    playback_started_ = false;
    initialized_ = true;

    SDL_PauseAudioDevice(device_id_, 1);

    const char* driver = SDL_GetCurrentAudioDriver();
    std::cout << "SDL audio driver: " << (driver != nullptr ? driver : "unknown") << '\n';
    std::cout << "SDL audio: " << sample_rate_ << " Hz, " << channels_
              << " channels, queued audio limit " << queue_limit_samples_ << " samples\n";
    return true;
}

void SdlAudio::shutdown()
{
    if (device_id_ != 0) {
        SDL_PauseAudioDevice(device_id_, 1);
        SDL_ClearQueuedAudio(device_id_);
        SDL_CloseAudioDevice(device_id_);
        device_id_ = 0;
    }

    if (initialized_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        initialized_ = false;
    }

    queue_limit_samples_ = 0;
    start_buffer_samples_ = 0;
    playback_started_ = false;
}

void SdlAudio::pause(bool paused)
{
    if (device_id_ == 0) {
        return;
    }

    SDL_PauseAudioDevice(device_id_, paused ? 1 : 0);
    if (paused) {
        SDL_ClearQueuedAudio(device_id_);
    }
    playback_started_ = !paused;
}

bool SdlAudio::active() const
{
    return initialized_ && device_id_ != 0;
}

int SdlAudio::sample_rate() const
{
    return sample_rate_;
}

std::size_t SdlAudio::buffered_samples() const
{
    if (!active()) {
        return 0;
    }

    return bytes_to_samples(SDL_GetQueuedAudioSize(device_id_));
}

std::size_t SdlAudio::writable_samples() const
{
    if (!active() || queue_limit_samples_ == 0) {
        return 0;
    }

    const auto buffered = buffered_samples();
    if (buffered >= queue_limit_samples_) {
        return 0;
    }
    return queue_limit_samples_ - buffered;
}

std::size_t SdlAudio::write(std::span<const std::int16_t> samples)
{
    if (!active() || samples.empty()) {
        return 0;
    }

    const auto writable = writable_samples();
    if (writable == 0) {
        return 0;
    }

    auto samples_to_write = std::min(samples.size(), writable);
    samples_to_write -= samples_to_write % static_cast<std::size_t>(std::max(1, channels_));
    if (samples_to_write == 0) {
        return 0;
    }
    const auto bytes_to_write = samples_to_write * sizeof(std::int16_t);
    if (bytes_to_write > static_cast<std::size_t>(std::numeric_limits<Uint32>::max())) {
        return 0;
    }

    if (SDL_QueueAudio(
            device_id_,
            samples.data(),
            static_cast<Uint32>(bytes_to_write)) != 0) {
        std::cerr << "SDL audio queue failed: " << SDL_GetError() << '\n';
        return 0;
    }

    if (!playback_started_ && buffered_samples() >= start_buffer_samples_) {
        SDL_PauseAudioDevice(device_id_, 0);
        playback_started_ = true;
    }

    return samples_to_write;
}

std::size_t SdlAudio::frames_to_samples(int frames) const
{
    return static_cast<std::size_t>(std::max(1, frames)) *
           static_cast<std::size_t>(std::max(1, channels_));
}

} // namespace czgba::platform
