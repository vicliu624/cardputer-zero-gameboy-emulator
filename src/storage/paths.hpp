#pragma once

#include <filesystem>

namespace czgba::storage {

struct UserPaths {
    std::filesystem::path data_root;
    std::filesystem::path roms;
    std::filesystem::path saves;
    std::filesystem::path states;
    std::filesystem::path cheats;
    std::filesystem::path themes;
};

UserPaths default_user_paths();
void ensure_user_paths(const UserPaths& paths);
std::filesystem::path save_path_for_rom(const UserPaths& paths, const std::filesystem::path& rom_path);
std::filesystem::path state_path_for_rom(const UserPaths& paths, const std::filesystem::path& rom_path, int slot);
std::filesystem::path path_from_utf8(const std::string& path);

} // namespace czgba::storage
