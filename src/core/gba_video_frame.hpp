#pragma once

#include <cstdint>

namespace czgba::core {

struct GbaVideoFrame {
    const std::uint32_t* pixels_xrgb8888 = nullptr;
    int width = 240;
    int height = 160;
    int pitch_pixels = 240;
};

} // namespace czgba::core
