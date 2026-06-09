#include "ui/settings_menu.hpp"

namespace czgba::ui {

SettingsMenu::SettingsMenu()
{
    model_.items = {
        {"theme", "Theme: Minimal", true, false},
        {"audio", "Audio: On", true, true},
        {"stretch", "Stretch: Off", false, false},
    };
    model_.command_bar = "4:BK";
}

const ScreenModel& SettingsMenu::model() const
{
    return model_;
}

} // namespace czgba::ui
