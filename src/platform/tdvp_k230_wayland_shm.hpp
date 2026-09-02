#pragma once

#include <cstdint>
#include <memory>

#include "render/tdvp_k230_presentation.hpp"

struct SDL_Window;

namespace czgba::platform {

struct TdvpK230WaylandShmState;

// A normal SDL-created Wayland window supplies focus and keyboard events. This
// presenter attaches CPU-owned XRGB wl_shm buffers directly to that window's
// wl_surface, avoiding SDL_Renderer, EGL and software GLES rasterisation.
class TdvpK230WaylandShm {
public:
    TdvpK230WaylandShm();
    ~TdvpK230WaylandShm();

    TdvpK230WaylandShm(const TdvpK230WaylandShm&) = delete;
    TdvpK230WaylandShm& operator=(const TdvpK230WaylandShm&) = delete;

    bool init(SDL_Window* window);
    void shutdown();
    // Returns true only when a new frame can be built and committed without
    // waiting for a compositor callback or writing a busy wl_buffer.
    bool ready_for_frame();
    void present(const render::TdvpK230PresentationFrame& frame);

private:
    std::unique_ptr<TdvpK230WaylandShmState> state_;
};

} // namespace czgba::platform
