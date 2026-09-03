#include "platform/tdvp_k230_wayland_shm.hpp"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>

#if defined(CZ_GBA_HAS_TDVP_NATIVE_WAYLAND)
#include <SDL.h>

#include "xdg-shell-client-protocol.h"

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include "input/input_mapper.hpp"
#include "platform/presentation_profile.hpp"
#include "platform/tdvp_k230_present_geometry.hpp"
#include "platform/tdvp_k230_present_scheduler.hpp"
#include "render/layout.hpp"
#endif

namespace czgba::platform {

struct TdvpK230WaylandShmState {
#if defined(CZ_GBA_HAS_TDVP_NATIVE_WAYLAND)
    struct Buffer {
        TdvpK230WaylandShmState* state = nullptr;
        int fd = -1;
        std::uint32_t* pixels = nullptr;
        std::size_t size = 0;
        wl_shm_pool* pool = nullptr;
        wl_buffer* buffer = nullptr;
        bool busy = false;
        bool game_buffer = false;
        std::uint64_t static_generation = 0;
    };

    struct KeyEvent {
        SDL_Keycode key = SDLK_UNKNOWN;
        bool pressed = false;
    };

    // This connection is deliberately not borrowed from SDL. Owning it gives
    // the emulator exactly one reader of its Wayland event queue and prevents
    // SDL's window dispatch from racing its frame callbacks or buffer releases.
    wl_display* display = nullptr;
    wl_registry* registry = nullptr;
    wl_compositor* compositor = nullptr;
    wl_subcompositor* subcompositor = nullptr;
    wl_shm* shm = nullptr;
    wl_seat* seat = nullptr;
    wl_keyboard* keyboard = nullptr;
    xdg_wm_base* wm_base = nullptr;
    wl_surface* root_surface = nullptr;
    xdg_surface* root_xdg_surface = nullptr;
    xdg_toplevel* root_toplevel = nullptr;
    wl_surface* game_surface = nullptr;
    wl_subsurface* game_subsurface = nullptr;
    wl_callback* game_frame_callback = nullptr;

    xkb_context* xkb_context_handle = nullptr;
    xkb_keymap* xkb_keymap_handle = nullptr;
    xkb_state* xkb_state_handle = nullptr;
    input::InputMapper input_mapper;
    std::vector<KeyEvent> pending_keys;
    bool release_input_pending = false;

