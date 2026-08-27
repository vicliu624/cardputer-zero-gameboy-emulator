#pragma once

#include <cstdint>
#include <memory>

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
    void present(const std::uint32_t* canvas_xrgb8888, int canvas_width, int canvas_height, int pitch_bytes);

private:
    std::unique_ptr<TdvpK230WaylandShmState> state_;
};

} // namespace czgba::platform
