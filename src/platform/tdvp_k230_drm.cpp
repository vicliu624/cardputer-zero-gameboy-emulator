#include "platform/tdvp_k230_drm.hpp"

#include <SDL.h>

#include <cerrno>
#include <cstring>
#include <iostream>

#if defined(__linux__) && defined(CZ_GBA_HAS_DRM_KMS)
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <drm/drm.h>
#include <drm/drm_mode.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "platform/presentation_profile.hpp"
#endif

namespace czgba::platform {

struct TdvpK230DrmState {
#if defined(__linux__) && defined(CZ_GBA_HAS_DRM_KMS)
    int drm_fd = -1;
    std::vector<int> input_fds;
    std::uint32_t connector_id = 0;
    std::uint32_t crtc_id = 0;
    std::uint32_t framebuffer_id = 0;
    std::uint32_t dumb_handle = 0;
    std::uint32_t pitch_bytes = 0;
    std::uint64_t dumb_size = 0;
    std::uint8_t* framebuffer = nullptr;
    drm_mode_modeinfo mode{};
    drm_mode_crtc original_crtc{};
    std::vector<std::uint32_t> original_connectors;
    bool original_crtc_valid = false;
#endif
};

#if defined(__linux__) && defined(CZ_GBA_HAS_DRM_KMS)
namespace {

// Connector status is exposed as a raw u32 by the kernel UAPI. libdrm gives
// this value a DRM_MODE_CONNECTED alias, but this Buildroot SDK intentionally
// ships only the kernel DRM headers. Linux defines connected as value 1.
constexpr std::uint32_t kConnectorConnected = 1;

void log_errno(const char* action)
{
    std::cerr << "TDVP K230 DRM: " << action << ": " << std::strerror(errno) << '\n';
}

template <typename T>
std::uint64_t user_pointer(const T* value)
{
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(value));
}

SDL_Keycode to_sdl_key(std::uint16_t code)
{
    switch (code) {
    case KEY_W: return SDLK_w;
    case KEY_A: return SDLK_a;
    case KEY_S: return SDLK_s;
    case KEY_D: return SDLK_d;
    case KEY_J: return SDLK_j;
    case KEY_K: return SDLK_k;
    case KEY_U: return SDLK_u;
    case KEY_I: return SDLK_i;
    case KEY_Q: return SDLK_q;
    case KEY_UP: return SDLK_UP;
    case KEY_DOWN: return SDLK_DOWN;
    case KEY_LEFT: return SDLK_LEFT;
    case KEY_RIGHT: return SDLK_RIGHT;
    case KEY_ENTER: return SDLK_RETURN;
    case KEY_SPACE: return SDLK_SPACE;
    case KEY_4: return SDLK_4;
    case KEY_5: return SDLK_5;
    case KEY_6: return SDLK_6;
    case KEY_7: return SDLK_7;
    case KEY_8: return SDLK_8;
    case KEY_F1: return SDLK_F1;
    case KEY_F2: return SDLK_F2;
    case KEY_F3: return SDLK_F3;
    case KEY_F4: return SDLK_F4;
    case KEY_F5: return SDLK_F5;
    default: return SDLK_UNKNOWN;
    }
}

bool is_target_mode(const drm_mode_modeinfo& mode)
{
    return k230_scanout_transform(mode.hdisplay, mode.vdisplay) != K230ScanoutTransform::Unsupported;
}

struct ConnectorInfo {
    drm_mode_get_connector raw{};
    std::vector<std::uint32_t> encoders;
    std::vector<drm_mode_modeinfo> modes;
    std::vector<std::uint32_t> properties;
    std::vector<std::uint64_t> property_values;
};

bool get_connector(int drm_fd, std::uint32_t connector_id, ConnectorInfo& info)
{
    drm_mode_modeinfo mode_probe{};
    info.raw.connector_id = connector_id;
    info.raw.count_modes = 1;
    info.raw.modes_ptr = user_pointer(&mode_probe);
    if (ioctl(drm_fd, DRM_IOCTL_MODE_GETCONNECTOR, &info.raw) != 0) {
        return false;
    }

    info.encoders.resize(info.raw.count_encoders);
    info.modes.resize(info.raw.count_modes);
    info.properties.resize(info.raw.count_props);
    info.property_values.resize(info.raw.count_props);
    info.raw.encoders_ptr = user_pointer(info.encoders.data());
    info.raw.modes_ptr = user_pointer(info.modes.data());
    info.raw.props_ptr = user_pointer(info.properties.data());
    info.raw.prop_values_ptr = user_pointer(info.property_values.data());
    if (ioctl(drm_fd, DRM_IOCTL_MODE_GETCONNECTOR, &info.raw) != 0) {
        return false;
    }
    return true;
}

bool get_encoder(int drm_fd, std::uint32_t encoder_id, drm_mode_get_encoder& encoder)
{
    encoder = {};
    encoder.encoder_id = encoder_id;
    return ioctl(drm_fd, DRM_IOCTL_MODE_GETENCODER, &encoder) == 0;
}

bool get_resources(int drm_fd, drm_mode_card_res& resources, std::vector<std::uint32_t>& connector_ids, std::vector<std::uint32_t>& crtc_ids)
{
    resources = {};
    if (ioctl(drm_fd, DRM_IOCTL_MODE_GETRESOURCES, &resources) != 0) {
        log_errno("DRM_IOCTL_MODE_GETRESOURCES");
        return false;
    }

    connector_ids.resize(resources.count_connectors);
    crtc_ids.resize(resources.count_crtcs);
    std::vector<std::uint32_t> framebuffer_ids(resources.count_fbs);
    std::vector<std::uint32_t> encoder_ids(resources.count_encoders);
    resources.connector_id_ptr = user_pointer(connector_ids.data());
    resources.crtc_id_ptr = user_pointer(crtc_ids.data());
    resources.fb_id_ptr = user_pointer(framebuffer_ids.data());
    resources.encoder_id_ptr = user_pointer(encoder_ids.data());
    if (ioctl(drm_fd, DRM_IOCTL_MODE_GETRESOURCES, &resources) != 0) {
        log_errno("DRM_IOCTL_MODE_GETRESOURCES data");
        return false;
    }
    return true;
}

std::vector<std::uint32_t> connectors_using_crtc(int drm_fd, const std::vector<std::uint32_t>& connector_ids, std::uint32_t crtc_id)
{
    std::vector<std::uint32_t> result;
    for (const auto connector_id : connector_ids) {
        ConnectorInfo connector{};
        if (!get_connector(drm_fd, connector_id, connector) || connector.raw.encoder_id == 0) {
            continue;
        }
        drm_mode_get_encoder encoder{};
        if (get_encoder(drm_fd, connector.raw.encoder_id, encoder) && encoder.crtc_id == crtc_id) {
            result.push_back(connector_id);
        }
    }
    return result;
}

bool choose_output(TdvpK230DrmState& state, bool report_missing_output)
{
    drm_mode_card_res resources{};
    std::vector<std::uint32_t> connector_ids;
    std::vector<std::uint32_t> crtc_ids;
    if (!get_resources(state.drm_fd, resources, connector_ids, crtc_ids)) {
        return false;
    }

    for (const auto connector_id : connector_ids) {
        ConnectorInfo connector{};
        if (!get_connector(state.drm_fd, connector_id, connector) || connector.raw.connection != kConnectorConnected) {
            continue;
        }

        const auto mode = std::find_if(connector.modes.begin(), connector.modes.end(), is_target_mode);
        if (mode == connector.modes.end()) {
            continue;
        }

        std::vector<std::uint32_t> encoder_ids;
        if (connector.raw.encoder_id != 0) {
            encoder_ids.push_back(connector.raw.encoder_id);
        }
        for (const auto encoder_id : connector.encoders) {
            if (encoder_id != 0 && std::find(encoder_ids.begin(), encoder_ids.end(), encoder_id) == encoder_ids.end()) {
                encoder_ids.push_back(encoder_id);
            }
        }

        for (const auto encoder_id : encoder_ids) {
            drm_mode_get_encoder encoder{};
            if (!get_encoder(state.drm_fd, encoder_id, encoder)) {
                continue;
            }
            for (std::size_t crtc_index = 0; crtc_index < crtc_ids.size(); ++crtc_index) {
                if ((encoder.possible_crtcs & (1U << crtc_index)) == 0) {
                    continue;
                }
                state.connector_id = connector.raw.connector_id;
                state.crtc_id = crtc_ids[crtc_index];
                state.mode = *mode;
                state.original_connectors = connectors_using_crtc(state.drm_fd, connector_ids, state.crtc_id);
                return true;
            }
        }
    }

    if (report_missing_output) {
        std::cerr << "TDVP K230 DRM: no connected 568x1232 native or 1232x568 landscape KMS output was found\n";
    }
    return false;
}

std::vector<std::string> drm_card_paths()
{
    std::vector<std::string> result;
    DIR* directory = opendir("/dev/dri");
    if (directory == nullptr) {
        return {"/dev/dri/card0"};
    }

    while (const dirent* entry = readdir(directory)) {
        const std::string name(entry->d_name);
        if (!name.starts_with("card") || name.size() == 4 ||
            !std::all_of(name.begin() + 4, name.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
            continue;
        }
        result.push_back("/dev/dri/" + name);
    }
    closedir(directory);

    std::sort(result.begin(), result.end());
    if (result.empty()) {
        result.push_back("/dev/dri/card0");
    }
    return result;
}

bool create_framebuffer(TdvpK230DrmState& state)
{
    drm_mode_create_dumb create{};
    create.width = state.mode.hdisplay;
    create.height = state.mode.vdisplay;
    create.bpp = 32;
    if (ioctl(state.drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) != 0) {
        log_errno("DRM_IOCTL_MODE_CREATE_DUMB");
        return false;
    }
    state.dumb_handle = create.handle;
    state.pitch_bytes = create.pitch;
    state.dumb_size = create.size;

    drm_mode_fb_cmd framebuffer{};
    framebuffer.width = create.width;
    framebuffer.height = create.height;
    framebuffer.pitch = create.pitch;
    framebuffer.bpp = 32;
    framebuffer.depth = 24;
    framebuffer.handle = create.handle;
    if (ioctl(state.drm_fd, DRM_IOCTL_MODE_ADDFB, &framebuffer) != 0) {
        log_errno("DRM_IOCTL_MODE_ADDFB");
        return false;
    }
    state.framebuffer_id = framebuffer.fb_id;

    drm_mode_map_dumb map{};
    map.handle = create.handle;
    if (ioctl(state.drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map) != 0) {
        log_errno("DRM_IOCTL_MODE_MAP_DUMB");
        return false;
    }

    void* mapping = mmap(nullptr, create.size, PROT_READ | PROT_WRITE, MAP_SHARED, state.drm_fd, map.offset);
    if (mapping == MAP_FAILED) {
        state.framebuffer = nullptr;
        log_errno("mmap DRM dumb buffer");
        return false;
    }
    state.framebuffer = static_cast<std::uint8_t*>(mapping);
    std::memset(state.framebuffer, 0, static_cast<std::size_t>(state.dumb_size));
    return true;
}

bool set_crtc(
    int drm_fd,
    std::uint32_t crtc_id,
    std::uint32_t framebuffer_id,
    std::uint32_t x,
    std::uint32_t y,
    const std::vector<std::uint32_t>& connector_ids,
    const drm_mode_modeinfo& mode,
    bool mode_valid)
{
    drm_mode_crtc request{};
    request.set_connectors_ptr = user_pointer(connector_ids.data());
    request.count_connectors = static_cast<std::uint32_t>(connector_ids.size());
    request.crtc_id = crtc_id;
    request.fb_id = framebuffer_id;
    request.x = x;
    request.y = y;
    request.mode_valid = mode_valid ? 1U : 0U;
    request.mode = mode;
    return ioctl(drm_fd, DRM_IOCTL_MODE_SETCRTC, &request) == 0;
}

void open_input_devices(TdvpK230DrmState& state)
{
    DIR* directory = opendir("/dev/input");
    if (directory == nullptr) {
        log_errno("opendir /dev/input");
        return;
    }

    while (const dirent* entry = readdir(directory)) {
        const std::string name(entry->d_name);
        if (!name.starts_with("event")) {
            continue;
        }
        const std::string path = "/dev/input/" + name;
        const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd >= 0) {
            state.input_fds.push_back(fd);
        }
    }
    closedir(directory);
}

} // namespace
#endif