    std::array<Buffer, 2> root_buffers{};
    std::array<Buffer, 3> game_buffers{};
    TdvpK230PresentScheduler game_scheduler;
    std::vector<std::uint32_t> static_pixels;
    int width = kK230LandscapeWidth;
    int height = kK230LandscapeHeight;
    TdvpK230DamageRect game_geometry{};
    std::uint64_t static_generation = 0;
    bool root_static_pending = false;
    bool child_mapped = false;
    bool configured = false;
    bool running = true;
    std::chrono::steady_clock::time_point last_stats_log;
    std::chrono::steady_clock::time_point last_frame_callback;
    double last_frame_callback_interval_ms = 0.0;
#endif
};

#if defined(CZ_GBA_HAS_TDVP_NATIVE_WAYLAND)
namespace {

using State = TdvpK230WaylandShmState;

void log_errno(const char* action)
{
    std::cerr << "TDVP K230 native Wayland: " << action << ": " << std::strerror(errno) << '\n';
}

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
    const auto& stats = state.game_scheduler.stats();
    std::cerr << "TDVP K230 native Wayland timing: requested=" << stats.present_requested
              << " committed=" << stats.present_committed
              << " skipped_callback=" << stats.present_skipped_frame_callback
              << " skipped_no_buffer=" << stats.present_skipped_no_buffer
              << " callbacks=" << stats.frame_callbacks
              << " releases=" << stats.buffer_releases
              << " callback_interval_ms=" << state.last_frame_callback_interval_ms
              << " root_static_pending=" << (state.root_static_pending ? "yes" : "no")
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

void buffer_release(void* data, wl_buffer*)
{
    auto& buffer = *static_cast<State::Buffer*>(data);
    buffer.busy = false;
    if (buffer.game_buffer && buffer.state != nullptr) {
        buffer.state->game_scheduler.note_buffer_release();
    }
}

constexpr wl_buffer_listener kBufferListener{
    buffer_release,
};

bool create_buffer(State& state, State::Buffer& buffer, int width, int height, bool game_buffer)
{
    if (width <= 0 || height <= 0) {
        return false;
    }
    buffer.state = &state;
    buffer.game_buffer = game_buffer;
    const int stride = width * static_cast<int>(sizeof(std::uint32_t));
    buffer.size = static_cast<std::size_t>(stride) * static_cast<std::size_t>(height);
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
        std::cerr << "TDVP K230 native Wayland: failed to create wl_shm pool\n";
        return false;
    }
    buffer.buffer = wl_shm_pool_create_buffer(
        buffer.pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
    if (buffer.buffer == nullptr) {
        std::cerr << "TDVP K230 native Wayland: failed to create wl_shm buffer\n";
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
    buffer.game_buffer = false;
    buffer.static_generation = 0;
    buffer.state = nullptr;
}

template <std::size_t Count>
State::Buffer* next_available_buffer(std::array<State::Buffer, Count>& buffers)
{
    for (auto& buffer : buffers) {
        if (!buffer.busy) {
            return &buffer;
        }
    }
    return nullptr;
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
        std::cerr << "TDVP K230 native Wayland: expected 3x integer presentation, got "
                  << destination.scale << "x\n";
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
    state.root_static_pending = true;
    return true;
}

bool scale_game_pixels(State& state, State::Buffer& buffer, const render::TdvpK230PresentationFrame& frame)
{
    using T = render::TdvpK230Layout;
    if (!frame.game_frame_updated || frame.game_pixels_xrgb8888 == nullptr ||
        frame.game_width != T::GameW || frame.game_height != T::GameH ||
        frame.game_pitch_pixels < frame.game_width ||
        state.game_geometry.width != T::GameW * 3 || state.game_geometry.height != T::GameH * 3) {
        return false;
    }

    constexpr std::uint32_t kGameBorder = (22U << 16) | (25U << 8) | 28U;
    constexpr int kScale = 3;
    for (int y = 0; y < T::GameH; ++y) {
        const auto* source = frame.game_pixels_xrgb8888 +
            static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.game_pitch_pixels);
        for (int dy = 0; dy < kScale; ++dy) {
            auto* target = buffer.pixels +
                static_cast<std::size_t>(y * kScale + dy) * static_cast<std::size_t>(state.game_geometry.width);
            for (int x = 0; x < T::GameW; ++x) {
                const bool border = x == 0 || y == 0 || x == T::GameW - 1 || y == T::GameH - 1;
                std::fill_n(target + x * kScale, kScale, border ? kGameBorder : source[x]);
            }
        }
    }
    return true;
}

void frame_callback_done(void* data, wl_callback* callback, std::uint32_t)
{
    auto& state = *static_cast<State*>(data);
    if (state.game_frame_callback == callback) {
        state.game_frame_callback = nullptr;
    }
    const auto now = std::chrono::steady_clock::now();
    if (state.last_frame_callback.time_since_epoch().count() != 0) {
        state.last_frame_callback_interval_ms =
            std::chrono::duration<double, std::milli>(now - state.last_frame_callback).count();
    }
    state.last_frame_callback = now;
    state.game_scheduler.note_frame_callback();
    wl_callback_destroy(callback);
}

constexpr wl_callback_listener kFrameCallbackListener{
    frame_callback_done,
};

SDL_Keycode sdl_keycode_from_xkb(xkb_keysym_t symbol)
{
    switch (symbol) {
    case XKB_KEY_w:
    case XKB_KEY_W: return SDLK_w;
    case XKB_KEY_s:
    case XKB_KEY_S: return SDLK_s;
    case XKB_KEY_a:
    case XKB_KEY_A: return SDLK_a;
    case XKB_KEY_d:
    case XKB_KEY_D: return SDLK_d;
    case XKB_KEY_j:
    case XKB_KEY_J: return SDLK_j;
    case XKB_KEY_k:
    case XKB_KEY_K: return SDLK_k;
    case XKB_KEY_u:
    case XKB_KEY_U: return SDLK_u;
    case XKB_KEY_i:
    case XKB_KEY_I: return SDLK_i;
    case XKB_KEY_q:
    case XKB_KEY_Q: return SDLK_q;
    case XKB_KEY_Up: return SDLK_UP;
    case XKB_KEY_Down: return SDLK_DOWN;
    case XKB_KEY_Left: return SDLK_LEFT;
    case XKB_KEY_Right: return SDLK_RIGHT;
    case XKB_KEY_Return:
    case XKB_KEY_KP_Enter: return SDLK_RETURN;
    case XKB_KEY_space: return SDLK_SPACE;
    case XKB_KEY_4: return SDLK_4;
    case XKB_KEY_5: return SDLK_5;
    case XKB_KEY_6: return SDLK_6;
    case XKB_KEY_7: return SDLK_7;
    case XKB_KEY_8: return SDLK_8;
    case XKB_KEY_F1: return SDLK_F1;
    case XKB_KEY_F2: return SDLK_F2;
    case XKB_KEY_F3: return SDLK_F3;
    case XKB_KEY_F4: return SDLK_F4;
    case XKB_KEY_F5: return SDLK_F5;
    default: return SDLK_UNKNOWN;
    }
}

void update_keymap(State& state, int fd, std::uint32_t size)
{
    if (size == 0) {
        close(fd);
        return;
    }
    auto* map = static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (map == MAP_FAILED) {
        close(fd);
        return;
    }
    if (state.xkb_context_handle == nullptr) {
        state.xkb_context_handle = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    }
    if (state.xkb_keymap_handle != nullptr) {
        xkb_keymap_unref(state.xkb_keymap_handle);
        state.xkb_keymap_handle = nullptr;
    }
    if (state.xkb_state_handle != nullptr) {
        xkb_state_unref(state.xkb_state_handle);
        state.xkb_state_handle = nullptr;
    }
    if (state.xkb_context_handle != nullptr) {
        state.xkb_keymap_handle = xkb_keymap_new_from_string(
            state.xkb_context_handle, map, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
        state.xkb_state_handle = state.xkb_keymap_handle != nullptr
            ? xkb_state_new(state.xkb_keymap_handle)
            : nullptr;
    }
    munmap(map, size);
    close(fd);
}

// Registry globals are delivered before the first roundtrip returns. Forward
// declare the seat listener so keyboard capabilities cannot be emitted before
// the client has attached its listener to the newly bound wl_seat object.
extern const wl_seat_listener kSeatListener;

void registry_global(void* data, wl_registry* registry, std::uint32_t name, const char* interface, std::uint32_t version)
{
    auto& state = *static_cast<State*>(data);
    if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
        state.compositor = static_cast<wl_compositor*>(
            wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, 4U)));
    } else if (std::strcmp(interface, wl_subcompositor_interface.name) == 0) {
        state.subcompositor = static_cast<wl_subcompositor*>(
            wl_registry_bind(registry, name, &wl_subcompositor_interface, 1));
    } else if (std::strcmp(interface, wl_shm_interface.name) == 0) {
        state.shm = static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
    } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
        state.seat = static_cast<wl_seat*>(
            wl_registry_bind(registry, name, &wl_seat_interface, std::min(version, 7U)));
        if (state.seat != nullptr) {
            wl_seat_add_listener(state.seat, &kSeatListener, &state);
        }
    } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
        state.wm_base = static_cast<xdg_wm_base*>(
            wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
    }
}

