#pragma once

namespace czgba::render {

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

} // namespace czgba::render
