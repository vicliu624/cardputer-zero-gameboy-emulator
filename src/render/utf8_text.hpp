#pragma once

#include <memory>
#include <string>

#include "render/canvas.hpp"

namespace czgba::render {

class Utf8Text {
public:
    Utf8Text();
    ~Utf8Text();

    Utf8Text(const Utf8Text&) = delete;
    Utf8Text& operator=(const Utf8Text&) = delete;

    bool available() const;
    int text_width(const std::string& text) const;
    void draw_text(Canvas& canvas, int x, int y, const std::string& text, Canvas::Pixel color) const;
    std::string ellipsize_to_width(const std::string& text, int max_width) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace czgba::render