void registry_remove(void*, wl_registry*, std::uint32_t)
{
}

constexpr wl_registry_listener kRegistryListener{
    registry_global,
    registry_remove,
};

void wm_base_ping(void*, xdg_wm_base* wm_base, std::uint32_t serial)
{
    xdg_wm_base_pong(wm_base, serial);
}

constexpr xdg_wm_base_listener kWmBaseListener{
    wm_base_ping,
};

void root_surface_configure(void* data, xdg_surface* surface, std::uint32_t serial)
{
    auto& state = *static_cast<State*>(data);
    xdg_surface_ack_configure(surface, serial);
    state.configured = true;
}

constexpr xdg_surface_listener kRootSurfaceListener{
    root_surface_configure,
};

void root_toplevel_configure(void*, xdg_toplevel*, std::int32_t, std::int32_t, wl_array*)
{
}

void root_toplevel_close(void* data, xdg_toplevel*)
{
    static_cast<State*>(data)->running = false;
}

constexpr xdg_toplevel_listener kRootToplevelListener{
    root_toplevel_configure,
    root_toplevel_close,
    nullptr,
    nullptr,
};

void keyboard_keymap(void* data, wl_keyboard*, std::uint32_t format, std::int32_t fd, std::uint32_t size)
{
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }
    update_keymap(*static_cast<State*>(data), fd, size);
}