TdvpK230Drm::TdvpK230Drm() = default;

TdvpK230Drm::~TdvpK230Drm()
{
    shutdown();
}

bool TdvpK230Drm::init()
{
    shutdown();

#if defined(__linux__) && defined(CZ_GBA_HAS_DRM_KMS)
    auto candidate = std::make_unique<TdvpK230DrmState>();
    std::string selected_card;
    for (const auto& card_path : drm_card_paths()) {
        candidate->drm_fd = open(card_path.c_str(), O_RDWR | O_CLOEXEC);
        if (candidate->drm_fd < 0) {
            continue;
        }
        if (choose_output(*candidate, false)) {
            selected_card = card_path;
            break;
        }
        close(candidate->drm_fd);
        candidate->drm_fd = -1;
    }
    if (candidate->drm_fd < 0) {
        std::cerr << "TDVP K230 DRM: no /dev/dri/cardN node exposes a supported connected KMS output\n";
        return false;
    }

    state_ = std::move(candidate);
    TdvpK230DrmState& state = *state_;

    state.original_crtc.crtc_id = state.crtc_id;
    if (ioctl(state.drm_fd, DRM_IOCTL_MODE_GETCRTC, &state.original_crtc) != 0) {
        log_errno("DRM_IOCTL_MODE_GETCRTC");
        shutdown();
        return false;
    }
    state.original_crtc_valid = true;
    if (!create_framebuffer(state)) {
        shutdown();
        return false;
    }

    const std::vector<std::uint32_t> output_connector{state.connector_id};
    if (!set_crtc(state.drm_fd, state.crtc_id, state.framebuffer_id, 0, 0, output_connector, state.mode, true)) {
        log_errno("DRM_IOCTL_MODE_SETCRTC");
        shutdown();
        return false;
    }

    open_input_devices(state);
    const auto transform = k230_scanout_transform(state.mode.hdisplay, state.mode.vdisplay);
    std::cout << "TDVP K230 DRM: " << state.mode.hdisplay << 'x' << state.mode.vdisplay
              << (transform == K230ScanoutTransform::RotateCounterClockwise
                      ? " native portrait scanout active (landscape content rotated CCW) on "
                      : " landscape scanout active on ")
              << selected_card << '\n';
    return true;
#else
    std::cerr << "TDVP K230 DRM: this build has no Linux DRM/KMS UAPI support\n";
    return false;
#endif
}

