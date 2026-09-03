#pragma once

#include <chrono>
#include <cstdint>
#include <memory>

#include "input/input_frame.hpp"
#include "render/tdvp_k230_presentation.hpp"

namespace czgba::platform {

struct TdvpK230WaylandShmState;

// TDVP owns one native Wayland client connection and one standard
// xdg_toplevel.  Each full-output wl_shm buffer caches static chrome and only
// rewrites the changing 720x480 GBA rectangle.  A buffer-release based
// scheduler deliberately keeps video submission independent from emulation,
// audio, and input. SDL remains available to the process for PulseAudio, but
// never owns this display or its event queue.
class TdvpK230WaylandShm {
public:
    TdvpK230WaylandShm();
    ~TdvpK230WaylandShm();

    TdvpK230WaylandShm(const TdvpK230WaylandShm&) = delete;
    TdvpK230WaylandShm& operator=(const TdvpK230WaylandShm&) = delete;

    bool init();
    void shutdown();
    void poll_events(input::InputFrame& input);
    void wait_until(std::chrono::steady_clock::time_point deadline);
    void present(const render::TdvpK230PresentationFrame& frame);
    // Rendering and pixel normalization are skipped while every compositor
    // buffer is busy. The emulation/audio loop still advances; the next
    // available buffer receives its latest frame.
    bool can_accept_present() const;
    bool should_quit() const;

private:
    std::unique_ptr<TdvpK230WaylandShmState> state_;
};

} // namespace czgba::platform
