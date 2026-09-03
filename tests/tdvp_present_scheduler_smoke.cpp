#include <cstdlib>
#include <iostream>

#include "platform/tdvp_k230_present_scheduler.hpp"

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "TDVP present scheduler smoke failed: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main()
{
    using czgba::platform::TdvpK230PresentDecision;
    using czgba::platform::TdvpK230PresentScheduler;

    TdvpK230PresentScheduler scheduler;
    require(scheduler.try_begin_present(true) == TdvpK230PresentDecision::NoFrame,
        "an idle presenter does not commit");

    scheduler.note_frame_available();
    require(scheduler.try_begin_present(true) == TdvpK230PresentDecision::Commit,
        "the first ready frame commits with a free buffer");
    require(scheduler.stats().present_committed == 0,
        "reserving a buffer is not counted as a submitted surface commit");
    scheduler.note_present_committed();

    scheduler.note_frame_available();
    require(scheduler.try_begin_present(false) == TdvpK230PresentDecision::NoFreeBuffer,
        "a busy compositor buffer never blocks the emulator loop");
    require(scheduler.has_latest_frame(),
        "buffer pressure retains only the newest frame");

    scheduler.note_buffer_release();
    require(scheduler.try_begin_present(true) == TdvpK230PresentDecision::Commit,
        "a released buffer permits the latest frame to commit");
    scheduler.note_present_committed();

    scheduler.note_frame_available();
    require(scheduler.try_begin_present(true) == TdvpK230PresentDecision::Commit,
        "a later frame can reserve another released buffer");
    scheduler.retry_latest_frame();
    require(scheduler.has_latest_frame(),
        "a failed buffer preparation restores the newest frame for retry");
    require(scheduler.stats().present_committed == 2,
        "a retried reservation is never reported as a committed frame");

    const auto& stats = scheduler.stats();
    require(stats.present_requested == 3, "each produced video frame is counted");
    require(stats.present_committed == 2, "only allowed commits are counted");
    require(stats.present_skipped_no_buffer == 1, "busy buffers are observable without blocking");
    require(stats.buffer_releases == 1, "buffer-release telemetry is retained");
    return 0;
}
