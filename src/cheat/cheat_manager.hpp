#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "cheat/cheat.hpp"
#include "cheat/cheat_parser.hpp"

namespace czgba::cheat {

class CheatManager {
public:
    void load_for_rom(const std::filesystem::path& cheat_path);
    const std::vector<Cheat>& cheats() const;
    bool toggle(const std::string& id);
    void clear();

private:
    CheatParser parser_;
    std::vector<Cheat> cheats_;
};

} // namespace czgba::cheat