void keyboard_enter(void*, wl_keyboard*, std::uint32_t, wl_surface*, wl_array*)
{
}

void keyboard_leave(void* data, wl_keyboard*, std::uint32_t, wl_surface*)
{
    static_cast<State*>(data)->release_input_pending = true;
}

void keyboard_key(void* data, wl_keyboard*, std::uint32_t, std::uint32_t,
                  std::uint32_t key, std::uint32_t key_state)
{
    auto& state = *static_cast<State*>(data);
    if (state.xkb_state_handle == nullptr) {
        return;
    }
    const auto keycode = static_cast<xkb_keycode_t>(key + 8U);
    const auto symbol = xkb_state_key_get_one_sym(state.xkb_state_handle, keycode);
    const auto mapped = sdl_keycode_from_xkb(symbol);
    if (mapped != SDLK_UNKNOWN) {
        state.pending_keys.push_back({mapped, key_state == WL_KEYBOARD_KEY_STATE_PRESSED});
    }
}

void keyboard_modifiers(void* data, wl_keyboard*, std::uint32_t, std::uint32_t depressed,
                        std::uint32_t latched, std::uint32_t locked, std::uint32_t group)
{
    auto& state = *static_cast<State*>(data);
    if (state.xkb_state_handle != nullptr) {
        xkb_state_update_mask(state.xkb_state_handle, depressed, latched, locked, 0, 0, group);
    }
}

void keyboard_repeat_info(void*, wl_keyboard*, std::int32_t, std::int32_t)
{
}

constexpr wl_keyboard_listener kKeyboardListener{
    keyboard_keymap,
    keyboard_enter,
    keyboard_leave,
    keyboard_key,
    keyboard_modifiers,
    keyboard_repeat_info,
};

void seat_capabilities(void* data, wl_seat*, std::uint32_t capabilities)
{
    auto& state = *static_cast<State*>(data);
    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0U && state.keyboard == nullptr) {
        state.keyboard = wl_seat_get_keyboard(state.seat);
        if (state.keyboard != nullptr) {
            wl_keyboard_add_listener(state.keyboard, &kKeyboardListener, &state);
        }
    } else if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) == 0U && state.keyboard != nullptr) {
        wl_keyboard_destroy(state.keyboard);
        state.keyboard = nullptr;
        state.release_input_pending = true;
    }
}

void seat_name(void*, wl_seat*, const char*)
{
}

const wl_seat_listener kSeatListener{
    seat_capabilities,
    seat_name,
};