void TdvpK230Drm::shutdown()
{
    if (!state_) {
        return;
    }

#if defined(__linux__) && defined(CZ_GBA_HAS_DRM_KMS)
    TdvpK230DrmState& state = *state_;
    for (const int fd : state.input_fds) {
        close(fd);
    }
    state.input_fds.clear();

    if (state.drm_fd >= 0 && state.original_crtc_valid) {
        if (state.original_crtc.mode_valid != 0) {
            const auto& connectors = state.original_connectors.empty()
                ? std::vector<std::uint32_t>{state.connector_id}
                : state.original_connectors;
            if (!set_crtc(
                    state.drm_fd,
                    state.original_crtc.crtc_id,
                    state.original_crtc.fb_id,
                    state.original_crtc.x,
                    state.original_crtc.y,
                    connectors,
                    state.original_crtc.mode,
                    true)) {
                log_errno("restore DRM_IOCTL_MODE_SETCRTC");
            }
        } else {
            const std::vector<std::uint32_t> no_connectors;
            const drm_mode_modeinfo no_mode{};
            if (!set_crtc(
                    state.drm_fd,
                    state.original_crtc.crtc_id,
                    0,
                    0,
                    0,
                    no_connectors,
                    no_mode,
                    false)) {
                log_errno("disable inactive DRM CRTC during restore");
            }
        }
    }
    if (state.framebuffer != nullptr) {
        munmap(state.framebuffer, static_cast<std::size_t>(state.dumb_size));
        state.framebuffer = nullptr;
    }
    if (state.drm_fd >= 0 && state.framebuffer_id != 0) {
        ioctl(state.drm_fd, DRM_IOCTL_MODE_RMFB, &state.framebuffer_id);
        state.framebuffer_id = 0;
    }
    if (state.drm_fd >= 0 && state.dumb_handle != 0) {
        drm_mode_destroy_dumb destroy{};
        destroy.handle = state.dumb_handle;
        ioctl(state.drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
        state.dumb_handle = 0;
    }
    if (state.drm_fd >= 0) {
        close(state.drm_fd);
        state.drm_fd = -1;
    }
#endif

    state_.reset();
    should_quit_ = false;
}

void TdvpK230Drm::poll_events(input::InputFrame& input)
{
    input.begin_frame();
    input.gba = input_mapper_.gba_state();

#if defined(__linux__) && defined(CZ_GBA_HAS_DRM_KMS)
    if (!state_) {
        return;
    }
    for (const int fd : state_->input_fds) {
        input_event event{};
        while (read(fd, &event, sizeof(event)) == static_cast<ssize_t>(sizeof(event))) {
            if (event.type != EV_KEY || event.value == 2) {
                continue;
            }
            const SDL_Keycode key = to_sdl_key(event.code);
            if (key == SDLK_UNKNOWN) {
                continue;
            }
            const bool pressed = event.value != 0;
            input_mapper_.handle_key(key, pressed, input);
            if (pressed && key == SDLK_q) {
                should_quit_ = true;
                input.quit_requested = true;
            }
        }
    }
#endif
}

void TdvpK230Drm::present(const std::uint32_t* canvas_xrgb8888, int canvas_width, int canvas_height, int pitch_bytes)
{
#if defined(__linux__) && defined(CZ_GBA_HAS_DRM_KMS)
    if (!state_ || state_->framebuffer == nullptr || canvas_xrgb8888 == nullptr ||
        canvas_width <= 0 || canvas_height <= 0 || pitch_bytes <= 0) {
        return;
    }

    TdvpK230DrmState& state = *state_;
    std::memset(state.framebuffer, 0, static_cast<std::size_t>(state.dumb_size));
    const auto transform = k230_scanout_transform(state.mode.hdisplay, state.mode.vdisplay);
    if (transform == K230ScanoutTransform::Unsupported) {
        return;
    }

    // The application always lays out the K230 UI in its physical landscape
    // coordinates. On the actual 568x1232 KMS scanout, each scaled pixel is
    // then rotated into the panel-native buffer below.
    const auto destination = integer_presentation_rect(
        canvas_width,
        canvas_height,
        kK230LandscapeWidth,
        kK230LandscapeHeight);
    if (destination.scale <= 0 || destination.width <= 0 || destination.height <= 0) {
        return;
    }

    const int source_stride = pitch_bytes / static_cast<int>(sizeof(std::uint32_t));
    if (source_stride < canvas_width || state.pitch_bytes < state.mode.hdisplay * sizeof(std::uint32_t)) {
        return;
    }
    const int destination_stride = static_cast<int>(state.pitch_bytes / sizeof(std::uint32_t));
    auto* destination_pixels = reinterpret_cast<std::uint32_t*>(state.framebuffer);
    for (int source_y = 0; source_y < canvas_height; ++source_y) {
        const auto* source_row = canvas_xrgb8888 + source_y * source_stride;
        for (int vertical = 0; vertical < destination.scale; ++vertical) {
            const int landscape_y = destination.y + source_y * destination.scale + vertical;
            for (int source_x = 0; source_x < canvas_width; ++source_x) {
                for (int horizontal = 0; horizontal < destination.scale; ++horizontal) {
                    const int landscape_x = destination.x + source_x * destination.scale + horizontal;
                    const auto scanout = k230_landscape_to_scanout(
                        landscape_x,
                        landscape_y,
                        state.mode.hdisplay,
                        state.mode.vdisplay);
                    destination_pixels[scanout.y * destination_stride + scanout.x] = source_row[source_x];
                }
            }
        }
    }
#else
    (void)canvas_xrgb8888;
    (void)canvas_width;
    (void)canvas_height;
    (void)pitch_bytes;
#endif
}

bool TdvpK230Drm::should_quit() const
{
    return should_quit_;
}

} // namespace czgba::platform
