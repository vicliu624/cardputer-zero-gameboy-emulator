#include "platform/tdvp_k230_wayland_shm.hpp"

#include <SDL.h>

#include <cerrno>
#include <cstring>
#include <iostream>

#if defined(CZ_GBA_HAS_TDVP_WAYLAND_PRESENTER)
#include <SDL_syswm.h>

#include <wayland-client.h>

#include "linux-dmabuf-unstable-v1-client-protocol.h"
#include "viewporter-client-protocol.h"

#if defined(CZ_GBA_HAS_DRM_KMS)
#include <dirent.h>
#include <drm/drm.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_mode.h>
#include <sys/ioctl.h>
#endif

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <string>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "platform/presentation_profile.hpp"
#include "render/layout.hpp"
#endif

namespace czgba::platform {

struct TdvpK230WaylandShmState {
#if defined(CZ_GBA_HAS_TDVP_WAYLAND_PRESENTER)
    enum class Transport {
        Shm,
        Dmabuf,
    };

    struct Buffer {
        int fd = -1;
        int prime_fd = -1;
        std::uint32_t dumb_handle = 0;
        std::uint32_t* pixels = nullptr;
        std::size_t size = 0;
        std::uint32_t stride = 0;
        wl_shm_pool* pool = nullptr;
        wl_buffer* buffer = nullptr;
        bool busy = false;
        std::uint64_t static_generation = 0;
    };

    wl_display* display = nullptr; // SDL owns the display connection.
    wl_surface* surface = nullptr; // SDL owns the xdg surface role.
    wl_registry* registry = nullptr;
    wl_shm* shm = nullptr;
    wp_viewporter* viewporter = nullptr;
    wp_viewport* viewport = nullptr;
    zwp_linux_dmabuf_v1* dmabuf = nullptr;
    bool dmabuf_xrgb8888_linear = false;
#if defined(CZ_GBA_HAS_DRM_KMS)
    int drm_fd = -1;
#endif
    Transport transport = Transport::Shm;
    std::array<Buffer, 3> buffers{};
    PresentationRect destination{};
    bool destination_offset_committed = false;
    std::uint64_t static_generation = 1;
    int static_source_width = 0;
    int static_source_height = 0;
    std::vector<std::uint32_t> static_source;
    std::vector<std::uint32_t> static_pixels;
#endif
};

#if defined(CZ_GBA_HAS_TDVP_WAYLAND_PRESENTER)
namespace {

using State = TdvpK230WaylandShmState;

constexpr int kSourceWidth = render::TdvpK230Layout::ScreenW;
constexpr int kSourceHeight = render::TdvpK230Layout::ScreenH;
constexpr std::uint32_t kBytesPerPixel = sizeof(std::uint32_t);
// wl_surface.offset was appended after damage_buffer (the v4 request), so it
// is request number 10 in the stable core protocol. Some K230 SDK headers
// predate the generated wl_surface_offset() wrapper even though the runtime
// compositor binds wl_surface at v5 or newer; marshal it only after checking
// that runtime object version below.
constexpr std::uint32_t kWlSurfaceOffsetRequest = 10;

void log_errno(const char* action)
{
    std::cerr << "TDVP K230 Wayland presenter: " << action << ": " << std::strerror(errno) << '\n';
}

bool is_game_pixel(int x, int y)
{
    using T = render::TdvpK230Layout;
    return x >= T::GameX && x < T::GameX + T::GameW && y >= T::GameY && y < T::GameY + T::GameH;
}

void buffer_release(void* data, wl_buffer*)
{
    static_cast<State::Buffer*>(data)->busy = false;
}

constexpr wl_buffer_listener kBufferListener{
    buffer_release,
};

void dmabuf_format(void*, zwp_linux_dmabuf_v1*, std::uint32_t)
{
    // An explicit linear modifier is required for the K230 source import.
}

void dmabuf_modifier(void* data, zwp_linux_dmabuf_v1*, std::uint32_t format,
    std::uint32_t modifier_hi, std::uint32_t modifier_lo)
{
    auto& state = *static_cast<State*>(data);
    const std::uint64_t modifier = (static_cast<std::uint64_t>(modifier_hi) << 32U) | modifier_lo;
    if (format == DRM_FORMAT_XRGB8888 && modifier == DRM_FORMAT_MOD_LINEAR) {
        state.dmabuf_xrgb8888_linear = true;
    }
}

constexpr zwp_linux_dmabuf_v1_listener kDmabufListener{
    dmabuf_format,
    dmabuf_modifier,
};

void registry_global(void* data, wl_registry* registry, std::uint32_t name, const char* interface, std::uint32_t version)
{
    auto& state = *static_cast<State*>(data);
    if (state.shm == nullptr && std::strcmp(interface, wl_shm_interface.name) == 0) {
        state.shm = static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, std::min(version, 1U)));
    } else if (state.viewporter == nullptr && std::strcmp(interface, wp_viewporter_interface.name) == 0) {
        state.viewporter = static_cast<wp_viewporter*>(wl_registry_bind(registry, name, &wp_viewporter_interface, std::min(version, 1U)));
    } else if (state.dmabuf == nullptr && std::strcmp(interface, zwp_linux_dmabuf_v1_interface.name) == 0) {
        const std::uint32_t bound_version = std::min(version, 3U);
        if (bound_version >= 3U) {
            state.dmabuf = static_cast<zwp_linux_dmabuf_v1*>(wl_registry_bind(
                registry, name, &zwp_linux_dmabuf_v1_interface, bound_version));
            zwp_linux_dmabuf_v1_add_listener(state.dmabuf, &kDmabufListener, &state);
        }
    }
}

