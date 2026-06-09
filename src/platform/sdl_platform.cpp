#include "platform/sdl_platform.hpp"

#include <SDL.h>

#include <iostream>

#include "render/layout.hpp"

namespace czgba::platform {

SdlPlatform::~SdlPlatform()
{
    shutdown();
}

bool SdlPlatform::init(const PlatformConfig& config)
{
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

    std::uint32_t window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS;

    if (config.fullscreen) {
        window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    window_ = SDL_CreateWindow(
        "cardputer-zero-gba",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        render::Layout::ScreenW,
        render::Layout::ScreenH,
        window_flags);

    if (window_ == nullptr) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        shutdown();
        return false;
    }

    SDL_SetWindowBordered(window_, SDL_FALSE);
    SDL_SetWindowSize(window_, render::Layout::ScreenW, render::Layout::ScreenH);
    if (config.fullscreen) {
        SDL_SetWindowFullscreen(window_, SDL_WINDOW_FULLSCREEN_DESKTOP);
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

    SDL_RenderSetLogicalSize(renderer_, render::Layout::ScreenW, render::Layout::ScreenH);

    texture_ = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_XRGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        render::Layout::ScreenW,
        render::Layout::ScreenH);

    if (texture_ == nullptr) {
        std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << '\n';
        shutdown();
        return false;
    }

    return true;
}

void SdlPlatform::shutdown()
{
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

void SdlPlatform::present(const std::uint32_t* canvas_xrgb8888, int pitch_bytes)
{
    SDL_UpdateTexture(texture_, nullptr, canvas_xrgb8888, pitch_bytes);
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
}

bool SdlPlatform::should_quit() const
{
    return should_quit_;
}

} // namespace czgba::platform
