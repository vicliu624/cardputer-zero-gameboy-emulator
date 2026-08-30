#include "platform/tdvp_k230_wayland_shm.hpp"

#include <SDL.h>

#include <cerrno>
#include <cstring>
#include <iostream>

#if defined(CZ_GBA_HAS_TDVP_WAYLAND_SHM)
#include <SDL_syswm.h>

#include <wayland-client.h>

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "platform/presentation_profile.hpp"
#include "render/layout.hpp"
#endif

namespace czgba::platform {

struct TdvpK230WaylandShmState {
#if defined(CZ_GBA_HAS_TDVP_WAYLAND_SHM)
    struct Buffer {
        int fd = -1;
        std::uint32_t* pixels = nullptr;
        std::size_t size = 0;
        wl_shm_pool* pool = nullptr;
        wl_buffer* buffer = nullptr;
        bool busy = false;
        std::uint64_t static_generation = 0;
    };

    wl_display* display = nullptr; // SDL owns the display connection.
    wl_surface* surface = nullptr; // SDL owns the xdg surface role.
    wl_registry* registry = nullptr;
    wl_shm* shm = nullptr;
    std::array<Buffer, 3> buffers{};
    int width = kK230LandscapeWidth;
    int height = kK230LandscapeHeight;
    std::uint64_t static_generation = 1;
    int static_source_width = 0;
    int static_source_height = 0;
    std::vector<std::uint32_t> static_source;
    std::vector<std::uint32_t> static_pixels;
#endif
};

#if defined(CZ_GBA_HAS_TDVP_WAYLAND_SHM)
namespace {

using State = TdvpK230WaylandShmState;

void log_errno(const char* action)
{
    std::cerr << "TDVP K230 wl_shm: " << action << ": " << std::strerror(errno) << '\n';
}

void registry_global(void* data, wl_registry* registry, std::uint32_t name, const char* interface, std::uint32_t version)
{
    auto& state = *static_cast<State*>(data);
    if (state.shm == nullptr && std::strcmp(interface, wl_shm_interface.name) == 0) {
        state.shm = static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, std::min(version, 1U)));
    }
}

void registry_remove(void*, wl_registry*, std::uint32_t)
{
}

constexpr wl_registry_listener kRegistryListener{
    registry_global,
    registry_remove,
};

void buffer_release(void* data, wl_buffer*)
{
    static_cast<State::Buffer*>(data)->busy = false;
}

constexpr wl_buffer_listener kBufferListener{
    buffer_release,
};

int create_shared_memory_file(std::size_t size)
{
    char path[] = "/tmp/cardputer-zero-gba-wl-shm-XXXXXX";
    const int fd = mkstemp(path);
    if (fd < 0) {
        return -1;
    }
    unlink(path);
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0 || ftruncate(fd, static_cast<off_t>(size)) != 0) {
        const int saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return -1;
    }
    return fd;
}

bool create_buffer(State& state, State::Buffer& buffer)
{
    const int stride = state.width * static_cast<int>(sizeof(std::uint32_t));
    buffer.size = static_cast<std::size_t>(stride) * static_cast<std::size_t>(state.height);
    buffer.fd = create_shared_memory_file(buffer.size);
    if (buffer.fd < 0) {
        log_errno("create shared-memory file");
        return false;
    }

    void* mapping = mmap(nullptr, buffer.size, PROT_READ | PROT_WRITE, MAP_SHARED, buffer.fd, 0);
    if (mapping == MAP_FAILED) {
        buffer.pixels = nullptr;
        log_errno("mmap shared-memory buffer");
        return false;
    }
    buffer.pixels = static_cast<std::uint32_t*>(mapping);
    buffer.pool = wl_shm_create_pool(state.shm, buffer.fd, static_cast<int>(buffer.size));
    if (buffer.pool == nullptr) {
        std::cerr << "TDVP K230 wl_shm: failed to create wl_shm pool\n";
        return false;
    }
    buffer.buffer = wl_shm_pool_create_buffer(
        buffer.pool,
        0,
        state.width,
        state.height,
        stride,
        WL_SHM_FORMAT_XRGB8888);
    if (buffer.buffer == nullptr) {
        std::cerr << "TDVP K230 wl_shm: failed to create wl_shm buffer\n";
        return false;
    }
    wl_buffer_add_listener(buffer.buffer, &kBufferListener, &buffer);
    return true;
}

