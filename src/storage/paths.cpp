#include "storage/paths.hpp"

#include <cstdlib>
#include <string>

namespace czgba::storage {
namespace {

std::filesystem::path home_dir()
{
#ifdef _WIN32
    if (const char* user_profile = std::getenv("USERPROFILE")) {
        return user_profile;
    }
#else
    if (const char* home = std::getenv("HOME")) {
        return home;
    }
#endif
    return std::filesystem::current_path();
}

} // namespace

UserPaths default_user_paths()
{
    const auto root = home_dir() / ".local" / "share" / "cardputer-zero-gba";
    return {
        root,
        root / "roms",
        root / "saves",
        root / "states",
        root / "cheats",
        root / "themes",
    };
}

void ensure_user_paths(const UserPaths& paths)
{
    std::filesystem::create_directories(paths.roms);
    std::filesystem::create_directories(paths.saves);
    std::filesystem::create_directories(paths.states);
    std::filesystem::create_directories(paths.cheats);
    std::filesystem::create_directories(paths.themes);
}

std::filesystem::path save_path_for_rom(const UserPaths& paths, const std::filesystem::path& rom_path)
{
    auto name = rom_path.stem();
    name += ".sav";
    return paths.saves / name;
}

std::filesystem::path state_path_for_rom(const UserPaths& paths, const std::filesystem::path& rom_path, int slot)
{
    auto name = rom_path.stem();
    name += ".slot" + std::to_string(slot) + ".state";
    return paths.states / name;
}

std::filesystem::path path_from_utf8(const std::string& path)
{
#ifdef _WIN32
    return std::filesystem::path(std::u8string(path.begin(), path.end()));
#else
    return std::filesystem::path(path);
#endif
}

} // namespace czgba::storage
