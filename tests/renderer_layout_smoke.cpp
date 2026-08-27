#include <array>
#include <cstdlib>
#include <iostream>

#include "app/app.hpp"
#include "core/gba_video_frame.hpp"
#include "platform/presentation_profile.hpp"
#include "render/layout.hpp"
#include "render/renderer.hpp"

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "renderer layout smoke failed: " << message << '\n';
        std::exit(1);
    }
}

czgba::render::Canvas::Pixel pixel_at(const czgba::render::Canvas& canvas, int x, int y)
{
    return canvas.data()[static_cast<std::size_t>(y) * static_cast<std::size_t>(canvas.width()) +
                         static_cast<std::size_t>(x)];
}

} // namespace

int main()
{
    using namespace czgba;
    using T = render::TdvpK230Layout;

    constexpr auto kFramePixel = render::rgb(31, 121, 197);
    std::array<render::Canvas::Pixel, T::GameW * T::GameH> pixels{};
    pixels.fill(kFramePixel);
    const core::GbaVideoFrame frame{
        pixels.data(),
        T::GameW,
        T::GameH,
        T::GameW,
    };

    app::RenderState state;
    state.mode = app::AppMode::Playing;
    state.game_frame = &frame;
    state.status.battery_percent = 87;
    state.status.current_slot = 1;
    state.bottom_bar = "MENU|SAVE|LOAD|FAST|CHEATS";

    render::Renderer renderer(render::RenderLayoutProfile::TdvpK230);
    renderer.draw(state);
    const auto& canvas = renderer.canvas();

    require(canvas.width() == T::ScreenW && canvas.height() == T::ScreenH,
            "TDVP renderer creates the dedicated 410x189 canvas");
    require(pixel_at(canvas, T::GameX + 120, T::GameY + 80) == kFramePixel,
            "native 240x160 GBA pixels are copied unchanged into the TDVP viewport");
    require(pixel_at(canvas, T::GameX, T::GameY) != kFramePixel,
            "TDVP viewport retains its application chrome border");
    require(pixel_at(canvas, T::LeftX, T::LeftY) != render::rgb(7, 10, 13),
            "expanded left information rail is rendered");
    require(pixel_at(canvas, T::RightX, T::RightY) != render::rgb(7, 10, 13),
            "expanded right control rail is rendered");
    require(pixel_at(canvas, T::BarX, T::BarY) != render::rgb(7, 10, 13),
            "expanded F1-F5 command bar is rendered");

    const auto initial_static_generation = renderer.tdvp_playing_static_cache_generation();
    renderer.draw(state);
    require(renderer.tdvp_playing_static_cache_generation() == initial_static_generation,
            "unchanged K230 playing chrome is reused from the static cache");
    state.status.current_slot = 2;
    renderer.draw(state);
    require(renderer.tdvp_playing_static_cache_generation() == initial_static_generation + 1,
            "K230 playing chrome cache rebuilds when UI status changes");

    const auto physical = platform::integer_presentation_rect(
        canvas.width(), canvas.height(), 1232, 568);
    require(physical.scale == 3 && physical.width == 1230 && physical.height == 567,
            "K230 canvas becomes a 1230x567 physical DRM presentation");
    require(T::GameW * physical.scale == 720 && T::GameH * physical.scale == 480,
            "K230 game viewport is physically 720x480");
    return 0;
}