void registry_remove(void*, wl_registry*, std::uint32_t)
{
}

constexpr wl_registry_listener kRegistryListener{
    registry_global,
    registry_remove,
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

void destroy_buffer(State& state, State::Buffer& buffer)
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
    if (buffer.prime_fd >= 0) {
        close(buffer.prime_fd);
        buffer.prime_fd = -1;
    }
#if defined(CZ_GBA_HAS_DRM_KMS)
    if (buffer.dumb_handle != 0 && state.drm_fd >= 0) {
        drm_mode_destroy_dumb destroy{};
        destroy.handle = buffer.dumb_handle;
        if (ioctl(state.drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy) != 0) {
            log_errno("DRM_IOCTL_MODE_DESTROY_DUMB");
        }
        buffer.dumb_handle = 0;
    }
#else
    (void)state;
#endif
    if (buffer.fd >= 0) {
        close(buffer.fd);
        buffer.fd = -1;
    }
    buffer.size = 0;
    buffer.stride = 0;
    buffer.busy = false;
    buffer.static_generation = 0;
}

bool create_shm_buffer(State& state, State::Buffer& buffer)
{
    if (state.shm == nullptr) {
        return false;
    }
    buffer.stride = kSourceWidth * kBytesPerPixel;
    buffer.size = static_cast<std::size_t>(buffer.stride) * kSourceHeight;
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
        std::cerr << "TDVP K230 Wayland presenter: failed to create wl_shm pool\n";
        return false;
    }
    buffer.buffer = wl_shm_pool_create_buffer(buffer.pool, 0, kSourceWidth, kSourceHeight,
        static_cast<int>(buffer.stride), WL_SHM_FORMAT_XRGB8888);
    if (buffer.buffer == nullptr) {
        std::cerr << "TDVP K230 Wayland presenter: failed to create wl_shm buffer\n";
        return false;
    }
    wl_buffer_add_listener(buffer.buffer, &kBufferListener, &buffer);
    return true;
}

#if defined(CZ_GBA_HAS_DRM_KMS)
int open_drm_card()
{
    DIR* directory = opendir("/dev/dri");
    if (directory == nullptr) {
        log_errno("open /dev/dri");
        return -1;
    }
    while (dirent* entry = readdir(directory)) {
        if (std::strncmp(entry->d_name, "card", 4) != 0 || entry->d_name[4] == '\0') {
            continue;
        }
        bool digits_only = true;
        for (const char* character = entry->d_name + 4; *character != '\0'; ++character) {
            if (*character < '0' || *character > '9') {
                digits_only = false;
                break;
            }
        }
        if (!digits_only) {
            continue;
        }
        const std::string path = std::string("/dev/dri/") + entry->d_name;
        const int fd = open(path.c_str(), O_RDWR | O_CLOEXEC);
        if (fd >= 0) {
            closedir(directory);
            return fd;
        }
    }
    closedir(directory);
    std::cerr << "TDVP K230 Wayland presenter: no readable DRM card for DMA-BUF allocation\n";
    return -1;
}

