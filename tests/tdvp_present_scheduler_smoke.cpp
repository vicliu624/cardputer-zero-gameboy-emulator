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
    require(scheduler.frame_callback_pending(),
        "a successful commit owns exactly one frame callback");
    require(scheduler.stats().present_committed == 0,
        "reserving a callback is not counted as a submitted surface commit");
    scheduler.note_present_committed();

    scheduler.note_frame_available();
    require(scheduler.try_begin_present(true) == TdvpK230PresentDecision::WaitForFrameCallback,
        "a newer frame replaces the pending video frame instead of committing early");
    require(scheduler.has_latest_frame(),
        "the newest frame remains available while the callback is outstanding");

    scheduler.note_frame_callback();
    require(scheduler.try_begin_present(false) == TdvpK230PresentDecision::NoFreeBuffer,
        "a busy compositor buffer never blocks the emulator loop");
    require(scheduler.has_latest_frame(),
        "buffer pressure retains only the newest frame");

    scheduler.note_buffer_release();
    require(scheduler.try_begin_present(true) == TdvpK230PresentDecision::Commit,
        "a released buffer permits the latest frame to commit");
    scheduler.note_present_committed();

    scheduler.note_frame_callback();
    scheduler.note_frame_available();
    require(scheduler.try_begin_present(true) == TdvpK230PresentDecision::Commit,
        "a later frame can reserve a callback");
    scheduler.cancel_pending_present();
    require(!scheduler.frame_callback_pending() && scheduler.has_latest_frame(),
        "a failed callback allocation restores the newest frame for retry");
    require(scheduler.stats().present_committed == 2,
        "a cancelled reservation is never reported as a committed frame");

    const auto& stats = scheduler.stats();
    require(stats.present_requested == 3, "each produced video frame is counted");
    require(stats.present_committed == 2, "only allowed commits are counted");
    require(stats.present_skipped_frame_callback == 1, "callback pacing skips an intermediate frame");
    require(stats.present_skipped_no_buffer == 1, "busy buffers are observable without blocking");
    require(stats.frame_callbacks == 2 && stats.buffer_releases == 1,
        "callback and release telemetry is retained");
    return 0;
}
