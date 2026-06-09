#pragma once

#include <SDL.h>

#include "input/input_frame.hpp"

namespace czgba::input {

class InputMapper {
public:
    void handle_key(SDL_Keycode key, bool pressed, InputFrame& frame);
    const core::GbaInputState& gba_state() const;

private:
    core::GbaInputState gba_{};
};

} // namespace czgba::input
