#pragma once

#include "ui/ui_screen.hpp"

namespace czgba::ui {

class PauseMenu {
public:
    PauseMenu();

    const ScreenModel& model() const;

private:
    ScreenModel model_{app::AppMode::Paused, "PAUSED"};
};

} // namespace czgba::ui
