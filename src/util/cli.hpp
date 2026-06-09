#pragma once

#include <string>

namespace czgba::util {

struct CliOptions {
    bool help = false;
    bool version = false;
    bool fullscreen = false;
    bool kiosk = false;
    bool no_audio = false;
    int scale = 1;
    int max_frames = 0;
    std::string rom_path;
    std::string config_path;
    std::string theme_name;
    std::string log_level = "error";
};

bool parse_cli(int argc, char** argv, CliOptions& options, std::string& error);
void print_help(const char* executable_name);
void print_version();

} // namespace czgba::util