bool create_dmabuf_buffer(State& state, State::Buffer& buffer)
{
    drm_mode_create_dumb create{};
    create.width = kSourceWidth;
    create.height = kSourceHeight;
    create.bpp = 32;
    if (ioctl(state.drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) != 0) {
        log_errno("DRM_IOCTL_MODE_CREATE_DUMB");
        return false;
    }
    buffer.dumb_handle = create.handle;
    buffer.stride = create.pitch;
    buffer.size = static_cast<std::size_t>(create.size);

    drm_mode_map_dumb map{};
    map.handle = create.handle;
    if (ioctl(state.drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map) != 0) {
        log_errno("DRM_IOCTL_MODE_MAP_DUMB");
        return false;
    }
    void* mapping = mmap(nullptr, buffer.size, PROT_READ | PROT_WRITE, MAP_SHARED, state.drm_fd,
        static_cast<off_t>(map.offset));
    if (mapping == MAP_FAILED) {
        buffer.pixels = nullptr;
        log_errno("mmap DRM dumb buffer");
        return false;
    }
    buffer.pixels = static_cast<std::uint32_t*>(mapping);

    drm_prime_handle prime{};
    prime.handle = create.handle;
    prime.flags = DRM_CLOEXEC | DRM_RDWR;
    if (ioctl(state.drm_fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &prime) != 0) {
        log_errno("DRM_IOCTL_PRIME_HANDLE_TO_FD");
        return false;
    }
    buffer.prime_fd = prime.fd;

    zwp_linux_buffer_params_v1* params = zwp_linux_dmabuf_v1_create_params(state.dmabuf);
    if (params == nullptr) {
        std::cerr << "TDVP K230 Wayland presenter: failed to create linux-dmabuf parameters\n";
        return false;
    }
    const int submitted_fd = fcntl(buffer.prime_fd, F_DUPFD_CLOEXEC, 3);
    if (submitted_fd < 0) {
        log_errno("duplicate DMA-BUF fd for Wayland");
        zwp_linux_buffer_params_v1_destroy(params);
        return false;
    }
    zwp_linux_buffer_params_v1_add(params, submitted_fd, 0, 0, buffer.stride,
        DRM_FORMAT_MOD_LINEAR >> 32U, DRM_FORMAT_MOD_LINEAR & 0xffffffffU);
    buffer.buffer = zwp_linux_buffer_params_v1_create_immed(params, kSourceWidth, kSourceHeight,
        DRM_FORMAT_XRGB8888, 0);
    zwp_linux_buffer_params_v1_destroy(params);
    if (buffer.buffer == nullptr) {
        std::cerr << "TDVP K230 Wayland presenter: compositor rejected linear XRGB DMA-BUF\n";
        return false;
    }
    wl_buffer_add_listener(buffer.buffer, &kBufferListener, &buffer);
    return true;
}
#endif

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

void cache_static_pixels(State& state, const std::uint32_t* canvas, int width, int height, int stride)
{
    std::fill(state.static_pixels.begin(), state.static_pixels.end(), 0U);
    for (int y = 0; y < height; ++y) {
        const auto* source_row = canvas + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride);
        auto* target_row = state.static_pixels.data() + static_cast<std::size_t>(y) * kSourceWidth;
        for (int x = 0; x < width; ++x) {
            if (!is_game_pixel(x, y)) {
                target_row[x] = source_row[x];
            }
        }
    }
    state.static_source_width = width;
    state.static_source_height = height;
    state.static_source.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        std::copy_n(canvas + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride), width,
            state.static_source.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width));
    }
    ++state.static_generation;
}

void copy_static_pixels(State::Buffer& buffer, const std::vector<std::uint32_t>& pixels)
{
    const std::size_t destination_stride = buffer.stride / kBytesPerPixel;
    for (int y = 0; y < kSourceHeight; ++y) {
        std::copy_n(pixels.data() + static_cast<std::size_t>(y) * kSourceWidth, kSourceWidth,
            buffer.pixels + static_cast<std::size_t>(y) * destination_stride);
    }
}

