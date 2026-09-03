#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#ifndef CZ_GBA_SOURCE_ROOT
#define CZ_GBA_SOURCE_ROOT "."
#endif

namespace {

std::string read_file(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.good()) {
        std::cerr << "failed to read " << path << '\n';
        std::exit(1);
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    auto contents = buffer.str();
    // The feed is also built from Windows worktrees through WSL. Normalize
    // checked-out CRLF source text so source-contract assertions describe C++
    // semantics rather than the host Git end-of-line policy.
    contents.erase(std::remove(contents.begin(), contents.end(), '\r'), contents.end());
    return contents;
}

bool contains(const std::string& haystack, const std::string& needle)
{
    return haystack.find(needle) != std::string::npos;
}

void require(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "packaging contract failed: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main()
{
    const std::filesystem::path root = CZ_GBA_SOURCE_ROOT;

    const auto desktop = read_file(root / "desktop" / "cardputer-zero-gba.desktop");
    require(contains(desktop, "Name=GBE"), "desktop name");
    require(contains(desktop, "Exec=/usr/lib/cardputer-zero-gba/cardputer-zero-gba-applaunch"), "desktop exec");
    require(contains(desktop, "Icon=share/images/cardputer-zero-gba.png"), "desktop icon");
    require(contains(desktop, "X-Zero-AppId=io.github.vicliu624.cardputer-zero-gba"), "desktop app id");
    require(contains(desktop, "X-Zero-Display=wayland"), "desktop display backend");

    const auto launcher = read_file(root / "packaging" / "cardputer-zero-gba-applaunch");
    require(contains(launcher, "CARDPUTER_ZERO_GBA_DISPLAY_BACKEND"), "launcher display env");
    require(contains(launcher, "SDL_VIDEODRIVER:=wayland"), "launcher wayland default");
    require(contains(launcher, "SDL_VIDEO_WAYLAND_WMCLASS=io.github.vicliu624.cardputer-zero-gba"), "launcher wmclass");
    require(contains(launcher, "--kiosk"), "launcher kiosk flag");
    require(contains(launcher, "/usr/lib/cardputer-zero-gba"), "launcher private libdir");

    const auto tdvp_launcher = read_file(root / "packaging" / "tdvp-k230" / "cardputer-zero-gba");
    require(contains(tdvp_launcher, "/opt/tdvp-cardputer-zero-gba"), "TDVP private runtime root");
    require(contains(tdvp_launcher, "--device-profile tdvp-k230"), "TDVP device profile launcher option");
    require(contains(tdvp_launcher, "DRM/KMS"), "TDVP Wayland client preserves the DRM/KMS ownership contract");
    require(contains(tdvp_launcher, "SDL_VIDEODRIVER=wayland"), "TDVP launcher selects the compositor client backend");
    require(!contains(tdvp_launcher, "SDL_VIDEODRIVER=fbdev"), "TDVP launcher must not select fbdev");
    require(contains(tdvp_launcher, "cd -- \"$HOME\""), "TDVP launcher starts relative ROM discovery from the user home");
    const auto tdvp_desktop = read_file(root / "packaging" / "tdvp-k230" / "tdvp-cardputer-zero-gba.desktop");
    require(contains(tdvp_desktop, "Exec=/usr/bin/cardputer-zero-gba"), "TDVP desktop launch path");
    require(contains(tdvp_desktop, "Icon=/usr/share/icons/hicolor/128x128/apps/cardputer-zero-gba.png"),
            "TDVP desktop uses its package-owned absolute icon path");
    require(contains(tdvp_desktop, "Categories=Game;Emulator;"), "TDVP desktop category");

    const auto tdvp_gba_launcher = read_file(root / "packaging" / "tdvp-k230" / "tdvp-gba");
    require(contains(tdvp_gba_launcher, "/opt/tdvp-gba"), "new TDVP runtime root");
    require(contains(tdvp_gba_launcher, "--device-profile tdvp-k230"), "new TDVP device profile launcher option");
    require(contains(tdvp_gba_launcher, "SDL_VIDEODRIVER=wayland"), "new TDVP launcher selects the compositor client backend");
    require(!contains(tdvp_gba_launcher, "SDL_VIDEODRIVER=fbdev"), "new TDVP launcher must not select fbdev");
    const auto tdvp_gba_desktop = read_file(root / "packaging" / "tdvp-k230" / "tdvp-gba.desktop");
    require(contains(tdvp_gba_desktop, "Exec=/usr/bin/tdvp-gba"), "new TDVP desktop launch path");
    require(contains(tdvp_gba_desktop, "Icon=/usr/share/icons/hicolor/128x128/apps/tdvp-gba.png"),
            "new TDVP desktop uses its package-owned absolute icon path");

    const auto sdl_audio = read_file(root / "src" / "platform" / "sdl_audio.cpp");
    require(contains(sdl_audio, "desired.callback = nullptr"), "queued audio device");
    require(contains(sdl_audio, "SDL_QueueAudio"), "queued audio write path");
    require(contains(sdl_audio, "SDL_GetQueuedAudioSize"), "queued audio size check");
    require(contains(sdl_audio, "SDL_GetCurrentAudioDriver"), "audio startup reports the selected runtime driver");
    require(contains(sdl_audio, "start_buffer_samples_"), "audio prebuffer");
    require(contains(sdl_audio, "playback_started_"), "audio playback start state");
    require(contains(sdl_audio, "queue_limit_samples_"), "audio queue limit");
    require(!contains(sdl_audio, "audio_callback"), "audio callback must not be used");
    require(!contains(sdl_audio, "SDL_LockAudioDevice"), "audio callback lock must not be used");

    const auto mgba_core = read_file(root / "src" / "core" / "mgba_core.cpp");
    require(contains(mgba_core, "core_->reset(core_);\n    configure_audio(audio_sample_rate_);"), "audio configured after reset");
    require(contains(mgba_core, "blip_samples_avail(right)"), "audio reads both channels");

    const auto input_mapper = read_file(root / "src" / "input" / "input_mapper.cpp");
    require(contains(input_mapper, "case SDLK_4:"), "key 4 mapping");
    require(contains(input_mapper, "case SDLK_5:"), "key 5 mapping");
    require(contains(input_mapper, "case SDLK_6:"), "key 6 mapping");
    require(contains(input_mapper, "case SDLK_7:"), "key 7 mapping");
    require(contains(input_mapper, "case SDLK_8:"), "key 8 mapping");
    require(contains(input_mapper, "case SDLK_F1:"), "TDVP K230 F1 menu mapping");
    require(contains(input_mapper, "case SDLK_F2:"), "TDVP K230 F2 save mapping");
    require(contains(input_mapper, "case SDLK_F3:"), "TDVP K230 F3 load mapping");
    require(contains(input_mapper, "case SDLK_F4:"), "TDVP K230 F4 fast mapping");
    require(contains(input_mapper, "case SDLK_F5:"), "TDVP K230 F5 cheats mapping");
    require(!contains(input_mapper, "case SDLK_ESCAPE:"), "old ESC shortcut removed");
    require(!contains(input_mapper, "case SDLK_1:"), "old key 1 shortcut removed");
    require(!contains(input_mapper, "case SDLK_2:"), "old key 2 shortcut removed");
    require(!contains(input_mapper, "case SDLK_f:"), "old F shortcut removed");
    require(!contains(input_mapper, "case SDLK_c:"), "old C shortcut removed");

    const auto command_bar = read_file(root / "src" / "app" / "command_bar.cpp");
    require(contains(command_bar, "MENU|SAVE|LOAD|FAST|CHEATS"), "command bar labels");

    const auto app = read_file(root / "src" / "app" / "app.cpp");
    require(contains(app, "batch.interleaved_s16.clear()"), "FAST audio drain without queueing compressed audio");

    const auto renderer = read_file(root / "src" / "render" / "renderer.cpp");
    const auto render_layout = read_file(root / "src" / "render" / "layout.hpp");
    const auto sdl_platform = read_file(root / "src" / "platform" / "sdl_platform.cpp");
    const auto tdvp_shm = read_file(root / "src" / "platform" / "tdvp_k230_wayland_shm.cpp");
    require(contains(sdl_platform, "SDL_RENDERER_ACCELERATED"), "Cardputer Zero keeps its SDL renderer");
    require(contains(sdl_platform, "PresentationProfile::TdvpK230"), "TDVP presentation profile");
    require(contains(sdl_platform, "tdvp_direct_drm_requested"), "TDVP direct DRM is opt-in only");
    require(contains(sdl_platform, "TdvpK230WaylandShm"), "TDVP defaults to direct wl_shm presentation");
    require(contains(sdl_platform, "refusing software GLES fallback"), "TDVP refuses software GLES fallback");
    require(!contains(sdl_platform, "SDL_RENDERER_PRESENTVSYNC"), "renderer vsync removed");
    require(contains(tdvp_shm, "wl_shm_create_pool"), "TDVP creates direct Wayland shared-memory pools");
    require(contains(tdvp_shm, "wl_surface_attach"), "TDVP attaches shared-memory buffers directly to Wayland");
    require(contains(tdvp_shm, "wl_surface_frame"), "TDVP paces presentation with compositor frame callbacks");
    require(contains(tdvp_shm, "wl_surface_damage_buffer"), "TDVP uses buffer-space damage for wl_shm presentation");
    require(!contains(tdvp_shm, "while (wl_display_dispatch("),
            "TDVP never blocks emulation waiting for a compositor-owned buffer");
    require(!contains(tdvp_shm, "SDL_GetWindowWMInfo"),
            "TDVP native client does not borrow SDL's Wayland display or surface");
    require(contains(tdvp_shm, "wl_display_connect(nullptr)"),
            "TDVP owns its native Wayland display connection");
    require(contains(tdvp_shm, "xdg_wm_base_get_xdg_surface"),
            "TDVP uses a standard xdg toplevel root surface");
    require(contains(tdvp_shm, "wl_subcompositor_get_subsurface"),
            "TDVP puts the dynamic GBA viewport in a child subsurface");
    require(contains(tdvp_shm, "wl_subsurface_set_desync"),
            "TDVP child surface does not wait for static root commits");
    require(!contains(tdvp_shm, "wl_subsurface_place_above(state.game_subsurface"),
            "TDVP never passes its parent surface where Wayland requires a sibling subsurface");
    require(contains(tdvp_shm, "state.game_surface, 0, 0, state.game_geometry.width"),
            "normal game damage is child-local rather than full-output damage");
    require(contains(tdvp_shm, "xkb_state_key_get_one_sym"),
            "TDVP owns keyboard translation through the Wayland seat");
    require(contains(tdvp_shm, "dispatch_events(*state_, timeout_ms)"),
            "TDVP waits for compositor/input events only until the emulation deadline");
    require(contains(tdvp_shm, "descriptor.events |= POLLOUT"),
            "TDVP drains a back-pressured Wayland socket without waiting for unrelated input");
    require(contains(tdvp_shm, "cancel_pending_present"),
            "TDVP retains the latest frame when a presentation reservation fails");
    require(contains(tdvp_shm, "scale_game_pixels"), "TDVP does dedicated nearest-neighbour game scaling");
    require(contains(tdvp_shm, "static_generation"), "TDVP caches static scaled UI buffers");
    require(contains(renderer, "TdvpK230PresentationFrame"),
            "renderer exposes separate static chrome and dynamic GBA presentation data");
    require(contains(renderer, "{0, 79}"), "bottom slot 1");
    require(contains(renderer, "{79, 54}"), "bottom slot 2");
    require(contains(renderer, "{133, 54}"), "bottom slot 3");
    require(contains(renderer, "{187, 54}"), "bottom slot 4");
    require(contains(renderer, "{241, 79}"), "bottom slot 5");
    require(contains(renderer, "draw_centered_text(left_content_x, 84"), "left centered status");
    require(contains(renderer, "status.fast_forward ? \"2X\" : \"1X\""), "FAST ratio status");
    require(!contains(renderer, "\"FPS\""), "FPS label removed");
    require(contains(renderer, "draw_centered_text(right_content_x, 50"), "right hint centered A/B");
    require(contains(renderer, "draw_centered_text(right_content_x, 130"), "right hint centered L/R");
    require(contains(render_layout, "struct TdvpK230Layout"), "TDVP has a distinct application layout");
    require(contains(render_layout, "static constexpr int ScreenW = 410"), "TDVP layout uses expanded 410px canvas");
    require(contains(render_layout, "static constexpr int GameW = 240"), "TDVP preserves native GBA width");
    require(contains(renderer, "F1 MENU"), "TDVP command bar shows K230 function-row labels");
    require(contains(renderer, "draw_tdvp_k230_side_panels"), "TDVP renders expanded side rails");
    require(contains(renderer, "tdvp_playing_static_cache_generation_"), "TDVP renderer caches unchanged playing chrome");

    const auto cmake = read_file(root / "CMakeLists.txt");
    require(contains(cmake, "APPLaunch/applications"), "APPLaunch desktop install path");
    require(contains(cmake, "APPLaunch/share/images"), "APPLaunch icon install path");
    require(contains(cmake, "lib/cardputer-zero-gba"), "private libdir install path");
    require(contains(cmake, "packaging/cardputer-zero-gba-applaunch"), "APPLaunch wrapper installed");
    require(contains(cmake, "CZ_GBA_PREFER_BUNDLED_SDL2"), "K230 can prefer bundled SDL2");
    require(contains(cmake, "CZ_GBA_TDVP_COMPOSABLE_FEED"),
            "TDVP feed build exposes a composable-runtime mode");
    require(contains(cmake, "CZ_GBA_SDL2_ROOT and CZ_GBA_MGBA_ROOT"),
            "TDVP composable mode requires independent SDL2 and mGBA prefixes");
    require(contains(cmake, "CZ_GBA_FETCH_SDL2 OFF"),
            "TDVP composable mode disables SDL FetchContent fallback");
    require(contains(cmake, "CZ_GBA_BUNDLE_MGBA OFF"),
            "TDVP composable mode disables bundled mGBA");
    require(contains(cmake, "CZ_GBA_REQUIRE_K230_DRM"), "K230 package can require DRM/KMS UAPI headers");
    require(contains(cmake, "CZ_GBA_REQUIRE_K230_WAYLAND_SHM"), "K230 package requires Wayland shared-memory client ABI");
    require(contains(cmake, "CZ_GBA_TDVP_WAYLAND_SCANNER"),
            "K230 build generates xdg client bindings from the matching SDK");
    require(contains(cmake, "CZ_GBA_XKBCOMMON_LIBRARY"),
            "K230 native client links the matching xkbcommon ABI");
    const auto tdvp_drm = read_file(root / "src" / "platform" / "tdvp_k230_drm.cpp");
    require(contains(tdvp_drm, "DRM_IOCTL_MODE_CREATE_DUMB"), "TDVP creates native KMS dumb buffers");
    require(contains(tdvp_drm, "DRM_IOCTL_MODE_SETCRTC"), "TDVP programs a KMS CRTC");
    require(contains(tdvp_drm, "DRM_IOCTL_MODE_GETCRTC"), "TDVP records original KMS state");
    require(contains(tdvp_drm, "drm_card_paths"), "TDVP finds the active DRM card dynamically");
    require(contains(tdvp_drm, "568x1232 native or 1232x568 landscape KMS output"),
            "TDVP recognizes the K230 native and landscape KMS modes");
    require(contains(tdvp_drm, "k230_landscape_to_scanout"),
            "TDVP maps landscape UI coordinates into the native KMS scanout");
    require(contains(tdvp_drm, "restore DRM_IOCTL_MODE_SETCRTC"), "TDVP restores KMS state on shutdown");
    require(contains(tdvp_drm, "disable inactive DRM CRTC during restore"), "TDVP restores an initially inactive CRTC");
    require(!contains(tdvp_drm, "linux/fb.h"), "TDVP DRM backend does not use fbdev");
    require(contains(tdvp_drm, "canvas_width, int canvas_height"), "TDVP DRM presents the active application canvas dimensions");

    require(std::filesystem::exists(root / "packaging" / "cardputer-zero-gba.png"), "APPLaunch icon exists");
    require(std::filesystem::exists(root / "assets" / "icons" / "cardputer-zero-gba-64.png"), "64px icon exists");
    require(std::filesystem::exists(root / "assets" / "icons" / "cardputer-zero-gba-128.png"), "128px icon exists");
    require(std::filesystem::exists(root / "assets" / "themes" / "minimal" / "theme.json"), "theme exists");
    require(std::filesystem::exists(root / "assets" / "themes" / "minimal" / "bezel.png"), "bezel exists");

    return 0;
}
