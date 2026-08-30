#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace czgba::audio {

struct PcmRingWatermarks {
    std::size_t queued_samples = 0;
    std::size_t low_queued_samples = 0;
    std::size_t high_queued_samples = 0;
};

// A preallocated single-producer/single-consumer PCM queue. The emulation
// worker is the only producer and the SDL callback is the only consumer.
// Values are interleaved signed 16-bit samples rather than byte streams so
// the queue cannot accidentally split an audio sample.
class PcmRing {
public:
    explicit PcmRing(std::size_t capacity_samples)
        : samples_(std::max<std::size_t>(2, capacity_samples - (capacity_samples % 2)), 0)
    {
    }

    PcmRing(const PcmRing&) = delete;
    PcmRing& operator=(const PcmRing&) = delete;

    bool write_all(std::span<const std::int16_t> samples)
    {
        if (samples.empty()) {
            return true;
        }
        if ((samples.size() % 2) != 0 || samples.size() > capacity_samples()) {
            rejected_samples_.fetch_add(samples.size(), std::memory_order_relaxed);
            return false;
        }

        const auto write = write_index_.load(std::memory_order_relaxed);
        const auto read = read_index_.load(std::memory_order_acquire);
        const auto used = write - read;
        if (samples.size() > capacity_samples() - used) {
            rejected_samples_.fetch_add(samples.size(), std::memory_order_relaxed);
            return false;
        }

        copy_into_ring(write, samples);
        const auto new_write = write + samples.size();
        write_index_.store(new_write, std::memory_order_release);
        note_watermark(static_cast<std::size_t>(new_write - read));
        return true;
    }

    std::size_t read_some(std::span<std::int16_t> destination)
    {
        if (destination.empty()) {
            return 0;
        }

        const auto read = read_index_.load(std::memory_order_relaxed);
        const auto write = write_index_.load(std::memory_order_acquire);
        const auto count = std::min<std::size_t>(destination.size(), write - read);
        copy_from_ring(read, destination.first(count));
        const auto new_read = read + count;
        read_index_.store(new_read, std::memory_order_release);
        note_watermark(static_cast<std::size_t>(write - new_read));
        return count;
    }

    std::size_t queued_samples() const
    {
        const auto write = write_index_.load(std::memory_order_acquire);
        const auto read = read_index_.load(std::memory_order_acquire);
        return static_cast<std::size_t>(write - read);
    }

    std::size_t writable_samples() const
    {
        return capacity_samples() - queued_samples();
    }

    std::size_t queued_frames() const
    {
        return queued_samples() / 2;
    }

    std::size_t writable_frames() const
    {
        return writable_samples() / 2;
    }

    std::size_t capacity_samples() const
    {
        return samples_.size();
    }

    std::size_t capacity_frames() const
    {
        return capacity_samples() / 2;
    }

    std::uint64_t rejected_samples() const
    {
        return rejected_samples_.load(std::memory_order_relaxed);
    }

    PcmRingWatermarks watermarks() const
    {
        return {
            queued_samples(),
            static_cast<std::size_t>(low_queued_samples_.load(std::memory_order_relaxed)),
            static_cast<std::size_t>(high_queued_samples_.load(std::memory_order_relaxed)),
        };
    }

    // Begin a new observable audio epoch after prebuffering. Startup and
    // recovery deliberately do not make an initial empty queue look like a
    // realtime low-water event in the telemetry.
    void reset_watermarks()
    {
        const auto queued = queued_samples();
        low_queued_samples_.store(queued, std::memory_order_relaxed);
        high_queued_samples_.store(queued, std::memory_order_relaxed);
    }

    // The producer calls this only while the audio device is paused. The
    // callback therefore cannot still be reading samples that are discarded.
    void discard_all_while_paused()
    {
        const auto write = write_index_.load(std::memory_order_acquire);
        read_index_.store(write, std::memory_order_release);
        low_queued_samples_.store(0, std::memory_order_relaxed);
        high_queued_samples_.store(0, std::memory_order_relaxed);
    }

private:
    void note_watermark(std::size_t queued_samples)
    {
        auto low = low_queued_samples_.load(std::memory_order_relaxed);
        while (queued_samples < low &&
               !low_queued_samples_.compare_exchange_weak(
                   low, queued_samples, std::memory_order_relaxed, std::memory_order_relaxed)) {
        }

        auto high = high_queued_samples_.load(std::memory_order_relaxed);
        while (queued_samples > high &&
               !high_queued_samples_.compare_exchange_weak(
                   high, queued_samples, std::memory_order_relaxed, std::memory_order_relaxed)) {
        }
    }

    void copy_into_ring(std::uint64_t write, std::span<const std::int16_t> samples)
    {
        const auto start = static_cast<std::size_t>(write % capacity_samples());
        const auto first = std::min(samples.size(), capacity_samples() - start);
        std::copy_n(samples.data(), first, samples_.data() + start);
        std::copy(samples.begin() + static_cast<std::ptrdiff_t>(first), samples.end(), samples_.begin());
    }

    void copy_from_ring(std::uint64_t read, std::span<std::int16_t> destination)
    {
        const auto start = static_cast<std::size_t>(read % capacity_samples());
        const auto first = std::min(destination.size(), capacity_samples() - start);
        std::copy_n(samples_.data() + start, first, destination.data());
        std::copy(samples_.begin(), samples_.begin() + static_cast<std::ptrdiff_t>(destination.size() - first),
                  destination.begin() + static_cast<std::ptrdiff_t>(first));
    }

    std::vector<std::int16_t> samples_;
    alignas(64) std::atomic<std::uint64_t> read_index_{0};
    alignas(64) std::atomic<std::uint64_t> write_index_{0};
    std::atomic<std::uint64_t> rejected_samples_{0};
    std::atomic<std::uint64_t> low_queued_samples_{0};
    std::atomic<std::uint64_t> high_queued_samples_{0};
};

} // namespace czgba::audio
