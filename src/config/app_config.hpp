#pragma once

#include <filesystem>
#include <string>

namespace czgba::config {

struct DisplayConfig {
    std::string theme = "minimal";
    bool show_left_status = true;
    bool show_right_hints = true;
    bool show_bottom_bar = true;
};

struct AudioConfig {
    int volume = 80;
    bool mute_when_paused = false;
    int sample_rate = 48000;
};

struct EmulationConfig {
    int fast_forward_speed = 2;
};

struct AppConfig {
    DisplayConfig display;
    AudioConfig audio;
    EmulationConfig emulation;
};

AppConfig default_config();
std::filesystem::path default_config_path();

} // namespace czgba::config
