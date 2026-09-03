#pragma once

#include <cstdint>

namespace czgba::platform {

enum class TdvpK230PresentDecision {
    NoFrame,
    NoFreeBuffer,
    Commit,
};

struct TdvpK230PresentStats {
    std::uint64_t present_requested = 0;
    std::uint64_t present_committed = 0;
    std::uint64_t present_skipped_no_buffer = 0;
    std::uint64_t buffer_releases = 0;
};

// Video presentation deliberately has a different policy than emulation. The
// main loop may keep producing the newest GBA frame at 59.7275 Hz, but this
// scheduler never waits for a compositor-owned wl_buffer or for a
// wl_surface.frame callback.  The latter is a presentation hint, not a buffer
// lifetime guarantee, and the K230 VGLite compositor can deliver it at a much
// lower and irregular rate.  A wl_buffer becomes writable only after its
// release event, which is the actual safety boundary for shared memory.
// If the compositor falls behind, outdated video frames are replaced by the
// newest one instead of delaying emulation, audio, or input.
class TdvpK230PresentScheduler {
public:
    void note_frame_available()
    {
        latest_frame_available_ = true;
        ++stats_.present_requested;
    }

    TdvpK230PresentDecision try_begin_present(bool has_free_buffer)
    {
        if (!latest_frame_available_) {
            return TdvpK230PresentDecision::NoFrame;
        }
        if (!has_free_buffer) {
            ++stats_.present_skipped_no_buffer;
            return TdvpK230PresentDecision::NoFreeBuffer;
        }

        latest_frame_available_ = false;
        return TdvpK230PresentDecision::Commit;
    }

    // Call only after wl_surface_commit(). Reserving a presentation and
    // submitting one are intentionally distinct so validation failures can
    // retain the newest frame for the next free wl_buffer.
    void note_present_committed()
    {
        ++stats_.present_committed;
    }

    void retry_latest_frame()
    {
        latest_frame_available_ = true;
    }

    void note_buffer_release()
    {
        ++stats_.buffer_releases;
    }

    bool has_latest_frame() const
    {
        return latest_frame_available_;
    }

    const TdvpK230PresentStats& stats() const
    {
        return stats_;
    }

private:
    TdvpK230PresentStats stats_;
    bool latest_frame_available_ = false;
};

} // namespace czgba::platform
