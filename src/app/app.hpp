#pragma once

#include <filesystem>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "app/command_bar.hpp"
#include "app/app_mode.hpp"
#include "app/toast.hpp"
#include "audio/audio_sample_batch.hpp"
#include "cheat/cheat_manager.hpp"
#include "core/gba_core.hpp"
#include "core/gba_video_frame.hpp"
#include "input/input_frame.hpp"
#include "storage/paths.hpp"
#include "storage/rom_scanner.hpp"

namespace czgba::app {

struct AppStatus {
    int battery_percent = 87;
    double fps = 59.7;
    int current_slot = 1;
    bool fast_forward = false;
    std::string toast;
};

struct RenderState {
    AppMode mode = AppMode::RomBrowser;
    const core::GbaVideoFrame* game_frame = nullptr;
    std::vector<storage::RomEntry> roms;
    int selected_rom = 0;
    int selected_pause_item = 0;
    AppStatus status;
    std::string error_message;
    std::string bottom_bar;
};

class App {
public:
    App(std::unique_ptr<core::GbaCore> core, std::filesystem::path working_directory);
    ~App();

    void start_with_rom(std::filesystem::path rom_path);
    // UI commands are always applied, but the emulation owner decides whether
    // this iteration may advance a frame. Audio-buffer pacing therefore no
    // longer depends on Wayland presentation cadence.
    void update(const input::InputFrame& input, bool advance_emulation = true);
    void tick(std::chrono::milliseconds elapsed);
    audio::AudioSampleBatch read_audio_samples(int sample_rate, int max_frames);
    RenderState render_state() const;
    bool should_quit() const;

private:
    void handle_action(AppAction action);
    void open_selected_rom();
    bool load_rom(const std::filesystem::path& rom_path);
    void close_current_rom();
    void save_state();
    void load_state();
    void load_cheats_for_current_rom();
    void toggle_selected_cheat();
    void activate_pause_item();
    void refresh_roms();
    void set_toast(std::string toast);
    std::string command_bar() const;

    std::unique_ptr<core::GbaCore> core_;
    storage::RomScanner rom_scanner_;
    storage::UserPaths paths_;
    cheat::CheatManager cheat_manager_;
    CommandBar command_bar_;
    Toast toast_;
    std::vector<storage::RomEntry> roms_;
    std::filesystem::path current_rom_;
    AppMode mode_ = AppMode::RomBrowser;
    int selected_rom_ = 0;
    int selected_pause_item_ = 0;
    int selected_cheat_ = 0;
    int current_slot_ = 1;
    bool fast_forward_ = false;
    bool should_quit_ = false;
    std::string error_message_;
    mutable core::GbaVideoFrame current_frame_{};
};

} // namespace czgba::app