bool dispatch_events(State& state, int timeout_ms)
{
    if (!state.running || state.display == nullptr) {
        return false;
    }
    while (wl_display_prepare_read(state.display) != 0) {
        if (wl_display_dispatch_pending(state.display) < 0) {
            state.running = false;
            return false;
        }
    }
    const int flush_result = wl_display_flush(state.display);
    if (flush_result < 0 && errno != EAGAIN) {
        log_errno("wl_display_flush");
        wl_display_cancel_read(state.display);
        state.running = false;
        return false;
    }

    pollfd descriptor{};
    descriptor.fd = wl_display_get_fd(state.display);
    descriptor.events = POLLIN;
    // If the Wayland socket's send buffer is full, wait for writable space as
    // well as inbound callbacks.  Waiting only for POLLIN can leave a valid
    // child-surface commit buffered until unrelated input arrives.
    if (flush_result < 0) {
        descriptor.events |= POLLOUT;
    }
    const int result = poll(&descriptor, 1, std::max(0, timeout_ms));
    if (result > 0 && (descriptor.revents & POLLIN) != 0) {
        if (wl_display_read_events(state.display) < 0) {
            state.running = false;
            return false;
        }
    } else {
        wl_display_cancel_read(state.display);
    }
    if (result < 0 && errno != EINTR) {
        log_errno("poll Wayland display");
        state.running = false;
        return false;
    }
    if (result > 0 && (descriptor.revents & POLLOUT) != 0 && wl_display_flush(state.display) < 0 && errno != EAGAIN) {
        log_errno("wl_display_flush after POLLOUT");
        state.running = false;
        return false;
    }
    if (wl_display_dispatch_pending(state.display) < 0 || wl_display_get_error(state.display) != 0) {
        state.running = false;
        return false;
    }
    return true;
}

void hide_game_surface(State& state)
{
    if (!state.child_mapped) {
        return;
    }
    if (state.game_frame_callback != nullptr) {
        wl_callback_destroy(state.game_frame_callback);
        state.game_frame_callback = nullptr;
        state.game_scheduler.cancel_pending_present();
    }
    // The subsurface uses desynchronised commits. Unmapping it therefore does
    // not wait for the root surface and cannot hold a paused/menu transition
    // behind a stale game frame.
    wl_surface_attach(state.game_surface, nullptr, 0, 0);
    wl_surface_commit(state.game_surface);
    state.child_mapped = false;
}

void commit_static_root(State& state)
{
    if (!state.root_static_pending || !state.configured) {
        return;
    }
    auto* buffer = next_available_buffer(state.root_buffers);
    if (buffer == nullptr) {
        return;
    }
    std::memcpy(buffer->pixels, state.static_pixels.data(), buffer->size);
    buffer->static_generation = state.static_generation;
    buffer->busy = true;
    wl_surface_attach(state.root_surface, buffer->buffer, 0, 0);
    wl_surface_damage_buffer(state.root_surface, 0, 0, state.width, state.height);
    wl_surface_commit(state.root_surface);
    state.root_static_pending = false;
}

