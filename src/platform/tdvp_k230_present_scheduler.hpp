#pragma once

#include <cstdint>

namespace czgba::platform {

enum class TdvpK230PresentDecision {
    NoFrame,
    WaitForFrameCallback,
    NoFreeBuffer,
    Commit,
};

struct TdvpK230PresentStats {
    std::uint64_t present_requested = 0;
    std::uint64_t present_committed = 0;
    std::uint64_t present_skipped_frame_callback = 0;
    std::uint64_t present_skipped_no_buffer = 0;
    std::uint64_t frame_callbacks = 0;
    std::uint64_t buffer_releases = 0;
};

// Video presentation deliberately has a different policy than emulation. The
// main loop may keep producing the newest GBA frame at 59.7275 Hz, but this
// scheduler permits at most one outstanding Wayland frame callback and never
// waits for a compositor-owned wl_buffer. If the compositor falls behind,
// outdated video frames are replaced by the newest one instead of delaying
// emulation, audio, or input.
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
        if (frame_callback_pending_) {
            ++stats_.present_skipped_frame_callback;
            return TdvpK230PresentDecision::WaitForFrameCallback;
        }
        if (!has_free_buffer) {
            ++stats_.present_skipped_no_buffer;
            return TdvpK230PresentDecision::NoFreeBuffer;
        }

        latest_frame_available_ = false;
        frame_callback_pending_ = true;
        ++stats_.present_committed;
        return TdvpK230PresentDecision::Commit;
    }

    void note_frame_callback()
    {
        frame_callback_pending_ = false;
        ++stats_.frame_callbacks;
    }

    void note_buffer_release()
    {
        ++stats_.buffer_releases;
    }

    bool has_latest_frame() const
    {
        return latest_frame_available_;
    }

    bool frame_callback_pending() const
    {
        return frame_callback_pending_;
    }

    const TdvpK230PresentStats& stats() const
    {
        return stats_;
    }

private:
    TdvpK230PresentStats stats_;
    bool latest_frame_available_ = false;
    bool frame_callback_pending_ = false;
};

} // namespace czgba::platform
