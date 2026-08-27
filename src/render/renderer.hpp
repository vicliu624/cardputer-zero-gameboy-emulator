#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "app/app.hpp"
#include "core/gba_video_frame.hpp"
#include "render/canvas.hpp"
#include "render/font_5x7.hpp"
#include "render/utf8_text.hpp"

namespace czgba::render {

class Renderer {
public:
    explicit Renderer(RenderLayoutProfile layout_profile = RenderLayoutProfile::CardputerZero);

    void draw(const app::RenderState& state);
    const Canvas& canvas() const;
    Canvas& canvas();
    std::uint64_t tdvp_playing_static_cache_generation() const;

private:
    void draw_tdvp_k230(const app::RenderState& state);
    void draw_tdvp_k230_playing(const app::RenderState& state);
    bool tdvp_playing_static_cache_matches(const app::RenderState& state) const;
    void rebuild_tdvp_playing_static_cache(const app::RenderState& state);
    void draw_tdvp_k230_background();
    void draw_tdvp_k230_game(const core::GbaVideoFrame* frame);
    void draw_tdvp_k230_game_test_pattern();
    void draw_tdvp_k230_side_panels(const app::AppStatus& status);
    void draw_tdvp_k230_bottom_bar(const app::RenderState& state);
    void draw_tdvp_k230_rom_browser(const app::RenderState& state);
    void draw_tdvp_k230_overlay(const char* title, int selected_index);
    void draw_tdvp_k230_error(const app::RenderState& state);
    void draw_tdvp_k230_toast(const std::string& toast);
    void draw_tdvp_k230_game_dimmer();
    void draw_theme_background();
    void draw_game(const core::GbaVideoFrame* frame);
    void draw_game_test_pattern();
    void draw_side_panels(const app::AppStatus& status);
    void draw_bottom_bar(const std::string& text);
    void draw_rom_browser(const app::RenderState& state);
    void draw_overlay(const char* title, int selected_index);
    void draw_error(const app::RenderState& state);
    void draw_toast(const std::string& toast);
    void draw_panel(int x, int y, int width, int height, Canvas::Pixel fill);
    void draw_bottom_bar_segmented(std::string_view text);
    void draw_bottom_bar_text_in_slot(int x, int width, std::string_view text, Canvas::Pixel color);
    void draw_panel_separators(int x);
    void draw_corner_pixels(int x, int y, int width, int height);
    void draw_deco_line(int x, int y, int width);
    void draw_pixel_gameboy_icon(int x, int y);
    void draw_dpad_icon(int x, int y);
    void draw_cartridge_icon(int x, int y, bool selected);
    void draw_star_icon(int x, int y);
    void draw_mini_preview(int x, int y, int width, int height);
    void draw_game_dimmer();
    void draw_centered_text(int x, int y, int width, const std::string& text, Canvas::Pixel color);
    void draw_rom_name(int x, int y, int max_width, const std::string& text, Canvas::Pixel color);
    std::string ellipsize_pixel_text(const std::string& text, int max_width) const;

    RenderLayoutProfile layout_profile_;
    Canvas canvas_;
    Font5x7 font_;
    Utf8Text utf8_text_;
    std::vector<Canvas::Pixel> tdvp_playing_static_pixels_;
    int tdvp_cached_battery_percent_ = -1;
    int tdvp_cached_slot_ = -1;
    bool tdvp_cached_fast_forward_ = false;
    std::string tdvp_cached_toast_;
    bool tdvp_playing_static_cache_valid_ = false;
    std::uint64_t tdvp_playing_static_cache_generation_ = 0;
};

} // namespace czgba::render
