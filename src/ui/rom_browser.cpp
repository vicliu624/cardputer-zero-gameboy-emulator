#include "ui/rom_browser.hpp"

#include <algorithm>

namespace czgba::ui {

void RomBrowser::set_roms(std::vector<storage::RomEntry> roms)
{
    roms_ = std::move(roms);
    rebuild_model();
}

void RomBrowser::set_selected_index(int index)
{
    model_.selected_index = std::clamp(index, 0, std::max(0, static_cast<int>(roms_.size()) - 1));
}

const ScreenModel& RomBrowser::model() const
{
    return model_;
}

void RomBrowser::rebuild_model()
{
    model_.mode = app::AppMode::RomBrowser;
    model_.title = "GBA";
    model_.command_bar = "ENT:RUN 4:QUIT UP:SEL";
    model_.items.clear();

    for (std::size_t i = 0; i < roms_.size(); ++i) {
        model_.items.push_back({
            "rom:" + std::to_string(i),
            roms_[i].display_name,
            true,
            false
        });
    }
    set_selected_index(model_.selected_index);
}

} // namespace czgba::ui
