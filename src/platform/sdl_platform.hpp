#pragma once

#include <cstdint>

#include "input/input_frame.hpp"
#include "input/input_mapper.hpp"

struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Window;

namespace czgba::platform {

struct PlatformConfig {
    bool kiosk = false;
    bool fullscreen = false;
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
    void present(const std::uint32_t* canvas_xrgb8888, int pitch_bytes);
    bool should_quit() const;

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    input::InputMapper input_mapper_;
    bool should_quit_ = false;
};

} // namespace czgba::platform
