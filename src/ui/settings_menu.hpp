#pragma once

#include "ui/ui_screen.hpp"

namespace czgba::ui {

class SettingsMenu {
public:
    SettingsMenu();

    const ScreenModel& model() const;

private:
    ScreenModel model_{app::AppMode::Settings, "SETTINGS"};
};

} // namespace czgba::ui
