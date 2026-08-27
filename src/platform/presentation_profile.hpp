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

// The K230 product is held landscape, but its RM69A10 DSI connector exposes a
// portrait 568x1232 native scanout. Keep the user-visible landscape coordinate
// system separate from the KMS buffer coordinate system: the emulator layout
// and the 720x480 GBA viewport are defined in the former, while the DRM
// backend maps each pixel into the latter.
enum class K230ScanoutTransform {
    Unsupported,
    Identity,
    RotateCounterClockwise,
};

struct PresentationPoint {
    int x = 0;
    int y = 0;
};

constexpr int kK230LandscapeWidth = 1232;
constexpr int kK230LandscapeHeight = 568;
constexpr int kK230NativeScanoutWidth = 568;
constexpr int kK230NativeScanoutHeight = 1232;

constexpr K230ScanoutTransform k230_scanout_transform(int scanout_width, int scanout_height)
{
    if (scanout_width == kK230LandscapeWidth && scanout_height == kK230LandscapeHeight) {
        return K230ScanoutTransform::Identity;
    }
    if (scanout_width == kK230NativeScanoutWidth && scanout_height == kK230NativeScanoutHeight) {
        // The firmware uses fbcon=rotate:3 for this panel. Apply that same
        // counter-clockwise content transform when addressing a raw DRM buffer.
        return K230ScanoutTransform::RotateCounterClockwise;
    }
    return K230ScanoutTransform::Unsupported;
}

constexpr PresentationPoint k230_landscape_to_scanout(
    int landscape_x,
    int landscape_y,
    int scanout_width,
    int scanout_height)
{
    switch (k230_scanout_transform(scanout_width, scanout_height)) {
    case K230ScanoutTransform::Identity:
        return {landscape_x, landscape_y};
    case K230ScanoutTransform::RotateCounterClockwise:
        return {landscape_y, scanout_height - 1 - landscape_x};
    case K230ScanoutTransform::Unsupported:
    default:
        return {-1, -1};
    }
}

constexpr PresentationProfileSpec presentation_profile_spec(PresentationProfile profile)
{
    switch (profile) {
    case PresentationProfile::TdvpK230:
        // SDL development fallback uses the user-visible landscape coordinate
        // system. The native DRM backend handles the K230's portrait scanout.
        return {kK230LandscapeWidth, kK230LandscapeHeight, true, true};
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
