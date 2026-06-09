#pragma once

#include <vector>

#include "storage/rom_scanner.hpp"
#include "ui/ui_screen.hpp"

namespace czgba::ui {

class RomBrowser {
public:
    void set_roms(std::vector<storage::RomEntry> roms);
    void set_selected_index(int index);

    const ScreenModel& model() const;

private:
    void rebuild_model();

    std::vector<storage::RomEntry> roms_;
    ScreenModel model_{app::AppMode::RomBrowser, "GBA"};
};

} // namespace czgba::ui
