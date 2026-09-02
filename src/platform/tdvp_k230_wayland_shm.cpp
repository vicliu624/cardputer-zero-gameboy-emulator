#include "platform/tdvp_k230_wayland_shm.hpp"

#include <SDL.h>

#include <cerrno>
#include <chrono>
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
#include "platform/tdvp_k230_present_geometry.hpp"
#include "platform/tdvp_k230_present_scheduler.hpp"
#include "render/layout.hpp"
#endif

namespace czgba::platform {

struct TdvpK230WaylandShmState {
#if defined(CZ_GBA_HAS_TDVP_WAYLAND_SHM)
    struct Buffer {
        TdvpK230WaylandShmState* state = nullptr;
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
    wl_callback* frame_callback = nullptr;
    std::array<Buffer, 3> buffers{};
    TdvpK230PresentScheduler scheduler;
    int width = kK230LandscapeWidth;
    int height = kK230LandscapeHeight;
    std::uint64_t static_generation = 0;
    bool full_damage_pending = true;
    std::vector<std::uint32_t> static_pixels;
    std::chrono::steady_clock::time_point last_stats_log;
    std::chrono::steady_clock::time_point last_frame_callback;
    double last_frame_callback_interval_ms = 0.0;
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
    auto& buffer = *static_cast<State::Buffer*>(data);
    buffer.busy = false;
    if (buffer.state != nullptr) {
        buffer.state->scheduler.note_buffer_release();
    }
}

constexpr wl_buffer_listener kBufferListener{
    buffer_release,
};

void frame_callback_done(void* data, wl_callback* callback, std::uint32_t)
{
    auto& state = *static_cast<State*>(data);
    if (state.frame_callback == callback) {
        state.frame_callback = nullptr;
    }
    const auto now = std::chrono::steady_clock::now();
    if (state.last_frame_callback.time_since_epoch().count() != 0) {
        state.last_frame_callback_interval_ms =
            std::chrono::duration<double, std::milli>(now - state.last_frame_callback).count();
    }
    state.last_frame_callback = now;
    state.scheduler.note_frame_callback();
    wl_callback_destroy(callback);
}

constexpr wl_callback_listener kFrameCallbackListener{
    frame_callback_done,
};

bool frame_timing_enabled()
{
    const char* value = std::getenv("CARDPUTER_ZERO_GBA_FRAME_TIMING");
    return value != nullptr && value[0] == '1' && value[1] == '\0';
}

void maybe_log_present_stats(State& state)
{
    if (!frame_timing_enabled()) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (state.last_stats_log.time_since_epoch().count() != 0 &&
        now - state.last_stats_log < std::chrono::seconds(1)) {
        return;
    }
    state.last_stats_log = now;
    const auto& stats = state.scheduler.stats();
    std::cerr << "TDVP K230 frame timing: requested=" << stats.present_requested
              << " committed=" << stats.present_committed
              << " skipped_callback=" << stats.present_skipped_frame_callback
              << " skipped_no_buffer=" << stats.present_skipped_no_buffer
              << " callbacks=" << stats.frame_callbacks
              << " releases=" << stats.buffer_releases
              << " callback_interval_ms=" << state.last_frame_callback_interval_ms
              << '\n';
}

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
    buffer.state = &state;
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
    buffer.state = nullptr;
}

bool scale_static_pixels(State& state, const render::TdvpK230PresentationFrame& frame)
{
    if (frame.static_pixels_xrgb8888 == nullptr || frame.static_width <= 0 || frame.static_height <= 0 ||
        frame.static_pitch_bytes <= 0) {
        return false;
    }
    const int source_stride = frame.static_pitch_bytes / static_cast<int>(sizeof(std::uint32_t));
    if (source_stride < frame.static_width) {
        return false;
    }

    const auto destination = integer_presentation_rect(
        frame.static_width, frame.static_height, state.width, state.height);
    if (destination.scale != 3) {
        std::cerr << "TDVP K230 wl_shm: expected 3x integer presentation, got " << destination.scale << "x\n";
        return false;
    }

    std::fill(state.static_pixels.begin(), state.static_pixels.end(), 0U);
    for (int y = 0; y < frame.static_height; ++y) {
        const auto* source_row = frame.static_pixels_xrgb8888 +
            static_cast<std::size_t>(y) * static_cast<std::size_t>(source_stride);
        for (int x = 0; x < frame.static_width; ++x) {
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
    state.static_generation = frame.static_generation;
    return true;
}

void scale_game_pixels(
    State& state,
    State::Buffer& buffer,
    const render::TdvpK230PresentationFrame& frame)
{
    if (!frame.game_frame_updated || frame.game_pixels_xrgb8888 == nullptr || frame.game_width <= 0 ||
        frame.game_height <= 0 || frame.game_pitch_pixels < frame.game_width) {
        return;
    }
    using T = render::TdvpK230Layout;
    const auto destination = integer_presentation_rect(
        frame.static_width, frame.static_height, state.width, state.height);
    if (destination.scale != 3) {
        return;
    }

    const int game_width = std::min(T::GameW, frame.game_width);
    const int game_height = std::min(T::GameH, frame.game_height);
    if (game_width <= 0 || game_height <= 0) {
        return;
    }

    constexpr std::uint32_t kGameBorder = (22U << 16) | (25U << 8) | 28U;
    for (int y = 0; y < game_height; ++y) {
        const auto* source = frame.game_pixels_xrgb8888 +
            static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.game_pitch_pixels);
        const int target_y = destination.y + (T::GameY + y) * destination.scale;
        const int target_x = destination.x + T::GameX * destination.scale;
        for (int dy = 0; dy < destination.scale; ++dy) {
            auto* target = buffer.pixels + static_cast<std::size_t>(target_y + dy) * static_cast<std::size_t>(state.width) + target_x;
            for (int x = 0; x < game_width; ++x) {
                const bool border = x == 0 || y == 0 || x == T::GameW - 1 || y == T::GameH - 1;
                std::fill_n(target + x * destination.scale, destination.scale, border ? kGameBorder : source[x]);
            }
        }
    }
}

State::Buffer* next_available_buffer(State& state)
{
    for (auto& buffer : state.buffers) {
        if (!buffer.busy) {
            return &buffer;
        }
    }
    return nullptr;
}

void dispatch_pending_events(State& state)
{
    // SDL owns the connection and continues to drive keyboard/window events.
    // Draining only already-queued Wayland events lets buffer releases and
    // frame callbacks advance without ever blocking the emulation thread.
    if (wl_display_dispatch_pending(state.display) < 0) {
        std::cerr << "TDVP K230 wl_shm: compositor connection closed\n";
    }
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
    state->last_stats_log = std::chrono::steady_clock::now();
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
        if (state_->frame_callback != nullptr) {
            wl_callback_destroy(state_->frame_callback);
            state_->frame_callback = nullptr;
        }
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

void TdvpK230WaylandShm::present(const render::TdvpK230PresentationFrame& frame)
{
#if defined(CZ_GBA_HAS_TDVP_WAYLAND_SHM)
    if (!state_ || frame.static_pixels_xrgb8888 == nullptr || frame.static_width <= 0 ||
        frame.static_height <= 0 || frame.static_pitch_bytes <= 0) {
        return;
    }

    State& state = *state_;
    dispatch_pending_events(state);
    const bool static_changed = state.static_generation != frame.static_generation;
    if (static_changed) {
        if (!scale_static_pixels(state, frame)) {
            return;
        }
        state.full_damage_pending = true;
    }

    if (static_changed || frame.game_frame_updated) {
        state.scheduler.note_frame_available();
    }
    if (!state.scheduler.has_latest_frame()) {
        maybe_log_present_stats(state);
        return;
    }

    State::Buffer* buffer = next_available_buffer(state);
    if (state.scheduler.try_begin_present(buffer != nullptr) != TdvpK230PresentDecision::Commit) {
        maybe_log_present_stats(state);
        return;
    }
    if (buffer->static_generation != state.static_generation) {
        std::memcpy(buffer->pixels, state.static_pixels.data(), buffer->size);
        buffer->static_generation = state.static_generation;
    }
    scale_game_pixels(state, *buffer, frame);

    state.frame_callback = wl_surface_frame(state.surface);
    if (state.frame_callback == nullptr) {
        std::cerr << "TDVP K230 wl_shm: wl_surface_frame failed\n";
        state.scheduler.cancel_pending_present();
        return;
    }
    wl_callback_add_listener(state.frame_callback, &kFrameCallbackListener, &state);
    buffer->busy = true;
    wl_surface_attach(state.surface, buffer->buffer, 0, 0);
    if (state.full_damage_pending) {
        wl_surface_damage_buffer(state.surface, 0, 0, state.width, state.height);
        state.full_damage_pending = false;
    } else {
        const auto game = tdvp_k230_game_damage_rect(
            frame.static_width, frame.static_height, state.width, state.height);
        wl_surface_damage_buffer(state.surface, game.x, game.y, game.width, game.height);
    }
    wl_surface_commit(state.surface);
    state.scheduler.note_present_committed();
    if (wl_display_flush(state.display) < 0 && errno != EAGAIN) {
        log_errno("wl_display_flush");
    }
    maybe_log_present_stats(state);
#else
    (void)frame;
#endif
}

} // namespace czgba::platform
