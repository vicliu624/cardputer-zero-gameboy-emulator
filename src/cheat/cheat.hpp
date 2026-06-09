#pragma once

#include <string>
#include <vector>

namespace czgba::cheat {

struct Cheat {
    std::string id;
    std::string name;
    std::vector<std::string> codes;
    bool enabled = false;
};

} // namespace czgba::cheat
