#include "render/canvas.hpp"

#include <algorithm>

namespace czgba::render {

Canvas::Canvas(int width, int height)
    : width_(std::max(1, width))
    , height_(std::max(1, height))
    , pixels_(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_))
{
    clear(rgb(0, 0, 0));
}

void Canvas::clear(Pixel color)
{
    std::fill(pixels_.begin(), pixels_.end(), color);
}

void Canvas::set_pixel(int x, int y, Pixel color)
{
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return;
    }

    pixels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)] = color;
}

void Canvas::fill_rect(int x, int y, int width, int height, Pixel color)
{
    if (width <= 0 || height <= 0) {
        return;
    }

    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min(width_, x + width);
    const int y1 = std::min(height_, y + height);

    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            set_pixel(px, py, color);
        }
    }
}

void Canvas::draw_rect(int x, int y, int width, int height, Pixel color)
{
    if (width <= 0 || height <= 0) {
        return;
    }

    draw_hline(x, y, width, color);
    draw_hline(x, y + height - 1, width, color);
    draw_vline(x, y, height, color);
    draw_vline(x + width - 1, y, height, color);
}

void Canvas::draw_hline(int x, int y, int width, Pixel color)
{
    if (width <= 0) {
        return;
    }

    for (int px = x; px < x + width; ++px) {
        set_pixel(px, y, color);
    }
}

void Canvas::draw_vline(int x, int y, int height, Pixel color)
{
    if (height <= 0) {
        return;
    }

    for (int py = y; py < y + height; ++py) {
        set_pixel(x, py, color);
    }
}

void Canvas::draw_pixels(int x, int y, int width, int height, const Pixel* pixels, int pitch_pixels)
{
    if (pixels == nullptr || width <= 0 || height <= 0 || pitch_pixels <= 0) {
        return;
    }

    for (int py = 0; py < height; ++py) {
        for (int px = 0; px < width; ++px) {
            set_pixel(x + px, y + py, pixels[static_cast<std::size_t>(py) * pitch_pixels + px]);
        }
    }
}

const Canvas::Pixel* Canvas::data() const
{
    return pixels_.data();
}

Canvas::Pixel* Canvas::data()
{
    return pixels_.data();
}

int Canvas::width() const
{
    return width_;
}

int Canvas::height() const
{
    return height_;
}

int Canvas::pitch_bytes() const
{
    return width_ * static_cast<int>(sizeof(Pixel));
}

} // namespace czgba::render