void copy_game_pixels(State::Buffer& buffer, const std::uint32_t* canvas, int source_stride)
{
    using T = render::TdvpK230Layout;
    const std::size_t destination_stride = buffer.stride / kBytesPerPixel;
    for (int y = 0; y < T::GameH; ++y) {
        const auto* source = canvas + static_cast<std::size_t>(T::GameY + y) * static_cast<std::size_t>(source_stride) + T::GameX;
        auto* target = buffer.pixels + static_cast<std::size_t>(T::GameY + y) * destination_stride + T::GameX;
        std::copy_n(source, T::GameW, target);
    }
}

State::Buffer* next_available_buffer(State& state)
{
    // SDL_PollEvent owns normal Wayland dispatch. Only consume events already
    // queued and publish a latest frame when a source buffer is reusable;
    // display back-pressure must never stall emulation or audio.
    wl_display_dispatch_pending(state.display);
    for (auto& buffer : state.buffers) {
        if (!buffer.busy) {
            return &buffer;
        }
    }
    return nullptr;
}

void attach_with_destination_offset(State& state, wl_buffer* buffer)
{
    const auto surface_version = wl_proxy_get_version(reinterpret_cast<wl_proxy*>(state.surface));
    if (!state.destination_offset_committed && surface_version >= 5U) {
        wl_surface_attach(state.surface, buffer, 0, 0);
        wl_proxy_marshal(reinterpret_cast<wl_proxy*>(state.surface), kWlSurfaceOffsetRequest,
            state.destination.x, state.destination.y);
    } else if (!state.destination_offset_committed) {
        wl_surface_attach(state.surface, buffer, state.destination.x, state.destination.y);
    } else {
        // wl_surface offsets are relative to the previous buffer. Once the
        // one-pixel centering offset is committed, every following buffer
        // must use the default zero delta to keep the surface stationary.
        wl_surface_attach(state.surface, buffer, 0, 0);
    }
    state.destination_offset_committed = true;
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

#if defined(CZ_GBA_HAS_TDVP_WAYLAND_PRESENTER) && defined(SDL_VIDEO_DRIVER_WAYLAND)
    if (window == nullptr) {
        return false;
    }

    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info) != SDL_TRUE || info.subsystem != SDL_SYSWM_WAYLAND ||
        info.info.wl.display == nullptr || info.info.wl.surface == nullptr) {
        std::cerr << "TDVP K230 Wayland presenter: SDL did not create a Wayland-native window\n";
        return false;
    }

    auto state = std::make_unique<TdvpK230WaylandShmState>();
    state->display = info.info.wl.display;
    state->surface = info.info.wl.surface;
    state->destination = integer_presentation_rect(
        kSourceWidth, kSourceHeight, kK230LandscapeWidth, kK230LandscapeHeight);
    if (state->destination.scale != 3) {
        std::cerr << "TDVP K230 Wayland presenter: expected a 3x source destination, got "
                  << state->destination.scale << "x\n";
        return false;
    }
    state->static_pixels.resize(static_cast<std::size_t>(kSourceWidth) * kSourceHeight);
    state->registry = wl_display_get_registry(state->display);
    if (state->registry == nullptr) {
        std::cerr << "TDVP K230 Wayland presenter: wl_display_get_registry failed\n";
        return false;
    }
    wl_registry_add_listener(state->registry, &kRegistryListener, state.get());
    if (wl_display_roundtrip(state->display) < 0 || state->viewporter == nullptr) {
        std::cerr << "TDVP K230 Wayland presenter: compositor did not expose wp_viewporter\n";
        return false;
    }
    state->viewport = wp_viewporter_get_viewport(state->viewporter, state->surface);
    if (state->viewport == nullptr) {
        std::cerr << "TDVP K230 Wayland presenter: failed to create wp_viewport\n";
        return false;
    }
    wp_viewport_set_destination(state->viewport, state->destination.width, state->destination.height);

    bool dma_buffers_created = false;
