#include "input/input_mapper.hpp"

namespace czgba::input {

void InputMapper::handle_key(SDL_Keycode key, bool pressed, InputFrame& frame)
{
    switch (key) {
    case SDLK_w:
    case SDLK_UP:
        gba_.up = pressed;
        if (pressed) frame.actions.push_back(app::AppAction::Up);
        break;
    case SDLK_s:
    case SDLK_DOWN:
        gba_.down = pressed;
        if (pressed) frame.actions.push_back(app::AppAction::Down);
        break;
    case SDLK_a:
    case SDLK_LEFT:
        gba_.left = pressed;
        if (pressed) frame.actions.push_back(app::AppAction::Left);
        break;
    case SDLK_d:
    case SDLK_RIGHT:
        gba_.right = pressed;
        if (pressed) frame.actions.push_back(app::AppAction::Right);
        break;
    case SDLK_j:
        gba_.a = pressed;
        if (pressed) frame.actions.push_back(app::AppAction::Confirm);
        break;
    case SDLK_k:
        gba_.b = pressed;
        if (pressed) frame.actions.push_back(app::AppAction::Back);
        break;
    case SDLK_u:
        gba_.l = pressed;
        break;
    case SDLK_i:
        gba_.r = pressed;
        break;
    case SDLK_RETURN:
        gba_.start = pressed;
        if (pressed) frame.actions.push_back(app::AppAction::Confirm);
        break;
    case SDLK_SPACE:
        gba_.select = pressed;
        break;
    case SDLK_4:
        if (pressed) frame.actions.push_back(app::AppAction::OpenMenu);
        break;
    case SDLK_5:
        if (pressed) frame.actions.push_back(app::AppAction::SaveState);
        break;
    case SDLK_6:
        if (pressed) frame.actions.push_back(app::AppAction::LoadState);
        break;
    case SDLK_7:
        if (pressed) frame.actions.push_back(app::AppAction::ToggleFastForward);
        break;
    case SDLK_8:
        if (pressed) frame.actions.push_back(app::AppAction::OpenCheatMenu);
        break;
    case SDLK_q:
        if (pressed) frame.actions.push_back(app::AppAction::Quit);
        break;
    default:
        break;
    }

    frame.gba = gba_;
}

const core::GbaInputState& InputMapper::gba_state() const
{
    return gba_;
}

} // namespace czgba::input
