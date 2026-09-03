#pragma once

#include "platform/presentation_profile.hpp"
#include "render/layout.hpp"

namespace czgba::platform {

struct TdvpK230DamageRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

// Wayland damage_buffer coordinates are buffer pixels, not the logical
// 410x189 application canvas. Keep this calculation beside the presenter so
// a change to either layout or integer scaling cannot silently widen routine
// Playing damage back to the whole 1232x568 surface.
constexpr TdvpK230DamageRect tdvp_k230_game_damage_rect(
    int source_width,
    int source_height,
    int output_width,
    int output_height)
{
    const auto destination = integer_presentation_rect(
        source_width, source_height, output_width, output_height);
    if (destination.scale <= 0 || destination.width <= 0 || destination.height <= 0) {
        return {};
    }
    using T = render::TdvpK230Layout;
    return {
        destination.x + T::GameX * destination.scale,
        destination.y + T::GameY * destination.scale,
        T::GameW * destination.scale,
        T::GameH * destination.scale,
    };
}

} // namespace czgba::platform
