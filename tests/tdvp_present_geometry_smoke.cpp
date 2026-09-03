#include <cstdlib>
#include <iostream>

#include "platform/presentation_profile.hpp"
#include "platform/tdvp_k230_present_geometry.hpp"
#include "render/layout.hpp"

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "TDVP present geometry smoke failed: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main()
{
    using namespace czgba;
    const auto rect = platform::tdvp_k230_game_damage_rect(
        render::TdvpK230Layout::ScreenW,
        render::TdvpK230Layout::ScreenH,
        platform::kK230LandscapeWidth,
        platform::kK230LandscapeHeight);
    require(rect.x == 256 && rect.y == 9,
        "the game viewport starts at its physical 3x buffer-space origin");
    require(rect.width == 720 && rect.height == 480,
        "normal Playing damage covers exactly the 240x160 GBA viewport at 3x");

    const auto surface = platform::tdvp_k230_game_surface_rect(
        render::TdvpK230Layout::ScreenW,
        render::TdvpK230Layout::ScreenH,
        platform::kK230LandscapeWidth,
        platform::kK230LandscapeHeight);
    require(surface.x == rect.x && surface.y == rect.y &&
                surface.width == rect.width && surface.height == rect.height,
        "the game child surface uses the exact physical dynamic viewport");

    const auto invalid = platform::tdvp_k230_game_damage_rect(0, 189, 1232, 568);
    require(invalid.width == 0 && invalid.height == 0,
        "invalid presentation geometry does not damage an arbitrary buffer area");
    return 0;
}
