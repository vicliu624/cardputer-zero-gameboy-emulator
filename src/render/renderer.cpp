#include "render/renderer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <string>
#include <string_view>

namespace czgba::render {
namespace {

constexpr auto kBlack = rgb(4, 7, 10);
constexpr auto kPanel = rgb(13, 18, 22);
constexpr auto kPanelDark = rgb(9, 13, 17);
constexpr auto kPanelEdge = rgb(70, 77, 92);
constexpr auto kPanelBright = rgb(127, 132, 152);
constexpr auto kPanelShade = rgb(35, 42, 51);
constexpr auto kWhite = rgb(238, 239, 235);
constexpr auto kMuted = rgb(178, 181, 190);
constexpr auto kLavender = rgb(154, 143, 205);
constexpr auto kLavenderDeep = rgb(60, 55, 105);
constexpr auto kGreen = rgb(96, 232, 92);
constexpr auto kBlue = rgb(58, 174, 235);
constexpr auto kGrid = rgb(37, 46, 44);
constexpr auto kGold = rgb(246, 206, 42);

std::string fixed_1(double value)
{
    const int scaled = static_cast<int>(value * 10.0 + 0.5);
    return std::to_string(scaled / 10) + "." + std::to_string(scaled % 10);
}

bool is_ascii_printable(std::string_view text)
{
    return std::all_of(text.begin(), text.end(), [](unsigned char ch) {
        return ch >= 32 && ch < 127;
    });
}

std::string ascii_fallback_name(const std::string& text)
{
    if (is_ascii_printable(text)) {
        return text;
    }

    std::string fallback;
    fallback.reserve(text.size());
    bool previous_space = false;
    for (unsigned char ch : text) {
        if (ch >= 32 && ch < 127) {
            fallback.push_back(static_cast<char>(ch));
            previous_space = false;
        } else if (!previous_space) {
            fallback.push_back('?');
            previous_space = true;
        }
    }

    if (fallback.empty()) {
        return "?";
    }
    return fallback;
}

} // namespace

Renderer::Renderer() = default;

void Renderer::draw(const app::RenderState& state)
{
    canvas_.clear(kBlack);
    draw_theme_background();

    if (state.mode == app::AppMode::RomBrowser) {
        draw_rom_browser(state);
        draw_bottom_bar(state.bottom_bar);
        return;
    }

    if (state.mode == app::AppMode::Error) {
        draw_error(state);
        draw_bottom_bar(state.bottom_bar);
        return;
    }

    draw_game(state.game_frame);
    draw_side_panels(state.status);
    draw_bottom_bar(state.bottom_bar);
    draw_toast(state.status.toast);

    if (state.mode == app::AppMode::Paused) {
        draw_overlay("PAUSED");
    } else if (state.mode == app::AppMode::CheatMenu) {
        draw_overlay("CHEATS");
    } else if (state.mode == app::AppMode::Settings) {
        draw_overlay("SETTINGS");
    }
}

const Canvas& Renderer::canvas() const
{
    return canvas_;
}

Canvas& Renderer::canvas()
{
    return canvas_;
}

void Renderer::draw_theme_background()
{
    canvas_.fill_rect(0, 0, Layout::ScreenW, Layout::ScreenH, rgb(7, 10, 13));
    canvas_.draw_rect(0, 0, Layout::ScreenW, Layout::ScreenH, kPanelEdge);
    canvas_.draw_rect(2, 2, Layout::ScreenW - 4, Layout::ScreenH - 4, rgb(21, 26, 33));
    canvas_.draw_rect(4, 4, Layout::ScreenW - 8, Layout::ScreenH - 8, rgb(42, 48, 59));

    for (int y = 6; y < Layout::ScreenH - 6; y += 6) {
        for (int x = 6; x < Layout::ScreenW - 6; x += 7) {
            if (((x + y) % 5) == 0) {
                canvas_.set_pixel(x, y, rgb(12, 16, 20));
            }
        }
    }
}

void Renderer::draw_game(const core::GbaVideoFrame* frame)
{
    if (frame != nullptr && frame->pixels_xrgb8888 != nullptr) {
        canvas_.draw_pixels(
            Layout::GameX,
            Layout::GameY,
            std::min(Layout::GameW, frame->width),
            std::min(Layout::GameH, frame->height),
            frame->pixels_xrgb8888,
            frame->pitch_pixels);
        canvas_.draw_rect(Layout::GameX, Layout::GameY, Layout::GameW, Layout::GameH, rgb(22, 25, 28));
        return;
    }

    draw_game_test_pattern();
}

void Renderer::draw_game_test_pattern()
{
    for (int y = 0; y < Layout::GameH; ++y) {
        for (int x = 0; x < Layout::GameW; ++x) {
            const int tile = ((x / 8) + (y / 8)) & 1;
            const int hill = (x + y / 2) % 64;
            Canvas::Pixel color = tile == 0 ? rgb(67, 130, 84) : rgb(78, 147, 91);

            if (hill < 16) {
                color = tile == 0 ? rgb(111, 168, 86) : rgb(127, 181, 94);
            }
            if (y > 106 && y < 132 && x > 20 && x < 220) {
                color = ((x + y) & 4) == 0 ? rgb(176, 146, 79) : rgb(197, 166, 95);
            }
            if ((x % 32 == 0) || (y % 32 == 0)) {
                color = kGrid;
            }

            canvas_.set_pixel(Layout::GameX + x, Layout::GameY + y, color);
        }
    }

    draw_centered_text(Layout::GameX, Layout::GameY + 146, Layout::GameW, "240X160 TEST", rgb(24, 30, 34));
    canvas_.draw_rect(Layout::GameX, Layout::GameY, Layout::GameW, Layout::GameH, rgb(22, 25, 28));
}

void Renderer::draw_side_panels(const app::AppStatus& status)
{
    draw_panel(Layout::LeftX + 4, Layout::LeftY + 5, Layout::SideW - 8, Layout::SideH - 10, kPanel);
    draw_panel(Layout::RightX + 4, Layout::RightY + 5, Layout::SideW - 8, Layout::SideH - 10, kPanel);

    draw_panel_separators(Layout::LeftX);
    draw_panel_separators(Layout::RightX);

    draw_pixel_gameboy_icon(Layout::LeftX + 13, 14);
    draw_dpad_icon(Layout::RightX + 20, 26);

    constexpr int left_content_x = Layout::LeftX + 4;
    constexpr int left_content_w = Layout::SideW - 8;
    draw_centered_text(left_content_x, 47, left_content_w, "BAT", kLavender);
    draw_centered_text(left_content_x, 58, left_content_w, std::to_string(status.battery_percent) + "%", kGreen);
    draw_centered_text(left_content_x, 84, left_content_w, "FAST", kLavender);
    draw_centered_text(left_content_x, 95, left_content_w, status.fast_forward ? "2X" : "1X", kBlue);
    draw_centered_text(left_content_x, 128, left_content_w, "S" + std::to_string(status.current_slot), kLavender);

    constexpr int right_content_x = Layout::RightX + 4;
    constexpr int right_content_w = Layout::SideW - 8;
    draw_centered_text(right_content_x, 50, right_content_w, "A:J", kWhite);
    draw_centered_text(right_content_x, 69, right_content_w, "B:K", kWhite);
    draw_centered_text(right_content_x, 91, right_content_w, "L:U", kWhite);
    draw_centered_text(right_content_x, 130, right_content_w, "R:I", kWhite);
}

void Renderer::draw_bottom_bar(const std::string& text)
{
    canvas_.fill_rect(Layout::BarX + 4, Layout::BarY + 1, Layout::BarW - 8, Layout::BarH - 3, kPanel);
    canvas_.draw_rect(Layout::BarX + 4, Layout::BarY + 1, Layout::BarW - 8, Layout::BarH - 3, kPanelEdge);

    if (text.find('|') != std::string::npos) {
        draw_bottom_bar_segmented(text);
        return;
    }

    const int text_x = std::max(8, (Layout::ScreenW - font_.text_width(text)) / 2);
    font_.draw_text(canvas_, text_x, Layout::BarY + 2, text, kWhite);
}

void Renderer::draw_rom_browser(const app::RenderState& state)
{
    draw_side_panels(state.status);

    const int list_x = 44;
    const int list_y = 8;
    const int list_w = 198;
    const int list_h = 146;
    const int info_x = 246;
    const int info_y = 8;
    const int info_w = 36;
    const int info_h = 146;

    draw_panel(list_x, list_y, list_w, list_h, kPanelDark);
    draw_panel(info_x, info_y, info_w, info_h, kPanelDark);
    draw_centered_text(list_x, 15, list_w, "GBA", kLavender);
    draw_deco_line(list_x + 12, 25, list_w - 24);

    if (state.roms.empty()) {
        draw_centered_text(list_x, 58, list_w, "NO ROMS FOUND", kWhite);
        draw_centered_text(list_x, 70, list_w, "PUT GBA IN ROM", kLavender);
        draw_cartridge_icon(info_x + 12, info_y + 22, false);
        draw_centered_text(info_x, info_y + 48, info_w, "0", kLavender);
        return;
    }

    const int first = std::clamp(state.selected_rom - 2, 0, std::max(0, static_cast<int>(state.roms.size()) - 5));
    const int last = std::min(static_cast<int>(state.roms.size()), first + 5);
    int y = 38;
    for (int i = first; i < last; ++i) {
        const bool selected = i == state.selected_rom;
        if (selected) {
            canvas_.fill_rect(list_x + 7, y - 5, list_w - 14, 18, kLavenderDeep);
            canvas_.draw_rect(list_x + 7, y - 5, list_w - 14, 18, kLavender);
            font_.draw_text(canvas_, list_x + 11, y + 1, ">", kWhite);
        } else {
            draw_deco_line(list_x + 14, y + 13, list_w - 28);
        }
        draw_cartridge_icon(list_x + 27, y - 2, selected);
        draw_rom_name(
            list_x + 45,
            y - 2,
            list_w - 56,
            state.roms[static_cast<std::size_t>(i)].display_name,
            selected ? kWhite : kMuted);
        y += 23;
    }

    if (last < static_cast<int>(state.roms.size())) {
        canvas_.fill_rect(list_x + list_w / 2 - 2, list_y + list_h - 12, 5, 2, kLavender);
        canvas_.fill_rect(list_x + list_w / 2 - 1, list_y + list_h - 10, 3, 2, kLavender);
        canvas_.set_pixel(list_x + list_w / 2, list_y + list_h - 8, kLavender);
    }

    const int selected = std::clamp(state.selected_rom, 0, static_cast<int>(state.roms.size()) - 1);
    draw_cartridge_icon(info_x + 12, info_y + 18, true);
    draw_star_icon(info_x + 14, info_y + 43);
    draw_centered_text(info_x, info_y + 64, info_w, "S" + std::to_string(state.status.current_slot), kLavender);
    draw_centered_text(info_x, info_y + 82, info_w, std::to_string(selected + 1), kWhite);
    draw_centered_text(info_x, info_y + 94, info_w, "/", kMuted);
    draw_centered_text(info_x, info_y + 106, info_w, std::to_string(state.roms.size()), kMuted);
    draw_centered_text(info_x, info_y + 125, info_w, "OK", kGreen);
}

void Renderer::draw_overlay(const char* title)
{
    draw_game_dimmer();

    const int x = 104;
    const int y = 33;
    const int w = 112;
    const int h = 98;
    draw_panel(x, y, w, h, kPanel);
    draw_deco_line(x + 20, y + 17, w - 40);
    draw_centered_text(x, y + 9, w, title, kLavender);

    constexpr const char* kItems[] = {
        "Resume",
        "Save State",
        "Load State",
        "Cheats",
        "Settings",
        "Quit Game",
    };
    int item_y = y + 27;
    for (int i = 0; i < 6; ++i) {
        if (i == 0) {
            canvas_.fill_rect(x + 5, item_y - 3, w - 10, 12, kLavenderDeep);
            canvas_.draw_rect(x + 5, item_y - 3, w - 10, 12, kLavender);
            font_.draw_text(canvas_, x + 9, item_y, ">", kWhite);
        }
        font_.draw_text(canvas_, x + 22, item_y, kItems[i], i == 0 ? kWhite : kMuted);
        item_y += 13;
    }
}

void Renderer::draw_error(const app::RenderState& state)
{
    canvas_.fill_rect(20, 48, Layout::ScreenW - 40, 58, kPanel);
    canvas_.draw_rect(20, 48, Layout::ScreenW - 40, 58, kPanelEdge);
    draw_centered_text(20, 58, Layout::ScreenW - 40, "ERROR", kLavender);
    draw_centered_text(20, 75, Layout::ScreenW - 40, state.error_message, kWhite);
}

void Renderer::draw_toast(const std::string& toast)
{
    if (toast.empty()) {
        return;
    }

    const std::string compact = toast.size() > 5 ? toast.substr(0, 5) : toast;
    canvas_.fill_rect(Layout::LeftX + 6, 35, 30, 13, rgb(24, 48, 22));
    canvas_.draw_rect(Layout::LeftX + 6, 35, 30, 13, kGreen);
    font_.draw_text(canvas_, Layout::LeftX + 9, 38, compact, kGreen);
}

void Renderer::draw_panel(int x, int y, int width, int height, Canvas::Pixel fill)
{
    canvas_.fill_rect(x, y, width, height, fill);
    canvas_.draw_rect(x, y, width, height, kPanelEdge);
    canvas_.draw_rect(x + 2, y + 2, width - 4, height - 4, rgb(22, 27, 34));
    draw_corner_pixels(x, y, width, height);
}

void Renderer::draw_bottom_bar_segmented(std::string_view text)
{
    struct Slot {
        int x;
        int width;
    };

    constexpr std::array<Slot, 5> kSlots{{
        {0, 79},
        {79, 54},
        {133, 54},
        {187, 54},
        {241, 79},
    }};

    int slot_index = 0;
    std::size_t start = 0;
    while (slot_index < static_cast<int>(kSlots.size()) && start <= text.size()) {
        const auto end = text.find('|', start);
        const auto part = text.substr(start, end == std::string_view::npos ? text.size() - start : end - start);
        draw_bottom_bar_text_in_slot(kSlots[static_cast<std::size_t>(slot_index)].x,
                                     kSlots[static_cast<std::size_t>(slot_index)].width,
                                     part,
                                     slot_index == 3 ? kGold : kWhite);
        ++slot_index;
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }

    for (int x : {79, 133, 187, 241}) {
        canvas_.draw_vline(x, Layout::BarY + 2, Layout::BarH - 5, kPanelShade);
        canvas_.set_pixel(x, Layout::BarY + 1, kPanelBright);
    }
}

void Renderer::draw_bottom_bar_text_in_slot(int x, int width, std::string_view text, Canvas::Pixel color)
{
    const int max_width = std::max(0, width - 8);
    std::string rendered{text};
    if (font_.text_width(rendered) > max_width) {
        rendered = ellipsize_pixel_text(rendered, max_width);
    }
    const int text_x = x + std::max(0, (width - font_.text_width(rendered)) / 2);
    font_.draw_text(canvas_, text_x, Layout::BarY + 2, rendered, color);
}

void Renderer::draw_panel_separators(int x)
{
    for (int y : {41, 76, 116}) {
        canvas_.draw_hline(x + 8, y, 24, kPanelShade);
        canvas_.set_pixel(x + 19, y - 1, kPanelBright);
        canvas_.set_pixel(x + 20, y, kPanelBright);
        canvas_.set_pixel(x + 19, y + 1, kPanelBright);
        canvas_.set_pixel(x + 18, y, kPanelBright);
    }
}

void Renderer::draw_corner_pixels(int x, int y, int width, int height)
{
    canvas_.set_pixel(x + 2, y + 2, kPanelBright);
    canvas_.set_pixel(x + width - 3, y + 2, kPanelBright);
    canvas_.set_pixel(x + 2, y + height - 3, kPanelBright);
    canvas_.set_pixel(x + width - 3, y + height - 3, kPanelBright);
}

void Renderer::draw_deco_line(int x, int y, int width)
{
    if (width <= 6) {
        return;
    }
    canvas_.draw_hline(x + 5, y, width - 10, kPanelShade);
    canvas_.draw_rect(x, y - 2, 5, 5, kPanelShade);
    canvas_.draw_rect(x + width - 5, y - 2, 5, 5, kPanelShade);
    canvas_.set_pixel(x + 2, y, kPanelBright);
    canvas_.set_pixel(x + width - 3, y, kPanelBright);
}

void Renderer::draw_pixel_gameboy_icon(int x, int y)
{
    canvas_.fill_rect(x + 2, y, 14, 22, rgb(32, 36, 43));
    canvas_.draw_rect(x + 2, y, 14, 22, kPanelEdge);
    canvas_.fill_rect(x + 4, y + 3, 10, 8, kPanelDark);
    canvas_.draw_rect(x + 4, y + 3, 10, 8, kLavender);
    canvas_.fill_rect(x + 6, y + 5, 6, 4, rgb(39, 64, 68));
    canvas_.fill_rect(x + 5, y + 16, 5, 2, kBlack);
    canvas_.fill_rect(x + 6, y + 15, 3, 4, kBlack);
    canvas_.set_pixel(x + 12, y + 16, kLavender);
    canvas_.set_pixel(x + 15, y + 18, kLavender);
    canvas_.fill_rect(x + 3, y + 21, 4, 2, kPanelBright);
    canvas_.fill_rect(x + 12, y + 21, 4, 2, kPanelBright);
}

void Renderer::draw_dpad_icon(int x, int y)
{
    canvas_.fill_rect(x - 3, y - 10, 7, 21, kLavender);
    canvas_.fill_rect(x - 10, y - 3, 21, 7, kLavender);
    canvas_.draw_rect(x - 3, y - 10, 7, 21, kPanelEdge);
    canvas_.draw_rect(x - 10, y - 3, 21, 7, kPanelEdge);
    canvas_.fill_rect(x - 2, y - 2, 5, 5, rgb(93, 86, 136));
    canvas_.set_pixel(x, y - 7, kBlack);
    canvas_.set_pixel(x, y + 7, kBlack);
    canvas_.set_pixel(x - 7, y, kBlack);
    canvas_.set_pixel(x + 7, y, kBlack);
}

void Renderer::draw_cartridge_icon(int x, int y, bool selected)
{
    const auto edge = selected ? kWhite : kPanelBright;
    canvas_.fill_rect(x, y, 11, 12, rgb(45, 47, 51));
    canvas_.draw_rect(x, y, 11, 12, edge);
    canvas_.fill_rect(x + 2, y + 2, 7, 4, kPanelDark);
    canvas_.draw_hline(x + 3, y + 8, 5, kPanelBright);
    canvas_.set_pixel(x + 1, y + 10, kPanelBright);
    canvas_.set_pixel(x + 9, y + 10, kPanelBright);
}

void Renderer::draw_star_icon(int x, int y)
{
    canvas_.set_pixel(x + 4, y, kGold);
    canvas_.fill_rect(x + 3, y + 1, 3, 3, kGold);
    canvas_.fill_rect(x, y + 4, 9, 2, kGold);
    canvas_.fill_rect(x + 2, y + 6, 5, 2, kGold);
    canvas_.set_pixel(x + 1, y + 8, kGold);
    canvas_.set_pixel(x + 7, y + 8, kGold);
}

void Renderer::draw_mini_preview(int x, int y, int width, int height)
{
    canvas_.fill_rect(x, y, width, height, kBlack);
    for (int py = 2; py < height - 2; ++py) {
        for (int px = 2; px < width - 2; ++px) {
            Canvas::Pixel color = ((px / 5 + py / 5) & 1) == 0 ? rgb(69, 125, 76) : rgb(96, 151, 84);
            if (py > height / 2 && px > width / 3) {
                color = rgb(174, 139, 78);
            }
            if (px > width / 2 && py < height / 3) {
                color = rgb(74, 91, 126);
            }
            canvas_.set_pixel(x + px, y + py, color);
        }
    }
    canvas_.draw_rect(x, y, width, height, kPanelEdge);
    canvas_.draw_rect(x + 2, y + 2, width - 4, height - 4, kPanelBright);
}

void Renderer::draw_game_dimmer()
{
    for (int y = 0; y < Layout::GameH; ++y) {
        for (int x = 0; x < Layout::GameW; ++x) {
            const int sx = Layout::GameX + x;
            const int sy = Layout::GameY + y;
            const auto original = canvas_.data()[static_cast<std::size_t>(sy) * Canvas::Width + sx];
            const auto r = static_cast<unsigned char>(((original >> 16) & 0xff) / 3);
            const auto g = static_cast<unsigned char>(((original >> 8) & 0xff) / 3);
            const auto b = static_cast<unsigned char>((original & 0xff) / 3);
            canvas_.set_pixel(sx, sy, rgb(r, g, b));
        }
    }
}

void Renderer::draw_centered_text(int x, int y, int width, const std::string& text, Canvas::Pixel color)
{
    const int text_x = x + std::max(0, (width - font_.text_width(text)) / 2);
    font_.draw_text(canvas_, text_x, y, text, color);
}

void Renderer::draw_rom_name(int x, int y, int max_width, const std::string& text, Canvas::Pixel color)
{
    if (utf8_text_.available()) {
        utf8_text_.draw_text(canvas_, x, y, utf8_text_.ellipsize_to_width(text, max_width), color);
        return;
    }

    const auto fallback = ellipsize_pixel_text(ascii_fallback_name(text), max_width);
    font_.draw_text(canvas_, x, y + 2, fallback, color);
}

std::string Renderer::ellipsize_pixel_text(const std::string& text, int max_width) const
{
    if (font_.text_width(text) <= max_width) {
        return text;
    }

    constexpr const char* ellipsis = "...";
    const int ellipsis_width = font_.text_width(ellipsis);
    if (ellipsis_width >= max_width) {
        return ellipsis;
    }

    std::string result;
    for (std::size_t count = 1; count <= text.size(); ++count) {
        std::string candidate = text.substr(0, count);
        candidate += ellipsis;
        if (font_.text_width(candidate) > max_width) {
            break;
        }
        result = std::move(candidate);
    }

    if (result.empty()) {
        return ellipsis;
    }
    return result;
}

} // namespace czgba::render