void destroy_buffer(State::Buffer& buffer)
{
    if (buffer.buffer != nullptr) {
        wl_buffer_destroy(buffer.buffer);
        buffer.buffer = nullptr;
    }
    if (buffer.pool != nullptr) {
        wl_shm_pool_destroy(buffer.pool);
        buffer.pool = nullptr;
    }
    if (buffer.pixels != nullptr) {
        munmap(buffer.pixels, buffer.size);
        buffer.pixels = nullptr;
    }
    if (buffer.fd >= 0) {
        close(buffer.fd);
        buffer.fd = -1;
    }
    buffer.size = 0;
    buffer.busy = false;
    buffer.static_generation = 0;
}

bool is_game_pixel(int x, int y)
{
    using T = render::TdvpK230Layout;
    return x >= T::GameX && x < T::GameX + T::GameW && y >= T::GameY && y < T::GameY + T::GameH;
}

bool static_pixels_changed(const State& state, const std::uint32_t* canvas, int width, int height, int stride)
{
    if (state.static_source_width != width || state.static_source_height != height ||
        state.static_source.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
        return true;
    }

    for (int y = 0; y < height; ++y) {
        const auto* source_row = canvas + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride);
        const auto* cached_row = state.static_source.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
        for (int x = 0; x < width; ++x) {
            if (!is_game_pixel(x, y) && source_row[x] != cached_row[x]) {
                return true;
            }
        }
    }
    return false;
}

void scale_static_pixels(State& state, const std::uint32_t* canvas, int width, int height, int stride)
{
    const auto destination = integer_presentation_rect(width, height, state.width, state.height);
    if (destination.scale != 3) {
        std::cerr << "TDVP K230 wl_shm: expected 3x integer presentation, got " << destination.scale << "x\n";
        return;
    }

    std::fill(state.static_pixels.begin(), state.static_pixels.end(), 0U);
    for (int y = 0; y < height; ++y) {
        const auto* source_row = canvas + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride);
        for (int x = 0; x < width; ++x) {
            if (is_game_pixel(x, y)) {
                continue;
            }
            const auto color = source_row[x];
            const int target_x = destination.x + x * destination.scale;
            const int target_y = destination.y + y * destination.scale;
            for (int dy = 0; dy < destination.scale; ++dy) {
                auto* target = state.static_pixels.data() +
                    static_cast<std::size_t>(target_y + dy) * static_cast<std::size_t>(state.width) + target_x;
                std::fill_n(target, destination.scale, color);
            }
        }
    }

    state.static_source_width = width;
    state.static_source_height = height;
    state.static_source.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        std::copy_n(
            canvas + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride),
            width,
            state.static_source.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width));
    }
    ++state.static_generation;
}

void scale_game_pixels(
    State& state,
    State::Buffer& buffer,
    const std::uint32_t* canvas,
    int canvas_width,
    int canvas_height,
    int source_stride)
{
    using T = render::TdvpK230Layout;
    const auto destination = integer_presentation_rect(canvas_width, canvas_height, state.width, state.height);
    if (destination.scale != 3) {
        return;
    }

    const int game_width = std::min(T::GameW, canvas_width - T::GameX);
    const int game_height = std::min(T::GameH, canvas_height - T::GameY);
    if (game_width <= 0 || game_height <= 0) {
        return;
    }

    for (int y = 0; y < game_height; ++y) {
        const auto* source = canvas + static_cast<std::size_t>(T::GameY + y) * static_cast<std::size_t>(source_stride) + T::GameX;
        const int target_y = destination.y + (T::GameY + y) * destination.scale;
        const int target_x = destination.x + T::GameX * destination.scale;
        for (int dy = 0; dy < destination.scale; ++dy) {
            auto* target = buffer.pixels + static_cast<std::size_t>(target_y + dy) * static_cast<std::size_t>(state.width) + target_x;
            for (int x = 0; x < game_width; ++x) {
                std::fill_n(target + x * destination.scale, destination.scale, source[x]);
            }
        }
    }
}

State::Buffer* next_available_buffer(State& state)
{
    // SDL_PollEvent owns the normal Wayland event dispatch on this connection.
    // Process events that are already queued, then present only when a wl_shm
    // buffer is immediately reusable. Waiting here couples compositor back
    // pressure to the UI thread; even though emulation is now independent,
    // dropping this display submission is the correct latest-frame policy.
    wl_display_dispatch_pending(state.display);
    for (auto& buffer : state.buffers) {
        if (!buffer.busy) {
            return &buffer;
        }
    }
    return nullptr;
}

} // namespace
#endif

TdvpK230WaylandShm::TdvpK230WaylandShm() = default;

