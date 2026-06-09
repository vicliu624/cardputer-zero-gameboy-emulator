#pragma once

#include <vector>

#include "app/app_action.hpp"
#include "core/gba_input.hpp"

namespace czgba::input {

struct InputFrame {
    core::GbaInputState gba;
    std::vector<app::AppAction> actions;
    bool quit_requested = false;

    void begin_frame()
    {
        actions.clear();
        quit_requested = false;
    }
};

} // namespace czgba::input
