#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace czgba::storage {

struct RomEntry {
    std::filesystem::path path;
    std::string display_name;
};

class RomScanner {
public:
    explicit RomScanner(std::filesystem::path working_directory);

    std::vector<RomEntry> scan() const;
    const std::vector<std::filesystem::path>& search_roots() const;

private:
    std::vector<std::filesystem::path> search_roots_;
};

std::string path_to_utf8(const std::filesystem::path& path);

} // namespace czgba::storage
