#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "app/app.hpp"

namespace {

class FakeCore final : public czgba::core::GbaCore {
public:
    bool load_rom(const std::string& rom_path) override
    {
        loaded_rom = rom_path;
        loaded = true;
        return true;
    }

    void unload_rom() override
    {
        loaded = false;
    }

    void reset() override {}
    void run_until_next_frame() override {}
    void set_input(const czgba::core::GbaInputState&) override {}

    czgba::core::GbaVideoFrame video_frame() const override
    {
        return {};
    }

    czgba::audio::AudioSampleBatch read_audio_samples(int sample_rate, int) override
    {
        return {sample_rate, {}};
    }

    bool load_sram(const std::string&) override
    {
        ++load_sram_calls;
        return true;
    }

    bool save_sram(const std::string&) override
    {
        ++save_sram_calls;
        return true;
    }

    bool load_state(const std::string&) override
    {
        ++load_state_calls;
        return true;
    }

    bool save_state(const std::string&) override
    {
        ++save_state_calls;
        return true;
    }

    bool load_cheats(const std::string&) override
    {
        ++load_cheats_calls;
        return true;
    }

    bool set_cheat_enabled(const std::string&, bool) override
    {
        return true;
    }

    std::string game_title() const override
    {
        return "TEST";
    }

    std::string game_code() const override
    {
        return "TEST";
    }

    std::string loaded_rom;
    bool loaded = false;
    int load_sram_calls = 0;
    int save_sram_calls = 0;
    int load_state_calls = 0;
    int save_state_calls = 0;
    int load_cheats_calls = 0;
};

void require(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "app menu smoke failed: " << message << '\n';
        std::exit(1);
    }
}

czgba::input::InputFrame action_frame(czgba::app::AppAction action)
{
    czgba::input::InputFrame frame;
    frame.actions.push_back(action);
    return frame;
}

} // namespace

int main()
{
    auto core = std::make_unique<FakeCore>();
    auto* fake_core = core.get();

    const auto work_dir = std::filesystem::temp_directory_path() / "cardputer-zero-gba-app-menu-smoke";
    std::filesystem::create_directories(work_dir);

    czgba::app::App app(std::move(core), work_dir);
    app.start_with_rom(work_dir / "test.gba");
    require(app.render_state().mode == czgba::app::AppMode::Playing, "starts playing after ROM load");

    app.update(action_frame(czgba::app::AppAction::OpenMenu));
    auto state = app.render_state();
    require(state.mode == czgba::app::AppMode::Paused, "menu key opens pause menu");
    require(state.selected_pause_item == 0, "pause menu starts on Resume");

    app.update(action_frame(czgba::app::AppAction::Down));
    app.update(action_frame(czgba::app::AppAction::Down));
    state = app.render_state();
    require(state.mode == czgba::app::AppMode::Paused, "Down stays in pause menu");
    require(state.selected_pause_item == 2, "Down moves pause selection");

    app.update(action_frame(czgba::app::AppAction::Up));
    state = app.render_state();
    require(state.selected_pause_item == 1, "Up moves pause selection");

    app.update(action_frame(czgba::app::AppAction::Confirm));
    state = app.render_state();
    require(state.mode == czgba::app::AppMode::Playing, "Save State returns to game");
    require(fake_core->save_state_calls == 1, "Save State invokes core save");

    app.update(action_frame(czgba::app::AppAction::OpenMenu));
    app.update(action_frame(czgba::app::AppAction::Down));
    app.update(action_frame(czgba::app::AppAction::Down));
    app.update(action_frame(czgba::app::AppAction::Confirm));
    state = app.render_state();
    require(state.mode == czgba::app::AppMode::Playing, "Load State returns to game");
    require(fake_core->load_state_calls == 1, "Load State invokes core load");

    app.update(action_frame(czgba::app::AppAction::OpenMenu));
    for (int i = 0; i < 4; ++i) {
        app.update(action_frame(czgba::app::AppAction::Down));
    }
    app.update(action_frame(czgba::app::AppAction::Confirm));
    state = app.render_state();
    require(state.mode == czgba::app::AppMode::Settings, "Settings item opens settings screen");

    app.update(action_frame(czgba::app::AppAction::Back));
    state = app.render_state();
    require(state.mode == czgba::app::AppMode::Paused, "Back returns from settings to pause menu");

    app.update(action_frame(czgba::app::AppAction::Back));
    state = app.render_state();
    require(state.mode == czgba::app::AppMode::Playing, "Back returns from pause menu to game");

    app.update(action_frame(czgba::app::AppAction::OpenMenu));
    app.update(action_frame(czgba::app::AppAction::Quit));
    state = app.render_state();
    require(state.mode == czgba::app::AppMode::RomBrowser, "Quit returns from pause menu to ROM browser");

    return 0;
}
