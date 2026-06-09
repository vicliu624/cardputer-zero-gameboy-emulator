#include "config/app_config.hpp"

#include <cstdlib>

namespace czgba::config {
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

AppConfig default_config()
{
    return {};
}

std::filesystem::path default_config_path()
{
#ifdef _WIN32
    return home_dir() / "AppData" / "Roaming" / "cardputer-zero-gba" / "config.json";
#else
    return home_dir() / ".config" / "cardputer-zero-gba" / "config.json";
#endif
}

} // namespace czgba::config
