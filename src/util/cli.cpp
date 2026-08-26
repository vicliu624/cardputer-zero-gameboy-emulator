#include "util/cli.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

#ifndef CARDPUTER_ZERO_GBA_VERSION
#define CARDPUTER_ZERO_GBA_VERSION "0.0.0"
#endif

namespace czgba::util {
namespace {

bool requires_value(std::string_view arg)
{
    return arg == "--scale" || arg == "--frames" || arg == "--rom" || arg == "--config" ||
           arg == "--theme" || arg == "--log-level" || arg == "--device-profile";
}

bool is_valid_log_level(std::string_view value)
{
    return value == "debug" || value == "info" || value == "warn" || value == "error";
}

} // namespace

bool parse_cli(int argc, char** argv, CliOptions& options, std::string& error)
{
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);

        if (arg == "--help") {
            options.help = true;
        } else if (arg == "--version") {
            options.version = true;
        } else if (arg == "--fullscreen") {
            options.fullscreen = true;
        } else if (arg == "--kiosk") {
            options.kiosk = true;
        } else if (arg == "--no-audio") {
            options.no_audio = true;
        } else if (requires_value(arg)) {
            if (i + 1 >= argc) {
                error = std::string(arg) + " requires a value";
                return false;
            }

            const std::string value(argv[++i]);
            if (arg == "--scale") {
                char* end = nullptr;
                const long parsed = std::strtol(value.c_str(), &end, 10);
                if (end == value.c_str() || *end != '\0' || parsed != 1) {
                    error = "--scale is fixed at 1 because the app window is always 320x170";
                    return false;
                }
                options.scale = static_cast<int>(parsed);
            } else if (arg == "--frames") {
                char* end = nullptr;
                const long parsed = std::strtol(value.c_str(), &end, 10);
                if (end == value.c_str() || *end != '\0' || parsed < 0) {
                    error = "--frames must be a non-negative integer";
                    return false;
                }
                options.max_frames = static_cast<int>(parsed);
            } else if (arg == "--device-profile") {
                if (value != "cardputer-zero" && value != "tdvp-k230") {
                    error = "--device-profile must be cardputer-zero or tdvp-k230";
                    return false;
                }
                options.device_profile = value;
            } else if (arg == "--rom") {
                options.rom_path = value;
            } else if (arg == "--config") {
                options.config_path = value;
            } else if (arg == "--theme") {
                options.theme_name = value;
            } else if (arg == "--log-level") {
                if (!is_valid_log_level(value)) {
                    error = "--log-level must be debug, info, warn, or error";
                    return false;
                }
                options.log_level = value;
            }
        } else {
            error = "unknown option: " + std::string(arg);
            return false;
        }
    }

    return true;
}

void print_help(const char* executable_name)
{
    std::cout
        << "Usage: " << executable_name << " [options]\n\n"
        << "Options:\n"
        << "  --help                         Show this help text\n"
        << "  --version                      Show app version\n"
        << "  --scale 1                      Compatibility option; window is always 320x170\n"
        << "  --frames <n>                   Run n presented frames, then exit\n"
        << "  --fullscreen                   Use a fullscreen SDL window\n"
        << "  --kiosk                        Use a borderless fixed 320x170 presentation surface\n"
        << "  --device-profile <name>        cardputer-zero (default) or tdvp-k230\n"
        << "  --no-audio                     Run without opening an SDL audio device\n"
        << "  --rom <path>                   Start the given GBA ROM directly\n"
        << "  --config <path>                Reserved config override path\n"
        << "  --theme <name>                 Reserved theme override name\n"
        << "  --log-level <debug|info|warn|error>\n\n"
        << "Starts in ROM Browser unless --rom is supplied.\n";
}

void print_version()
{
    std::cout << "cardputer-zero-gba " << CARDPUTER_ZERO_GBA_VERSION << '\n';
}

} // namespace czgba::util
