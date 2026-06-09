#pragma once

#include <cstdint>
#include <string_view>

#include "render/canvas.hpp"

namespace czgba::render {

class Font5x7 {
public:
    static constexpr int GlyphW = 5;
    static constexpr int GlyphH = 7;
    static constexpr int Advance = 6;

    void draw_text(Canvas& canvas, int x, int y, std::string_view text, Canvas::Pixel color) const;
    int text_width(std::string_view text) const;

private:
    const std::uint8_t* glyph_for(char ch) const;
};

} // namespace czgba::render
