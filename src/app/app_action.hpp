#pragma once

namespace czgba::app {

enum class AppAction {
    None,
    Confirm,
    Back,
    Up,
    Down,
    Left,
    Right,
    OpenMenu,
    SaveState,
    LoadState,
    ToggleFastForward,
    OpenCheatMenu,
    Quit
};

} // namespace czgba::app