void destroy_state_objects(State& state)
{
    if (state.game_frame_callback != nullptr) {
        wl_callback_destroy(state.game_frame_callback);
        state.game_frame_callback = nullptr;
    }
    for (auto& buffer : state.game_buffers) {
        destroy_buffer(buffer);
    }
    for (auto& buffer : state.root_buffers) {
        destroy_buffer(buffer);
    }
    if (state.game_subsurface != nullptr) {
        wl_subsurface_destroy(state.game_subsurface);
        state.game_subsurface = nullptr;
    }
    if (state.game_surface != nullptr) {
        wl_surface_destroy(state.game_surface);
        state.game_surface = nullptr;
    }
    if (state.root_toplevel != nullptr) {
        xdg_toplevel_destroy(state.root_toplevel);
        state.root_toplevel = nullptr;
    }
    if (state.root_xdg_surface != nullptr) {
        xdg_surface_destroy(state.root_xdg_surface);
        state.root_xdg_surface = nullptr;
    }
    if (state.root_surface != nullptr) {
        wl_surface_destroy(state.root_surface);
        state.root_surface = nullptr;
    }
    if (state.xkb_state_handle != nullptr) {
        xkb_state_unref(state.xkb_state_handle);
        state.xkb_state_handle = nullptr;
    }
    if (state.xkb_keymap_handle != nullptr) {
        xkb_keymap_unref(state.xkb_keymap_handle);
        state.xkb_keymap_handle = nullptr;
    }
    if (state.xkb_context_handle != nullptr) {
        xkb_context_unref(state.xkb_context_handle);
        state.xkb_context_handle = nullptr;
    }
    if (state.keyboard != nullptr) {
        wl_keyboard_destroy(state.keyboard);
        state.keyboard = nullptr;
    }
    if (state.seat != nullptr) {
        wl_seat_destroy(state.seat);
        state.seat = nullptr;
    }
    if (state.wm_base != nullptr) {
        xdg_wm_base_destroy(state.wm_base);
        state.wm_base = nullptr;
    }
    if (state.shm != nullptr) {
        wl_shm_destroy(state.shm);
        state.shm = nullptr;
    }
    if (state.subcompositor != nullptr) {
        wl_subcompositor_destroy(state.subcompositor);
        state.subcompositor = nullptr;
    }
    if (state.compositor != nullptr) {
        wl_compositor_destroy(state.compositor);
        state.compositor = nullptr;
    }
    if (state.registry != nullptr) {
        wl_registry_destroy(state.registry);
        state.registry = nullptr;
    }
    if (state.display != nullptr) {
        wl_display_disconnect(state.display);
        state.display = nullptr;
    }
}

} // namespace
#endif

TdvpK230WaylandShm::TdvpK230WaylandShm() = default;

TdvpK230WaylandShm::~TdvpK230WaylandShm()
{
    shutdown();
}

