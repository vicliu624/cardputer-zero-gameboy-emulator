#pragma once

#include <chrono>
#include <cstdint>
#include <memory>

#include "input/input_frame.hpp"
#include "render/tdvp_k230_presentation.hpp"

namespace czgba::platform {

struct TdvpK230WaylandShmState;

// TDVP owns one native Wayland client connection. The root xdg_toplevel keeps
// static chrome in a full-output wl_shm buffer while a desynchronised child
// wl_subsurface contains only the changing 720x480 GBA viewport. SDL remains
// available to the process for PulseAudio, but never owns this display or its
// event queue.
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
    bool should_quit() const;

private:
    std::unique_ptr<TdvpK230WaylandShmState> state_;
};

} // namespace czgba::platform
