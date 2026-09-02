#include "platform/sdl_platform.hpp"

#include <SDL.h>

#include <cstdlib>
#include <iostream>

#include "render/layout.hpp"
#include "render/tdvp_k230_presentation.hpp"

namespace czgba::platform {

namespace {

bool tdvp_direct_drm_requested()
{
    const char* value = std::getenv("CARDPUTER_ZERO_GBA_TDVP_DIRECT_DRM");
    return value != nullptr && value[0] == '1' && value[1] == '\0';
}

} // namespace

SdlPlatform::~SdlPlatform()
{
    shutdown();
}

bool SdlPlatform::init(const PlatformConfig& config)
{
    presentation_profile_ = config.presentation_profile;
    canvas_width_ = config.canvas_width;
    canvas_height_ = config.canvas_height;
    if (canvas_width_ <= 0 || canvas_height_ <= 0) {
        std::cerr << "Invalid application canvas dimensions\n";
        return false;
    }
    if (presentation_profile_ == PresentationProfile::TdvpK230 && tdvp_direct_drm_requested()) {
        auto drm = std::make_unique<TdvpK230Drm>();
        if (drm->init()) {
            tdvp_k230_drm_ = std::move(drm);
            return true;
        }
        std::cerr << "TDVP K230 direct DRM presentation unavailable; falling back to SDL Wayland\n";
    } else if (presentation_profile_ == PresentationProfile::TdvpK230) {
        // Labwc owns DRM master on the supported device. Its Wayland client
        // path remains backed by DRM/KMS, but prevents an application from
        // modesetting the system compositor's CRTC.
        std::cout << "TDVP K230: using the SDL Wayland client presentation path\n";
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "1");
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
    SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_ALLOW_LIBDECOR, "0");
    SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_PREFER_LIBDECOR, "0");
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "1");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return false;
    }

    const auto profile = presentation_profile_spec(config.presentation_profile);
    const bool use_fullscreen = config.fullscreen || profile.fullscreen;

    std::uint32_t window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS;

    if (use_fullscreen) {
        window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    window_ = SDL_CreateWindow(
        "cardputer-zero-gba",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        profile.initial_window_width,
        profile.initial_window_height,
        window_flags);

    if (window_ == nullptr) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        shutdown();
        return false;
    }

    SDL_SetWindowBordered(window_, SDL_FALSE);
    if (!use_fullscreen) {
        SDL_SetWindowSize(window_, profile.initial_window_width, profile.initial_window_height);
    }
    if (use_fullscreen) {
        SDL_SetWindowFullscreen(window_, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }

    if (presentation_profile_ == PresentationProfile::TdvpK230) {
        auto shm = std::make_unique<TdvpK230WaylandShm>();
        if (!shm->init(window_)) {
            std::cerr << "TDVP K230 wl_shm presentation unavailable; refusing software GLES fallback\n";
            shutdown();
            return false;
        }
        tdvp_k230_wayland_shm_ = std::move(shm);
        return true;
    }

    renderer_ = SDL_CreateRenderer(
        window_,
        -1,
        SDL_RENDERER_ACCELERATED);

    if (renderer_ == nullptr) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << '\n';
        shutdown();
        return false;
    }

    if (!profile.integer_scale) {
        SDL_RenderSetLogicalSize(renderer_, canvas_width_, canvas_height_);
    }

    texture_ = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_XRGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        canvas_width_,
        canvas_height_);

    if (texture_ == nullptr) {
        std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << '\n';
        shutdown();
        return false;
    }

    return true;
}

void SdlPlatform::shutdown()
{
    if (tdvp_k230_drm_) {
        tdvp_k230_drm_->shutdown();
        tdvp_k230_drm_.reset();
    }
    if (tdvp_k230_wayland_shm_) {
        tdvp_k230_wayland_shm_->shutdown();
        tdvp_k230_wayland_shm_.reset();
    }
    if (texture_ != nullptr) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

void SdlPlatform::poll_events(input::InputFrame& input)
{
    if (tdvp_k230_drm_) {
        tdvp_k230_drm_->poll_events(input);
        should_quit_ = tdvp_k230_drm_->should_quit();
        return;
    }

    input.begin_frame();
    input.gba = input_mapper_.gba_state();

    SDL_Event event{};
    while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT) {
            should_quit_ = true;
            input.quit_requested = true;
        }
        if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            const bool pressed = event.type == SDL_KEYDOWN;
            input_mapper_.handle_key(event.key.keysym.sym, pressed, input);
        }
    }
}

void SdlPlatform::present(const std::uint32_t* canvas_xrgb8888, int canvas_width, int canvas_height, int pitch_bytes)
{
    if (canvas_width != canvas_width_ || canvas_height != canvas_height_) {
        std::cerr << "Application canvas dimensions changed after platform initialization\n";
        return;
    }
    if (tdvp_k230_drm_) {
        tdvp_k230_drm_->present(canvas_xrgb8888, canvas_width, canvas_height, pitch_bytes);
        return;
    }
    if (tdvp_k230_wayland_shm_) {
        std::cerr << "TDVP K230 presentation requires static chrome and a raw GBA frame\n";
        return;
    }
    SDL_UpdateTexture(texture_, nullptr, canvas_xrgb8888, pitch_bytes);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    if (presentation_profile_ == PresentationProfile::TdvpK230) {
        int output_width = 0;
        int output_height = 0;
        if (SDL_GetRendererOutputSize(renderer_, &output_width, &output_height) == 0) {
            const auto destination = integer_presentation_rect(
                canvas_width_,
                canvas_height_,
                output_width,
                output_height);
            if (destination.width > 0 && destination.height > 0) {
                const SDL_Rect rect{destination.x, destination.y, destination.width, destination.height};
                SDL_RenderCopy(renderer_, texture_, nullptr, &rect);
            }
        }
    } else {
        SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    }
    SDL_RenderPresent(renderer_);
}

bool SdlPlatform::tdvp_k230_presentation_ready()
{
    // The experimental direct-DRM mode remains ungated; the supported
    // Wayland path can coalesce intermediate emulation frames before the
    // expensive XRGB conversion is performed.
    return !tdvp_k230_wayland_shm_ || tdvp_k230_wayland_shm_->ready_for_frame();
}

void SdlPlatform::present_tdvp_k230(const render::TdvpK230PresentationFrame& frame)
{
    if (tdvp_k230_wayland_shm_) {
        tdvp_k230_wayland_shm_->present(frame);
    }
}

bool SdlPlatform::should_quit() const
{
    return should_quit_;
}

} // namespace czgba::platform
