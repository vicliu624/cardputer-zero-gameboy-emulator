#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#ifndef CZ_GBA_SOURCE_ROOT
#define CZ_GBA_SOURCE_ROOT "."
#endif

namespace {

std::string read_file(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.good()) {
        std::cerr << "failed to read " << path << '\n';
        std::exit(1);
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool contains(const std::string& haystack, const std::string& needle)
{
    return haystack.find(needle) != std::string::npos;
}

void require(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "packaging contract failed: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main()
{
    const std::filesystem::path root = CZ_GBA_SOURCE_ROOT;

    const auto desktop = read_file(root / "desktop" / "cardputer-zero-gba.desktop");
    require(contains(desktop, "Name=GBE"), "desktop name");
    require(contains(desktop, "Exec=/usr/lib/cardputer-zero-gba/cardputer-zero-gba-applaunch"), "desktop exec");
    require(contains(desktop, "Icon=share/images/cardputer-zero-gba.png"), "desktop icon");
    require(contains(desktop, "X-Zero-AppId=io.github.vicliu624.cardputer-zero-gba"), "desktop app id");
    require(contains(desktop, "X-Zero-Display=wayland"), "desktop display backend");

    const auto launcher = read_file(root / "packaging" / "cardputer-zero-gba-applaunch");
    require(contains(launcher, "CARDPUTER_ZERO_GBA_DISPLAY_BACKEND"), "launcher display env");
    require(contains(launcher, "SDL_VIDEODRIVER:=wayland"), "launcher wayland default");
    require(contains(launcher, "SDL_VIDEO_WAYLAND_WMCLASS=io.github.vicliu624.cardputer-zero-gba"), "launcher wmclass");
    require(contains(launcher, "--kiosk"), "launcher kiosk flag");
    require(contains(launcher, "/usr/lib/cardputer-zero-gba"), "launcher private libdir");

    const auto sdl_audio = read_file(root / "src" / "platform" / "sdl_audio.cpp");
    require(contains(sdl_audio, "desired.callback = nullptr"), "queued audio device");
    require(contains(sdl_audio, "SDL_QueueAudio"), "queued audio write path");
    require(contains(sdl_audio, "SDL_GetQueuedAudioSize"), "queued audio size check");
    require(contains(sdl_audio, "start_buffer_samples_"), "audio prebuffer");
    require(contains(sdl_audio, "playback_started_"), "audio playback start state");
    require(contains(sdl_audio, "queue_limit_samples_"), "audio queue limit");
    require(!contains(sdl_audio, "audio_callback"), "audio callback must not be used");
    require(!contains(sdl_audio, "SDL_LockAudioDevice"), "audio callback lock must not be used");

    const auto mgba_core = read_file(root / "src" / "core" / "mgba_core.cpp");
    require(contains(mgba_core, "core_->reset(core_);\n    configure_audio(audio_sample_rate_);"), "audio configured after reset");
    require(contains(mgba_core, "blip_samples_avail(right)"), "audio reads both channels");

    const auto input_mapper = read_file(root / "src" / "input" / "input_mapper.cpp");
    require(contains(input_mapper, "case SDLK_4:"), "key 4 mapping");
    require(contains(input_mapper, "case SDLK_5:"), "key 5 mapping");
    require(contains(input_mapper, "case SDLK_6:"), "key 6 mapping");
    require(contains(input_mapper, "case SDLK_7:"), "key 7 mapping");
    require(contains(input_mapper, "case SDLK_8:"), "key 8 mapping");
    require(!contains(input_mapper, "case SDLK_ESCAPE:"), "old ESC shortcut removed");
    require(!contains(input_mapper, "case SDLK_1:"), "old key 1 shortcut removed");
    require(!contains(input_mapper, "case SDLK_2:"), "old key 2 shortcut removed");
    require(!contains(input_mapper, "case SDLK_f:"), "old F shortcut removed");
    require(!contains(input_mapper, "case SDLK_c:"), "old C shortcut removed");

    const auto command_bar = read_file(root / "src" / "app" / "command_bar.cpp");
    require(contains(command_bar, "MENU|SAVE|LOAD|FAST|CHEATS"), "command bar labels");

    const auto app = read_file(root / "src" / "app" / "app.cpp");
    require(contains(app, "batch.interleaved_s16.clear()"), "FAST audio drain without queueing compressed audio");

    const auto renderer = read_file(root / "src" / "render" / "renderer.cpp");
    const auto sdl_platform = read_file(root / "src" / "platform" / "sdl_platform.cpp");
    require(contains(sdl_platform, "SDL_RENDERER_ACCELERATED"), "accelerated renderer");
    require(!contains(sdl_platform, "SDL_RENDERER_PRESENTVSYNC"), "renderer vsync removed");
    require(contains(renderer, "{0, 79}"), "bottom slot 1");
    require(contains(renderer, "{79, 54}"), "bottom slot 2");
    require(contains(renderer, "{133, 54}"), "bottom slot 3");
    require(contains(renderer, "{187, 54}"), "bottom slot 4");
    require(contains(renderer, "{241, 79}"), "bottom slot 5");
    require(contains(renderer, "draw_centered_text(left_content_x, 84"), "left centered status");
    require(contains(renderer, "status.fast_forward ? \"2X\" : \"1X\""), "FAST ratio status");
    require(!contains(renderer, "\"FPS\""), "FPS label removed");
    require(contains(renderer, "draw_centered_text(right_content_x, 50"), "right hint centered A/B");
    require(contains(renderer, "draw_centered_text(right_content_x, 130"), "right hint centered L/R");

    const auto cmake = read_file(root / "CMakeLists.txt");
    require(contains(cmake, "APPLaunch/applications"), "APPLaunch desktop install path");
    require(contains(cmake, "APPLaunch/share/images"), "APPLaunch icon install path");
    require(contains(cmake, "lib/cardputer-zero-gba"), "private libdir install path");
    require(contains(cmake, "packaging/cardputer-zero-gba-applaunch"), "APPLaunch wrapper installed");

    require(std::filesystem::exists(root / "packaging" / "cardputer-zero-gba.png"), "APPLaunch icon exists");
    require(std::filesystem::exists(root / "assets" / "icons" / "cardputer-zero-gba-64.png"), "64px icon exists");
    require(std::filesystem::exists(root / "assets" / "icons" / "cardputer-zero-gba-128.png"), "128px icon exists");
    require(std::filesystem::exists(root / "assets" / "themes" / "minimal" / "theme.json"), "theme exists");
    require(std::filesystem::exists(root / "assets" / "themes" / "minimal" / "bezel.png"), "bezel exists");

    return 0;
}
