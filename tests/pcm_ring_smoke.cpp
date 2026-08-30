#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

#include "audio/pcm_ring.hpp"

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "pcm ring smoke failed: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main()
{
    using czgba::audio::PcmRing;

    PcmRing ring(8);
    const std::vector<std::int16_t> first{1, 2, 3, 4, 5, 6};
    require(ring.write_all(first), "accepts a complete interleaved batch");
    require(ring.queued_samples() == first.size(), "counts queued samples");
    require(!ring.write_all(std::vector<std::int16_t>{7, 8, 9, 10}), "rejects a batch instead of short-writing it");
    require(ring.rejected_samples() == 4, "counts rejected samples");

    std::vector<std::int16_t> first_out(4);
    require(ring.read_some(first_out) == first_out.size(), "reads the requested prefix");
    require(first_out == std::vector<std::int16_t>({1, 2, 3, 4}), "preserves sample order");
    require(ring.write_all(std::vector<std::int16_t>{7, 8, 9, 10}), "wraps without a partial write");
    std::vector<std::int16_t> rest(6);
    require(ring.read_some(rest) == rest.size(), "reads across the wrap point");
    require(rest == std::vector<std::int16_t>({5, 6, 7, 8, 9, 10}), "preserves wrapped order");

    require(ring.write_all(std::vector<std::int16_t>{11, 12, 13, 14}), "accepts a batch for watermark testing");
    ring.reset_watermarks();
    std::int16_t watermark_read[2]{};
    require(ring.read_some(watermark_read) == 2, "reads a watermark test frame");
    const auto watermarks = ring.watermarks();
    require(watermarks.low_queued_samples == 2, "records the callback-visible low water mark");
    require(watermarks.high_queued_samples == 4, "records the producer-visible high water mark");
    ring.discard_all_while_paused();
    const auto reset_watermarks = ring.watermarks();
    require(reset_watermarks.queued_samples == 0 && reset_watermarks.low_queued_samples == 0 &&
                reset_watermarks.high_queued_samples == 0,
            "clearing a paused epoch clears its watermarks");

    constexpr std::uint32_t kFrames = 100000;
    PcmRing concurrent_ring(2048);
    std::atomic<bool> producer_done{false};
    std::atomic<bool> mismatch{false};

    std::thread producer([&] {
        for (std::uint32_t frame = 0; frame < kFrames; ++frame) {
            const std::int16_t value = static_cast<std::int16_t>(frame & 0x7fffU);
            const std::int16_t pair[]{value, value};
            while (!concurrent_ring.write_all(pair)) {
                std::this_thread::yield();
            }
        }
        producer_done.store(true, std::memory_order_release);
    });

    std::thread consumer([&] {
        std::uint32_t expected = 0;
        std::int16_t pair[2]{};
        while (!producer_done.load(std::memory_order_acquire) || concurrent_ring.queued_samples() != 0) {
            if (concurrent_ring.read_some(pair) != 2) {
                std::this_thread::yield();
                continue;
            }
            const auto value = static_cast<std::int16_t>(expected & 0x7fffU);
            if (pair[0] != value || pair[1] != value) {
                mismatch.store(true, std::memory_order_release);
                return;
            }
            ++expected;
        }
        if (expected != kFrames) {
            mismatch.store(true, std::memory_order_release);
        }
    });

    producer.join();
    consumer.join();
    require(!mismatch.load(std::memory_order_acquire), "preserves order under concurrent producer/consumer load");
    return 0;
}
