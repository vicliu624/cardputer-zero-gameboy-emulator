#pragma once

#include <vector>

#include "cheat/cheat.hpp"
#include "ui/ui_screen.hpp"

namespace czgba::ui {

class CheatMenu {
public:
    void set_cheats(const std::vector<cheat::Cheat>& cheats);
    void set_selected_index(int index);

    const ScreenModel& model() const;

private:
    ScreenModel model_{app::AppMode::CheatMenu, "CHEATS"};
};

} // namespace czgba::ui
