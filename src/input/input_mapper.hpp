#pragma once

#include <SDL.h>

#include "input/input_frame.hpp"

namespace czgba::input {

class InputMapper {
public:
    void handle_key(SDL_Keycode key, bool pressed, InputFrame& frame);
    // Wayland sends a keyboard-leave when Labwc changes focus. Clear held GBA
    // controls there so a lost key-release cannot leave a direction or button
    // latched in the emulation core.
    void release_all(InputFrame& frame);
    const core::GbaInputState& gba_state() const;

private:
    core::GbaInputState gba_{};
};

} // namespace czgba::input