bool TdvpK230WaylandShm::init()
{
    shutdown();

#if defined(CZ_GBA_HAS_TDVP_NATIVE_WAYLAND)
    auto state = std::make_unique<State>();
    state->display = wl_display_connect(nullptr);
    if (state->display == nullptr) {
        log_errno("wl_display_connect");
        return false;
    }
    state->registry = wl_display_get_registry(state->display);
    if (state->registry == nullptr) {
        std::cerr << "TDVP K230 native Wayland: wl_display_get_registry failed\n";
        destroy_state_objects(*state);
        return false;
    }
    wl_registry_add_listener(state->registry, &kRegistryListener, state.get());
    if (wl_display_roundtrip(state->display) < 0 || wl_display_roundtrip(state->display) < 0 ||
        state->compositor == nullptr || state->subcompositor == nullptr || state->shm == nullptr ||
        state->seat == nullptr || state->wm_base == nullptr) {
        std::cerr << "TDVP K230 native Wayland: compositor is missing a required xdg, shm, seat, or subsurface global\n";
        destroy_state_objects(*state);
        return false;
    }
    xdg_wm_base_add_listener(state->wm_base, &kWmBaseListener, state.get());

    state->root_surface = wl_compositor_create_surface(state->compositor);
    state->game_surface = wl_compositor_create_surface(state->compositor);
    if (state->root_surface == nullptr || state->game_surface == nullptr) {
        std::cerr << "TDVP K230 native Wayland: wl_compositor_create_surface failed\n";
        destroy_state_objects(*state);
        return false;
    }
    state->root_xdg_surface = xdg_wm_base_get_xdg_surface(state->wm_base, state->root_surface);
    if (state->root_xdg_surface == nullptr) {
        std::cerr << "TDVP K230 native Wayland: xdg_wm_base_get_xdg_surface failed\n";
        destroy_state_objects(*state);
        return false;
    }
    xdg_surface_add_listener(state->root_xdg_surface, &kRootSurfaceListener, state.get());
    state->root_toplevel = xdg_surface_get_toplevel(state->root_xdg_surface);
    if (state->root_toplevel == nullptr) {
        std::cerr << "TDVP K230 native Wayland: xdg_surface_get_toplevel failed\n";
        destroy_state_objects(*state);
        return false;
    }
    xdg_toplevel_add_listener(state->root_toplevel, &kRootToplevelListener, state.get());
    xdg_toplevel_set_title(state->root_toplevel, "GBA Emulator");
    xdg_toplevel_set_app_id(state->root_toplevel, "io.github.vicliu624.cardputer-zero-gba");
    // This is a standard xdg_toplevel, not a layer-shell or DRM surface. Labwc
    // continues to own the output transform, composition, and KMS page flips.
    xdg_toplevel_set_fullscreen(state->root_toplevel, nullptr);

    state->game_geometry = tdvp_k230_game_surface_rect(
        render::TdvpK230Layout::ScreenW, render::TdvpK230Layout::ScreenH, state->width, state->height);
    if (state->game_geometry.width <= 0 || state->game_geometry.height <= 0) {
        std::cerr << "TDVP K230 native Wayland: invalid game subsurface geometry\n";
        destroy_state_objects(*state);
        return false;
    }
    state->game_subsurface = wl_subcompositor_get_subsurface(
        state->subcompositor, state->game_surface, state->root_surface);
    if (state->game_subsurface == nullptr) {
        std::cerr << "TDVP K230 native Wayland: wl_subcompositor_get_subsurface failed\n";
        destroy_state_objects(*state);
        return false;
    }
    wl_subsurface_set_position(state->game_subsurface, state->game_geometry.x, state->game_geometry.y);
    // Desynchronisation is crucial: a 60Hz child commit must not wait for a
    // rare static root-surface update before Labwc can compose it.
    wl_subsurface_set_desync(state->game_subsurface);
    // A newly-created subsurface is initially stacked above its parent.
    // wl_subsurface_place_above() only accepts another *sibling* subsurface
    // as its reference, so the parent root surface must not be supplied here.

    state->static_pixels.resize(static_cast<std::size_t>(state->width) * static_cast<std::size_t>(state->height));
    for (auto& buffer : state->root_buffers) {
        if (!create_buffer(*state, buffer, state->width, state->height, false)) {
            destroy_state_objects(*state);
            return false;
        }
    }
    for (auto& buffer : state->game_buffers) {
        if (!create_buffer(*state, buffer, state->game_geometry.width, state->game_geometry.height, true)) {
            destroy_state_objects(*state);
            return false;
        }
    }

    // The initial commit requests an xdg configure. No pixels are attached
    // until the compositor acknowledges the role, which avoids an undefined
    // first-frame size or a stale transform during greeter/desktop startup.
    wl_surface_commit(state->root_surface);
    wl_surface_commit(state->game_surface);
    if (wl_display_flush(state->display) < 0 && errno != EAGAIN) {
        log_errno("initial wl_display_flush");
        destroy_state_objects(*state);
        return false;
    }
    state->last_stats_log = std::chrono::steady_clock::now();
    state_ = std::move(state);
    std::cout << "TDVP K230: using native xdg_toplevel with a desynchronised game wl_subsurface\n";
    return true;
#else
    std::cerr << "TDVP K230 native Wayland: this build lacks the required Wayland xdg/xkb client ABI\n";
    return false;
#endif
}

void TdvpK230WaylandShm::shutdown()
{
#if defined(CZ_GBA_HAS_TDVP_NATIVE_WAYLAND)
    if (state_) {
        destroy_state_objects(*state_);
    }
#endif
    state_.reset();
}

void TdvpK230WaylandShm::poll_events(input::InputFrame& input)
{
    input.begin_frame();
#if defined(CZ_GBA_HAS_TDVP_NATIVE_WAYLAND)
    if (!state_) {
        input.quit_requested = true;
        return;
    }
    State& state = *state_;
    dispatch_events(state, 0);
    if (state.release_input_pending) {
        state.input_mapper.release_all(input);
        state.release_input_pending = false;
    }
    for (const auto& event : state.pending_keys) {
        state.input_mapper.handle_key(event.key, event.pressed, input);
    }
    state.pending_keys.clear();
    input.gba = state.input_mapper.gba_state();
    if (!state.running) {
        input.quit_requested = true;
    }
#else
    input.quit_requested = true;
#endif
}

