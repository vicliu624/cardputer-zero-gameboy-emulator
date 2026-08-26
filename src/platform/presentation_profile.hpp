#pragma once

#include <algorithm>

namespace czgba::platform {

// A presentation profile describes physical-output ownership and geometry.
// main.cpp explicitly maps it to the matching application render-layout
// profile. Neither profile changes emulator timing, input semantics, or the
// native 240x160 GBA source frame.
enum class PresentationProfile {
    CardputerZero,
    TdvpK230,
};

struct PresentationProfileSpec {
    int initial_window_width = 320;
    int initial_window_height = 170;
    bool fullscreen = false;
    bool integer_scale = false;
};

struct PresentationRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int scale = 1;
};

constexpr PresentationProfileSpec presentation_profile_spec(PresentationProfile profile)
{
    switch (profile) {
    case PresentationProfile::TdvpK230:
        // The panel is 1232x568 when driven in its required landscape mode.
        return {1232, 568, true, true};
    case PresentationProfile::CardputerZero:
    default:
        return {320, 170, false, false};
    }
}

constexpr PresentationRect integer_presentation_rect(
    int source_width,
    int source_height,
    int output_width,
    int output_height)
{
    if (source_width <= 0 || source_height <= 0 || output_width <= 0 || output_height <= 0) {
        return {};
    }

    const int scale = std::max(1, std::min(output_width / source_width, output_height / source_height));
    const int width = source_width * scale;
    const int height = source_height * scale;
    return {
        (output_width - width) / 2,
        (output_height - height) / 2,
        width,
        height,
        scale,
    };
}

} // namespace czgba::platform
