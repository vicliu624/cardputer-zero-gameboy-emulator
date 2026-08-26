#pragma once

#include <cstdint>
#include <vector>

#include "render/layout.hpp"

namespace czgba::render {

class Canvas {
public:
    using Pixel = std::uint32_t;

    explicit Canvas(int width = Layout::ScreenW, int height = Layout::ScreenH);

    void clear(Pixel color);
    void set_pixel(int x, int y, Pixel color);
    void fill_rect(int x, int y, int width, int height, Pixel color);
    void draw_rect(int x, int y, int width, int height, Pixel color);
    void draw_hline(int x, int y, int width, Pixel color);
    void draw_vline(int x, int y, int height, Pixel color);
    void draw_pixels(int x, int y, int width, int height, const Pixel* pixels, int pitch_pixels);

    const Pixel* data() const;
    Pixel* data();
    int width() const;
    int height() const;
    int pitch_bytes() const;

private:
    int width_;
    int height_;
    std::vector<Pixel> pixels_;
};

constexpr Canvas::Pixel rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    return (static_cast<Canvas::Pixel>(r) << 16) |
           (static_cast<Canvas::Pixel>(g) << 8) |
           static_cast<Canvas::Pixel>(b);
}

} // namespace czgba::render
