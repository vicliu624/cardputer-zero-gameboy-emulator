#include "ui/cheat_menu.hpp"

#include <algorithm>

namespace czgba::ui {

void CheatMenu::set_cheats(const std::vector<cheat::Cheat>& cheats)
{
    model_.items.clear();
    for (const auto& cheat : cheats) {
        model_.items.push_back({
            cheat.id,
            cheat.name,
            true,
            cheat.enabled
        });
    }
    model_.command_bar = "ENT:TOG 4:BK";
    set_selected_index(model_.selected_index);
}

void CheatMenu::set_selected_index(int index)
{
    model_.selected_index = std::clamp(index, 0, std::max(0, static_cast<int>(model_.items.size()) - 1));
}

const ScreenModel& CheatMenu::model() const
{
    return model_;
}

} // namespace czgba::ui
