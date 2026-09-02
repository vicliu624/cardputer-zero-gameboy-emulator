#pragma once

#include <cstdint>
#include <memory>

struct SDL_Window;

namespace czgba::platform {

struct TdvpK230WaylandShmState;

// A normal SDL-created Wayland window supplies focus and keyboard events. This
// presenter attaches a small CPU-written XRGB source directly to that
// wl_surface, preferring a linear DMA-BUF and retaining an equally small
// wl_shm fallback. wp_viewporter hands 3x presentation to the compositor,
// avoiding SDL_Renderer, EGL, and CPU-side full-screen rasterisation.
class TdvpK230WaylandShm {
public:
    TdvpK230WaylandShm();
    ~TdvpK230WaylandShm();

    TdvpK230WaylandShm(const TdvpK230WaylandShm&) = delete;
    TdvpK230WaylandShm& operator=(const TdvpK230WaylandShm&) = delete;

    bool init(SDL_Window* window);
    void shutdown();
    void present(const std::uint32_t* canvas_xrgb8888, int canvas_width, int canvas_height, int pitch_bytes);

private:
    std::unique_ptr<TdvpK230WaylandShmState> state_;
};

} // namespace czgba::platform
