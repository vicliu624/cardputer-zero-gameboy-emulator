#pragma once

#include <string>
#include <vector>

#include "app/app_action.hpp"
#include "app/app_mode.hpp"

namespace czgba::ui {

struct MenuItem {
    std::string id;
    std::string label;
    bool enabled = true;
    bool checked = false;
};

struct ScreenModel {
    app::AppMode mode = app::AppMode::RomBrowser;
    std::string title;
    std::vector<MenuItem> items;
    int selected_index = 0;
    std::string command_bar;
};

class UiScreen {
public:
    virtual ~UiScreen() = default;

    virtual app::AppMode mode() const = 0;
    virtual const ScreenModel& model() const = 0;
    virtual void handle_action(app::AppAction action) = 0;
};

} // namespace czgba::ui
