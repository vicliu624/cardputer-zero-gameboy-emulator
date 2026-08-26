#pragma once

#include <cstdint>
#include <memory>

#include "input/input_frame.hpp"
#include "input/input_mapper.hpp"

namespace czgba::platform {

struct TdvpK230DrmState;

// Native, application-scoped DRM/KMS presentation for the TDVP K230 profile.
// This owns one CRTC only while the application is running and restores its
// prior state during shutdown. It deliberately does not use fbdev or require
// a Wayland compositor.
class TdvpK230Drm {
public:
    TdvpK230Drm();
    ~TdvpK230Drm();

    TdvpK230Drm(const TdvpK230Drm&) = delete;
    TdvpK230Drm& operator=(const TdvpK230Drm&) = delete;

    bool init();
    void shutdown();
    void poll_events(input::InputFrame& input);
    void present(const std::uint32_t* canvas_xrgb8888, int canvas_width, int canvas_height, int pitch_bytes);
    bool should_quit() const;

private:
    std::unique_ptr<TdvpK230DrmState> state_;
    input::InputMapper input_mapper_;
    bool should_quit_ = false;
};

} // namespace czgba::platform
