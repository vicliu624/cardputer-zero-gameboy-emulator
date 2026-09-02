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
    const auto& presentation = renderer.tdvp_k230_presentation();

    require(canvas.width() == T::ScreenW && canvas.height() == T::ScreenH,
            "TDVP renderer creates the dedicated 410x189 canvas");
    require(presentation.static_pixels_xrgb8888 != nullptr &&
                presentation.static_width == T::ScreenW && presentation.static_height == T::ScreenH,
            "TDVP renderer publishes a complete static chrome canvas");
    require(presentation.game_frame_updated && presentation.game_pixels_xrgb8888 == pixels.data() &&
                presentation.game_width == T::GameW && presentation.game_height == T::GameH,
            "the raw 240x160 GBA frame is published without rebuilding the static canvas");
    require(presentation.static_pixels_xrgb8888[
                static_cast<std::size_t>(T::LeftY) * static_cast<std::size_t>(T::ScreenW) + T::LeftX] !=
                render::rgb(7, 10, 13),
            "expanded left information rail is rendered");
    require(presentation.static_pixels_xrgb8888[
                static_cast<std::size_t>(T::RightY) * static_cast<std::size_t>(T::ScreenW) + T::RightX] !=
                render::rgb(7, 10, 13),
            "expanded right control rail is rendered");
    require(presentation.static_pixels_xrgb8888[
                static_cast<std::size_t>(T::BarY) * static_cast<std::size_t>(T::ScreenW) + T::BarX] !=
                render::rgb(7, 10, 13),
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
