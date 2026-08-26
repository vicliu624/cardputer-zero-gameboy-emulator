#pragma once

namespace czgba::render {

// Application layout is deliberately separate from the physical presentation
// profile. The emulator always renders its native 240x160 GBA frame; a layout
// decides how the surrounding application chrome uses the available canvas.
enum class RenderLayoutProfile {
    CardputerZero,
    TdvpK230,
};

struct CanvasSpec {
    int width;
    int height;
};

struct Layout {
    static constexpr int ScreenW = 320;
    static constexpr int ScreenH = 170;

    static constexpr int GameX = 40;
    static constexpr int GameY = 0;
    static constexpr int GameW = 240;
    static constexpr int GameH = 160;

    static constexpr int LeftX = 0;
    static constexpr int LeftY = 0;
    static constexpr int SideW = 40;
    static constexpr int SideH = 160;

    static constexpr int RightX = 280;
    static constexpr int RightY = 0;

    static constexpr int BarX = 0;
    static constexpr int BarY = 160;
    static constexpr int BarW = 320;
    static constexpr int BarH = 10;
};

// The TDVP panel is 1232x568. A 410x189 application canvas is presented at a
// 3x integer scale (1230x567), leaving only a one-pixel horizontal centering
// margin. This preserves crisp pixels and gives the GBA frame dedicated space
// between two useful, keyboard-oriented information rails.
struct TdvpK230Layout {
    static constexpr int ScreenW = 410;
    static constexpr int ScreenH = 189;

    static constexpr int GameX = 85;
    static constexpr int GameY = 3;
    static constexpr int GameW = 240;
    static constexpr int GameH = 160;

    static constexpr int LeftX = 3;
    static constexpr int LeftY = 3;
    static constexpr int SideW = 78;
    static constexpr int SideH = 160;

    static constexpr int RightX = 329;
    static constexpr int RightY = 3;

    static constexpr int BarX = 3;
    static constexpr int BarY = 166;
    static constexpr int BarW = 404;
    static constexpr int BarH = 20;
};

constexpr CanvasSpec canvas_spec(RenderLayoutProfile profile)
{
    return profile == RenderLayoutProfile::TdvpK230
        ? CanvasSpec{TdvpK230Layout::ScreenW, TdvpK230Layout::ScreenH}
        : CanvasSpec{Layout::ScreenW, Layout::ScreenH};
}

} // namespace czgba::render
