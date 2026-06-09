#pragma once

#include <string>

#include "app/app_mode.hpp"

namespace czgba::app {

class CommandBar {
public:
    std::string text_for(AppMode mode) const;
};

} // namespace czgba::app
