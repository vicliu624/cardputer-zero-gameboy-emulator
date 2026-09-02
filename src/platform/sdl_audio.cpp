#include "platform/sdl_audio.hpp"

#include <SDL.h>

#include <algorithm>
#include <iostream>
#include <limits>
#include <string>

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

    // TDVP's SDL2 runtime recognizes this private opt-in hint and creates a
    // larger per-stream PulseAudio queue without changing the system ALSA
    // sink policy. Upstream SDL2 ignores unknown hints, so other platforms
    // retain their existing behavior.
    if (config.pulse_playback_buffer_frames > 0) {
        const auto requested_frames = std::to_string(std::max(
            config.pulse_playback_buffer_frames,
            config.device_buffer_frames));
        SDL_SetHint("SDL_AUDIO_PULSEAUDIO_BUFFER_FRAMES", requested_frames.c_str());
    }

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
    const int reserved_frames = std::min(queue_limit_frames, device_buffer_frames * 2);
    high_watermark_samples_ = frames_to_samples(std::max(
        start_buffer_frames,
        queue_limit_frames - reserved_frames));
    clear_pending_samples();
    samples_submitted_ = 0;
    samples_queued_ = 0;
    samples_dropped_ = 0;
    queue_failures_ = 0;
    playback_started_ = false;
    initialized_ = true;

    SDL_PauseAudioDevice(device_id_, 1);

    const char* driver = SDL_GetCurrentAudioDriver();
    std::cout << "SDL audio driver: " << (driver != nullptr ? driver : "unknown") << '\n';
    std::cout << "SDL audio: " << sample_rate_ << " Hz, " << channels_
              << " channels, queued audio limit " << queue_limit_samples_
              << " samples, high watermark " << high_watermark_samples_ << " samples";
    if (config.pulse_playback_buffer_frames > 0) {
        std::cout << ", PulseAudio stream buffer request "
                  << config.pulse_playback_buffer_frames << " frames";
    }
    std::cout << '\n';
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
    high_watermark_samples_ = 0;
    clear_pending_samples();
    samples_submitted_ = 0;
    samples_queued_ = 0;
    samples_dropped_ = 0;
    queue_failures_ = 0;
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
        clear_pending_samples();
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

void SdlAudio::service()
{
    if (!active()) {
        return;
    }

    const auto channels = static_cast<std::size_t>(std::max(1, channels_));
    while (pending_samples() > 0) {
        const auto writable = writable_samples();
        if (writable == 0) {
            break;
        }

        auto samples_to_write = std::min(pending_samples(), writable);
        samples_to_write -= samples_to_write % channels;
        if (samples_to_write == 0) {
            break;
        }

        const auto bytes_to_write = samples_to_write * sizeof(std::int16_t);
        if (bytes_to_write > static_cast<std::size_t>(std::numeric_limits<Uint32>::max())) {
            ++queue_failures_;
            std::cerr << "SDL audio queue fragment is too large\n";
            break;
        }

        if (SDL_QueueAudio(
                device_id_,
                pending_samples_.data() + pending_offset_,
                static_cast<Uint32>(bytes_to_write)) != 0) {
            ++queue_failures_;
            std::cerr << "SDL audio queue failed: " << SDL_GetError() << '\n';
            break;
        }

        pending_offset_ += samples_to_write;
        samples_queued_ += samples_to_write;
        start_playback_if_ready();
        compact_pending_samples();
    }
}

bool SdlAudio::should_throttle() const
{
    if (!active() || !playback_started_ || high_watermark_samples_ == 0) {
        return false;
    }
    return buffered_samples() + pending_samples() >= high_watermark_samples_;
}

std::size_t SdlAudio::write(std::span<const std::int16_t> samples)
{
    if (!active() || samples.empty()) {
        return 0;
    }

    samples_submitted_ += samples.size();
    pending_samples_.insert(pending_samples_.end(), samples.begin(), samples.end());
    service();
    return samples.size();
}

SdlAudioStats SdlAudio::stats() const
{
    return {
        samples_submitted_,
        samples_queued_,
        samples_dropped_,
        queue_failures_,
        buffered_samples(),
        pending_samples(),
        buffered_samples() + pending_samples(),
        should_throttle(),
        playback_started_,
    };
}

std::size_t SdlAudio::frames_to_samples(int frames) const
{
    return static_cast<std::size_t>(std::max(1, frames)) *
           static_cast<std::size_t>(std::max(1, channels_));
}

std::size_t SdlAudio::pending_samples() const
{
    return pending_offset_ < pending_samples_.size() ? pending_samples_.size() - pending_offset_ : 0;
}

void SdlAudio::clear_pending_samples()
{
    pending_samples_.clear();
    pending_offset_ = 0;
}

void SdlAudio::compact_pending_samples()
{
    if (pending_offset_ == pending_samples_.size()) {
        clear_pending_samples();
    } else if (pending_offset_ > pending_samples_.size() / 2) {
        pending_samples_.erase(
            pending_samples_.begin(),
            pending_samples_.begin() + static_cast<std::ptrdiff_t>(pending_offset_));
        pending_offset_ = 0;
    }
}

void SdlAudio::start_playback_if_ready()
{
    if (!playback_started_ && buffered_samples() >= start_buffer_samples_) {
        SDL_PauseAudioDevice(device_id_, 0);
        playback_started_ = true;
    }
}

} // namespace czgba::platform
