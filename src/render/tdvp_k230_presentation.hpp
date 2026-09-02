#pragma once

#include <cstdint>

namespace czgba::render {

// The K230 presenter owns the physical wl_shm buffers while Renderer owns the
// immutable chrome and the newest emulation frame. Keeping this boundary as a
// value type prevents the 60 Hz presenter from rebuilding a complete logical
// canvas just to display a new 240x160 GBA frame.
struct TdvpK230PresentationFrame {
    const std::uint32_t* static_pixels_xrgb8888 = nullptr;
    int static_width = 0;
    int static_height = 0;
    int static_pitch_bytes = 0;
    std::uint64_t static_generation = 0;

    const std::uint32_t* game_pixels_xrgb8888 = nullptr;
    int game_width = 0;
    int game_height = 0;
    int game_pitch_pixels = 0;
    bool game_frame_updated = false;
};

} // namespace czgba::render
