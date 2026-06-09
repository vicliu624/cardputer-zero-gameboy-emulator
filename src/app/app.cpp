#include "app/app.hpp"

#include <algorithm>
#include <iostream>

namespace czgba::app {
App::App(std::unique_ptr<core::GbaCore> core, std::filesystem::path working_directory)
    : core_(std::move(core))
    , rom_scanner_(std::move(working_directory))
    , paths_(storage::default_user_paths())
{
    storage::ensure_user_paths(paths_);
    refresh_roms();
}

App::~App()
{
    close_current_rom();
}

void App::start_with_rom(std::filesystem::path rom_path)
{
    load_rom(std::move(rom_path));
}

void App::tick(std::chrono::milliseconds elapsed)
{
    toast_.update(elapsed);
}

void App::update(const input::InputFrame& input)
{
    if (input.quit_requested) {
        should_quit_ = true;
    }

    for (const auto action : input.actions) {
        handle_action(action);
    }

    if (mode_ == AppMode::Playing) {
        core_->set_input(input.gba);
        const int frames = fast_forward_ ? 2 : 1;
        for (int i = 0; i < frames; ++i) {
            core_->run_until_next_frame();
        }
    }
}

audio::AudioSampleBatch App::read_audio_samples(int sample_rate, int max_frames)
{
    if (mode_ != AppMode::Playing || core_ == nullptr) {
        return {sample_rate, {}};
    }
    auto batch = core_->read_audio_samples(sample_rate, max_frames);
    if (fast_forward_) {
        batch.interleaved_s16.clear();
    }
    return batch;
}

RenderState App::render_state() const
{
    current_frame_ = core_->video_frame();
    return {
        mode_,
        current_frame_.pixels_xrgb8888 == nullptr ? nullptr : &current_frame_,
        roms_,
        selected_rom_,
        {87, fast_forward_ ? 119.4 : 59.7, current_slot_, fast_forward_, toast_.visible() ? toast_.text() : ""},
        error_message_,
        command_bar()
    };
}

bool App::should_quit() const
{
    return should_quit_;
}

void App::handle_action(AppAction action)
{
    switch (mode_) {
    case AppMode::RomBrowser:
        if (action == AppAction::Up && selected_rom_ > 0) {
            --selected_rom_;
        } else if (action == AppAction::Down && selected_rom_ + 1 < static_cast<int>(roms_.size())) {
            ++selected_rom_;
        } else if (action == AppAction::Confirm) {
            open_selected_rom();
        } else if (action == AppAction::OpenMenu || action == AppAction::Quit) {
            should_quit_ = true;
        }
        break;

    case AppMode::Playing:
        if (action == AppAction::OpenMenu) {
            mode_ = AppMode::Paused;
        } else if (action == AppAction::SaveState) {
            save_state();
        } else if (action == AppAction::LoadState) {
            load_state();
        } else if (action == AppAction::ToggleFastForward) {
            fast_forward_ = !fast_forward_;
            set_toast(fast_forward_ ? "FF ON" : "FF OFF");
        } else if (action == AppAction::OpenCheatMenu) {
            if (cheat_manager_.cheats().empty()) {
                set_toast("NO CH");
            } else {
                mode_ = AppMode::CheatMenu;
            }
        }
        break;

    case AppMode::Paused:
        if (action == AppAction::OpenMenu || action == AppAction::Back || action == AppAction::Confirm) {
            mode_ = AppMode::Playing;
        } else if (action == AppAction::Quit) {
            close_current_rom();
            refresh_roms();
            mode_ = AppMode::RomBrowser;
        }
        break;

    case AppMode::CheatMenu:
        if (action == AppAction::Up && selected_cheat_ > 0) {
            --selected_cheat_;
        } else if (action == AppAction::Down && selected_cheat_ + 1 < static_cast<int>(cheat_manager_.cheats().size())) {
            ++selected_cheat_;
        } else if (action == AppAction::Confirm) {
            toggle_selected_cheat();
        } else if (action == AppAction::OpenMenu || action == AppAction::Back) {
            mode_ = AppMode::Paused;
        }
        break;

    case AppMode::Error:
        if (action == AppAction::OpenMenu || action == AppAction::Back || action == AppAction::Confirm) {
            mode_ = AppMode::RomBrowser;
        }
        break;

    default:
        break;
    }
}

void App::open_selected_rom()
{
    if (roms_.empty()) {
        return;
    }
    selected_rom_ = std::clamp(selected_rom_, 0, static_cast<int>(roms_.size()) - 1);
    load_rom(roms_[static_cast<std::size_t>(selected_rom_)].path);
}

bool App::load_rom(const std::filesystem::path& rom_path)
{
    close_current_rom();

    current_rom_ = rom_path;
    error_message_.clear();
    toast_.clear();

    const auto rom_path_utf8 = storage::path_to_utf8(rom_path);

    if (!core_->load_rom(rom_path_utf8)) {
        error_message_ = "ROM LOAD FAIL";
        mode_ = AppMode::Error;
        return false;
    }

    const auto save_path = storage::save_path_for_rom(paths_, current_rom_);
    if (std::filesystem::exists(save_path)) {
        core_->load_sram(storage::path_to_utf8(save_path));
    }
    load_cheats_for_current_rom();

    mode_ = AppMode::Playing;
    set_toast("ROM OK");
    std::cout << "Loaded ROM: " << storage::path_to_utf8(rom_path) << '\n';
    return true;
}

void App::close_current_rom()
{
    if (!current_rom_.empty()) {
        core_->save_sram(storage::path_to_utf8(storage::save_path_for_rom(paths_, current_rom_)));
    }
    core_->unload_rom();
    cheat_manager_.clear();
    current_rom_.clear();
}

void App::save_state()
{
    if (current_rom_.empty()) {
        return;
    }
    const auto path = storage::state_path_for_rom(paths_, current_rom_, current_slot_);
    set_toast(core_->save_state(storage::path_to_utf8(path)) ? "SAVED S1" : "SAVE FAIL");
}

void App::load_state()
{
    if (current_rom_.empty()) {
        return;
    }
    const auto path = storage::state_path_for_rom(paths_, current_rom_, current_slot_);
    set_toast(core_->load_state(storage::path_to_utf8(path)) ? "LOADED S1" : "LOAD FAIL");
}

void App::load_cheats_for_current_rom()
{
    if (current_rom_.empty()) {
        cheat_manager_.clear();
        return;
    }

    auto cheat_name = current_rom_.stem();
    cheat_name += ".cht";
    const auto cheat_path = paths_.cheats / cheat_name;
    cheat_manager_.load_for_rom(cheat_path);
    if (!cheat_manager_.cheats().empty()) {
        core_->load_cheats(storage::path_to_utf8(cheat_path));
    }
    selected_cheat_ = 0;
}

void App::toggle_selected_cheat()
{
    const auto& cheats = cheat_manager_.cheats();
    if (cheats.empty()) {
        set_toast("NO CH");
        return;
    }

    selected_cheat_ = std::clamp(selected_cheat_, 0, static_cast<int>(cheats.size()) - 1);
    const auto id = cheats[static_cast<std::size_t>(selected_cheat_)].id;
    if (!cheat_manager_.toggle(id)) {
        set_toast("CH FAIL");
        return;
    }

    const auto enabled = cheat_manager_.cheats()[static_cast<std::size_t>(selected_cheat_)].enabled;
    core_->set_cheat_enabled(id, enabled);
    set_toast(enabled ? "CH ON" : "CH OFF");
}

void App::refresh_roms()
{
    roms_ = rom_scanner_.scan();
    selected_rom_ = std::clamp(selected_rom_, 0, std::max(0, static_cast<int>(roms_.size()) - 1));
}

void App::set_toast(std::string toast)
{
    toast_.show(std::move(toast));
}

std::string App::command_bar() const
{
    return command_bar_.text_for(mode_);
}

} // namespace czgba::app
