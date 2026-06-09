#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "cheat/cheat.hpp"

namespace czgba::cheat {

class CheatParser {
public:
    std::vector<Cheat> parse_file(const std::filesystem::path& path) const;
    std::vector<Cheat> parse_text(const std::string& text) const;
};

} // namespace czgba::cheat