#if defined(CZ_GBA_HAS_DRM_KMS)
    if (state->dmabuf != nullptr && state->dmabuf_xrgb8888_linear) {
        state->drm_fd = open_drm_card();
        if (state->drm_fd >= 0) {
            dma_buffers_created = true;
            state->transport = State::Transport::Dmabuf;
            for (auto& buffer : state->buffers) {
                if (!create_dmabuf_buffer(*state, buffer)) {
                    dma_buffers_created = false;
                    break;
                }
            }
        }
        if (!dma_buffers_created) {
            for (auto& buffer : state->buffers) {
                destroy_buffer(*state, buffer);
            }
            if (state->drm_fd >= 0) {
                close(state->drm_fd);
                state->drm_fd = -1;
            }
        }
    }
#endif

    if (!dma_buffers_created) {
        if (state->shm == nullptr) {
            std::cerr << "TDVP K230 Wayland presenter: neither linear linux-dmabuf nor wl_shm is available\n";
            return false;
        }
        state->transport = State::Transport::Shm;
        for (auto& buffer : state->buffers) {
            if (!create_shm_buffer(*state, buffer)) {
                for (auto& created : state->buffers) {
                    destroy_buffer(*state, created);
                }
                return false;
            }
        }
    }

    state_ = std::move(state);
    const char* transport = state_->transport == State::Transport::Dmabuf ? "zero-copy DMA-BUF" : "small wl_shm fallback";
    std::cout << "TDVP K230: using " << transport << " Wayland source "
              << kSourceWidth << 'x' << kSourceHeight << " -> VGLite 3x viewport\n";
    return true;
#else
    (void)window;
    std::cerr << "TDVP K230 Wayland presenter: this build lacks its Wayland protocol ABI\n";
    return false;
#endif
}

void TdvpK230WaylandShm::shutdown()
{
#if defined(CZ_GBA_HAS_TDVP_WAYLAND_PRESENTER)
    if (state_) {
        for (auto& buffer : state_->buffers) {
            destroy_buffer(*state_, buffer);
        }
#if defined(CZ_GBA_HAS_DRM_KMS)
        if (state_->drm_fd >= 0) {
            close(state_->drm_fd);
            state_->drm_fd = -1;
        }
#endif
        if (state_->viewport != nullptr) {
            wp_viewport_destroy(state_->viewport);
            state_->viewport = nullptr;
        }
        if (state_->dmabuf != nullptr) {
            zwp_linux_dmabuf_v1_destroy(state_->dmabuf);
            state_->dmabuf = nullptr;
        }
        if (state_->viewporter != nullptr) {
            wp_viewporter_destroy(state_->viewporter);
            state_->viewporter = nullptr;
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
#if defined(CZ_GBA_HAS_TDVP_WAYLAND_PRESENTER)
    if (!state_ || canvas_xrgb8888 == nullptr || canvas_width != kSourceWidth ||
        canvas_height != kSourceHeight || pitch_bytes <= 0) {
        return;
    }
    const int source_stride = pitch_bytes / static_cast<int>(kBytesPerPixel);
    if (source_stride < canvas_width) {
        return;
    }

    State& state = *state_;
    const bool static_changed = static_pixels_changed(
        state, canvas_xrgb8888, canvas_width, canvas_height, source_stride);
    if (static_changed) {
        cache_static_pixels(state, canvas_xrgb8888, canvas_width, canvas_height, source_stride);
    }

    State::Buffer* buffer = next_available_buffer(state);
    if (buffer == nullptr) {
        return;
    }
    const bool needs_static_copy = buffer->static_generation != state.static_generation;
    if (needs_static_copy) {
        copy_static_pixels(*buffer, state.static_pixels);
        buffer->static_generation = state.static_generation;
    }
    copy_game_pixels(*buffer, canvas_xrgb8888, source_stride);

    buffer->busy = true;
    attach_with_destination_offset(state, buffer->buffer);
    if (static_changed || needs_static_copy) {
        wl_surface_damage(state.surface, state.destination.x, state.destination.y,
            state.destination.width, state.destination.height);
    } else {
        using T = render::TdvpK230Layout;
        wl_surface_damage(state.surface,
            state.destination.x + T::GameX * state.destination.scale,
            state.destination.y + T::GameY * state.destination.scale,
            T::GameW * state.destination.scale,
            T::GameH * state.destination.scale);
    }
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