void TdvpK230WaylandShm::wait_until(std::chrono::steady_clock::time_point deadline)
{
#if defined(CZ_GBA_HAS_TDVP_NATIVE_WAYLAND)
    if (!state_) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    const auto remaining = deadline > now ? deadline - now : std::chrono::steady_clock::duration::zero();
    const auto milliseconds = std::chrono::duration<double, std::milli>(remaining).count();
    const int timeout_ms = static_cast<int>(std::clamp(std::ceil(milliseconds), 0.0, 1000.0));
    dispatch_events(*state_, timeout_ms);
#else
    (void)deadline;
#endif
}

void TdvpK230WaylandShm::present(const render::TdvpK230PresentationFrame& frame)
{
#if defined(CZ_GBA_HAS_TDVP_NATIVE_WAYLAND)
    if (!state_ || !state_->running || !state_->configured || frame.static_pixels_xrgb8888 == nullptr ||
        frame.static_width <= 0 || frame.static_height <= 0 || frame.static_pitch_bytes <= 0) {
        return;
    }
    State& state = *state_;
    const bool static_changed = state.static_generation != frame.static_generation;
    if (static_changed && !scale_static_pixels(state, frame)) {
        return;
    }

    const bool has_game_frame = frame.game_frame_updated && frame.game_pixels_xrgb8888 != nullptr &&
        frame.game_width > 0 && frame.game_height > 0 && frame.game_pitch_pixels >= frame.game_width;
    if (!has_game_frame) {
        hide_game_surface(state);
        commit_static_root(state);
        if (wl_display_flush(state.display) < 0 && errno != EAGAIN) {
            log_errno("wl_display_flush");
            state.running = false;
        }
        maybe_log_present_stats(state);
        return;
    }

    // Root chrome changes are intentionally independent of the fast game
    // path. A busy root buffer only delays a toast/rail update; it never holds
    // mGBA, PulseAudio, keyboard handling, or a free game child buffer.
    commit_static_root(state);

    state.game_scheduler.note_frame_available();
    auto* buffer = next_available_buffer(state.game_buffers);
    if (state.game_scheduler.try_begin_present(buffer != nullptr) != TdvpK230PresentDecision::Commit) {
        maybe_log_present_stats(state);
        return;
    }
    if (!scale_game_pixels(state, *buffer, frame)) {
        state.game_scheduler.cancel_pending_present();
        return;
    }
    state.game_frame_callback = wl_surface_frame(state.game_surface);
    if (state.game_frame_callback == nullptr) {
        std::cerr << "TDVP K230 native Wayland: wl_surface_frame failed\n";
        state.game_scheduler.cancel_pending_present();
        return;
    }
    wl_callback_add_listener(state.game_frame_callback, &kFrameCallbackListener, &state);
    buffer->busy = true;
    wl_surface_attach(state.game_surface, buffer->buffer, 0, 0);
    // Child buffer coordinates are exactly 720x480. It is the only surface
    // damaged during normal game play; no 1232x568 root upload is induced.
    wl_surface_damage_buffer(
        state.game_surface, 0, 0, state.game_geometry.width, state.game_geometry.height);
    wl_surface_commit(state.game_surface);
    state.child_mapped = true;
    state.game_scheduler.note_present_committed();
    if (wl_display_flush(state.display) < 0 && errno != EAGAIN) {
        log_errno("wl_display_flush");
        state.running = false;
    }
    maybe_log_present_stats(state);
#else
    (void)frame;
#endif
}

bool TdvpK230WaylandShm::should_quit() const
{
#if defined(CZ_GBA_HAS_TDVP_NATIVE_WAYLAND)
    return !state_ || !state_->running;
#else
    return true;
#endif
}

} // namespace czgba::platform
