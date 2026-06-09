#include "storage/rom_scanner.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace czgba::storage {
namespace {

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

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
    return {};
}

} // namespace

RomScanner::RomScanner(std::filesystem::path working_directory)
{
    search_roots_.push_back(working_directory / "rom");
    search_roots_.push_back(working_directory / "roms");

    const auto home = home_dir();
    if (!home.empty()) {
        search_roots_.push_back(home / ".local" / "share" / "cardputer-zero-gba" / "roms");
    }
}

std::vector<RomEntry> RomScanner::scan() const
{
    std::vector<RomEntry> entries;
    for (const auto& root : search_roots_) {
        if (!std::filesystem::exists(root) || !std::filesystem::is_directory(root)) {
            continue;
        }

        for (const auto& item : std::filesystem::directory_iterator(root)) {
            if (!item.is_regular_file()) {
                continue;
            }

            if (lower(item.path().extension().string()) != ".gba") {
                continue;
            }

            entries.push_back({item.path(), path_to_utf8(item.path().stem())});
        }
    }

    std::sort(entries.begin(), entries.end(), [](const RomEntry& a, const RomEntry& b) {
        return a.display_name < b.display_name;
    });
    return entries;
}

const std::vector<std::filesystem::path>& RomScanner::search_roots() const
{
    return search_roots_;
}

std::string path_to_utf8(const std::filesystem::path& path)
{
#ifdef _WIN32
    const auto str = path.u8string();
    return std::string(reinterpret_cast<const char*>(str.data()), str.size());
#else
    return path.string();
#endif
}

} // namespace czgba::storage