TdvpK230WaylandShm::~TdvpK230WaylandShm()
{
    shutdown();
}

bool TdvpK230WaylandShm::init(SDL_Window* window)
{
    shutdown();

#if defined(CZ_GBA_HAS_TDVP_WAYLAND_SHM) && defined(SDL_VIDEO_DRIVER_WAYLAND)
    if (window == nullptr) {
        return false;
    }

    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info) != SDL_TRUE || info.subsystem != SDL_SYSWM_WAYLAND ||
        info.info.wl.display == nullptr || info.info.wl.surface == nullptr) {
        std::cerr << "TDVP K230 wl_shm: SDL did not create a Wayland-native window\n";
        return false;
    }

    auto state = std::make_unique<TdvpK230WaylandShmState>();
    state->display = info.info.wl.display;
    state->surface = info.info.wl.surface;
    state->static_pixels.resize(static_cast<std::size_t>(state->width) * static_cast<std::size_t>(state->height));
    state->registry = wl_display_get_registry(state->display);
    if (state->registry == nullptr) {
        std::cerr << "TDVP K230 wl_shm: wl_display_get_registry failed\n";
        return false;
    }
    wl_registry_add_listener(state->registry, &kRegistryListener, state.get());
    if (wl_display_roundtrip(state->display) < 0 || state->shm == nullptr) {
        std::cerr << "TDVP K230 wl_shm: compositor did not expose wl_shm\n";
        return false;
    }
    for (auto& buffer : state->buffers) {
        if (!create_buffer(*state, buffer)) {
            for (auto& created : state->buffers) {
                destroy_buffer(created);
            }
            if (state->shm != nullptr) {
                wl_shm_destroy(state->shm);
            }
            if (state->registry != nullptr) {
                wl_registry_destroy(state->registry);
            }
            return false;
        }
    }

    state_ = std::move(state);
    std::cout << "TDVP K230: using direct Wayland wl_shm presentation\n";
    return true;
#else
    (void)window;
    std::cerr << "TDVP K230 wl_shm: this build has no Wayland client ABI support\n";
    return false;
#endif
}

void TdvpK230WaylandShm::shutdown()
{
#if defined(CZ_GBA_HAS_TDVP_WAYLAND_SHM)
    if (state_) {
        for (auto& buffer : state_->buffers) {
            destroy_buffer(buffer);
        }
        if (state_->shm != nullptr) {
            wl_shm_destroy(state_->shm);
            state_->shm = nullptr;
        }
        if (state_->registry != nullptr) {
            wl_registry_destroy(state_->registry);
            state_->registry = nullptr;
        }
    }
#endif
    state_.reset();
}

void TdvpK230WaylandShm::present(
    const std::uint32_t* canvas_xrgb8888,
    int canvas_width,
    int canvas_height,
    int pitch_bytes)
{
#if defined(CZ_GBA_HAS_TDVP_WAYLAND_SHM)
    if (!state_ || canvas_xrgb8888 == nullptr || canvas_width <= 0 || canvas_height <= 0 || pitch_bytes <= 0) {
        return;
    }
    const int source_stride = pitch_bytes / static_cast<int>(sizeof(std::uint32_t));
    if (source_stride < canvas_width) {
        return;
    }

    State& state = *state_;
    if (static_pixels_changed(state, canvas_xrgb8888, canvas_width, canvas_height, source_stride)) {
        scale_static_pixels(state, canvas_xrgb8888, canvas_width, canvas_height, source_stride);
    }

    State::Buffer* buffer = next_available_buffer(state);
    if (buffer == nullptr) {
        // The compositor is still scanning every application-owned buffer.
        // Reuse the last presented frame instead of blocking the caller.
        return;
    }
    if (buffer->static_generation != state.static_generation) {
        std::memcpy(buffer->pixels, state.static_pixels.data(), buffer->size);
        buffer->static_generation = state.static_generation;
    }
    scale_game_pixels(state, *buffer, canvas_xrgb8888, canvas_width, canvas_height, source_stride);

    buffer->busy = true;
    wl_surface_attach(state.surface, buffer->buffer, 0, 0);
    wl_surface_damage(state.surface, 0, 0, state.width, state.height);
    wl_surface_commit(state.surface);
    if (wl_display_flush(state.display) < 0 && errno != EAGAIN) {
        log_errno("wl_display_flush");
    }
#else
    (void)canvas_xrgb8888;
    (void)canvas_width;
    (void)canvas_height;
    (void)pitch_bytes;
#endif
}

} // namespace czgba::platform
