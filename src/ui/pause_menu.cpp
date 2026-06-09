#include "ui/pause_menu.hpp"

namespace czgba::ui {

PauseMenu::PauseMenu()
{
    model_.items = {
        {"resume", "Resume", true, false},
        {"save_state", "Save State", true, false},
        {"load_state", "Load State", true, false},
        {"cheats", "Cheats", true, false},
        {"settings", "Settings", true, false},
        {"quit_game", "Quit Game", true, false},
    };
    model_.command_bar = "ENT:OK 4:BK Q:QUIT";
}

const ScreenModel& PauseMenu::model() const
{
    return model_;
}

} // namespace czgba::ui
