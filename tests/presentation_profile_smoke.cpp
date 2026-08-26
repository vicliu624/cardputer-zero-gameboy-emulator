#include <cstdlib>
#include <iostream>

#include "platform/presentation_profile.hpp"
#include "render/layout.hpp"

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "presentation profile smoke failed: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main()
{
    const auto cardputer = czgba::platform::presentation_profile_spec(
        czgba::platform::PresentationProfile::CardputerZero);
    require(cardputer.initial_window_width == 320, "Cardputer Zero initial width");
    require(cardputer.initial_window_height == 170, "Cardputer Zero initial height");
    require(!cardputer.fullscreen, "Cardputer Zero does not force fullscreen");
    require(!cardputer.integer_scale, "Cardputer Zero preserves existing SDL logical sizing");

    const auto k230 = czgba::platform::presentation_profile_spec(
        czgba::platform::PresentationProfile::TdvpK230);
    require(k230.initial_window_width == 1232, "TDVP K230 landscape width");
    require(k230.initial_window_height == 568, "TDVP K230 landscape height");
    require(k230.fullscreen, "TDVP K230 forces fullscreen");
    require(k230.integer_scale, "TDVP K230 uses integer scaling");

    const auto cardputer_canvas = czgba::render::canvas_spec(
        czgba::render::RenderLayoutProfile::CardputerZero);
    require(cardputer_canvas.width == 320 && cardputer_canvas.height == 170,
            "Cardputer Zero preserves the compact application canvas");

    const auto k230_canvas = czgba::render::canvas_spec(
        czgba::render::RenderLayoutProfile::TdvpK230);
    require(k230_canvas.width == 410 && k230_canvas.height == 189,
            "TDVP K230 uses its dedicated large-screen application canvas");
    require(czgba::render::TdvpK230Layout::GameX == 85 &&
                czgba::render::TdvpK230Layout::GameY == 3 &&
                czgba::render::TdvpK230Layout::GameW == 240 &&
                czgba::render::TdvpK230Layout::GameH == 160,
            "TDVP preserves the native 240x160 GBA viewport");

    const auto rect = czgba::platform::integer_presentation_rect(
        k230_canvas.width, k230_canvas.height, 1232, 568);
    require(rect.scale == 3, "K230 large-screen scale is three");
    require(rect.width == 1230 && rect.height == 567, "K230 destination uses nearly the entire panel");
    require(rect.x == 1 && rect.y == 0, "K230 destination is pixel-centered");

    const auto legacy_rect = czgba::platform::integer_presentation_rect(320, 170, 1232, 568);
    require(legacy_rect.scale == 3 && legacy_rect.width == 960 && legacy_rect.height == 510,
            "legacy compact canvas behavior remains defined");

    const auto small = czgba::platform::integer_presentation_rect(320, 170, 320, 170);
    require(small.scale == 1 && small.x == 0 && small.y == 0, "native source surface remains native");

    const auto invalid = czgba::platform::integer_presentation_rect(320, 170, 0, 568);
    require(invalid.width == 0 && invalid.height == 0, "invalid outputs do not create a destination rect");
    return 0;
}
