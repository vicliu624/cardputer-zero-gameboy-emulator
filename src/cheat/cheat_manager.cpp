#include "cheat/cheat_manager.hpp"

#include <algorithm>

namespace czgba::cheat {

void CheatManager::load_for_rom(const std::filesystem::path& cheat_path)
{
    cheats_ = parser_.parse_file(cheat_path);
}

const std::vector<Cheat>& CheatManager::cheats() const
{
    return cheats_;
}

bool CheatManager::toggle(const std::string& id)
{
    const auto it = std::find_if(cheats_.begin(), cheats_.end(), [&](const Cheat& cheat) {
        return cheat.id == id;
    });
    if (it == cheats_.end()) {
        return false;
    }

    it->enabled = !it->enabled;
    return true;
}

void CheatManager::clear()
{
    cheats_.clear();
}

} // namespace czgba::cheat
