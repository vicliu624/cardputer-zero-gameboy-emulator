#pragma once

#include <cstdint>
#include <memory>

#include "input/input_frame.hpp"
#include "input/input_mapper.hpp"
#include "platform/presentation_profile.hpp"
#include "platform/tdvp_k230_drm.hpp"
#include "platform/tdvp_k230_wayland_shm.hpp"

struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Window;

namespace czgba::platform {

struct PlatformConfig {
    bool kiosk = false;
    bool fullscreen = false;
    PresentationProfile presentation_profile = PresentationProfile::CardputerZero;
    int canvas_width = 320;
    int canvas_height = 170;
};

class SdlPlatform {
public:
    SdlPlatform() = default;
    ~SdlPlatform();

    SdlPlatform(const SdlPlatform&) = delete;
    SdlPlatform& operator=(const SdlPlatform&) = delete;

    bool init(const PlatformConfig& config);
    void shutdown();
    void poll_events(input::InputFrame& input);
    void present(const std::uint32_t* canvas_xrgb8888, int canvas_width, int canvas_height, int pitch_bytes);
    bool should_quit() const;

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    input::InputMapper input_mapper_;
    std::unique_ptr<TdvpK230Drm> tdvp_k230_drm_;
    std::unique_ptr<TdvpK230WaylandShm> tdvp_k230_wayland_shm_;
    PresentationProfile presentation_profile_ = PresentationProfile::CardputerZero;
    int canvas_width_ = 320;
    int canvas_height_ = 170;
    bool should_quit_ = false;
};

} // namespace czgba::platform
