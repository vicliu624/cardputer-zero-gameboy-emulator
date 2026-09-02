#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/gba_core.hpp"

struct mCore;
struct mStandardLogger;

namespace czgba::core {

enum class MgbaLogLevel {
    Error,
    Warn,
    Info,
    Debug
};

class MgbaCore final : public GbaCore {
public:
    explicit MgbaCore(MgbaLogLevel log_level = MgbaLogLevel::Error);
    ~MgbaCore() override;

    MgbaCore(const MgbaCore&) = delete;
    MgbaCore& operator=(const MgbaCore&) = delete;

    bool load_rom(const std::string& rom_path) override;
    void unload_rom() override;
    void reset() override;
    void run_until_next_frame() override;
    void set_input(const GbaInputState& input) override;
    GbaVideoFrame video_frame() const override;
    audio::AudioSampleBatch read_audio_samples(int sample_rate, int max_frames) override;

    bool load_sram(const std::string& path) override;
    bool save_sram(const std::string& path) override;
    bool load_state(const std::string& path) override;
    bool save_state(const std::string& path) override;
    bool load_cheats(const std::string& path) override;
    bool set_cheat_enabled(const std::string& cheat_id, bool enabled) override;

    std::string game_title() const override;
    std::string game_code() const override;

private:
    void destroy_core();
    void ensure_logger();
    void release_logger();
    // mGBA advances at the emulation clock, while the TDVP presenter may
    // intentionally coalesce several emulated frames into one Wayland
    // commit. Convert the native framebuffer only when a consumer asks for
    // it, so skipped presentations do not compete with audio output.
    void normalize_video() const;
    void configure_audio(int sample_rate);
    void append_audio_samples(int max_frames);
    static std::uint32_t input_to_mgba_keys(const GbaInputState& input);

    mCore* core_ = nullptr;
    mStandardLogger* logger_ = nullptr;
    bool config_initialized_ = false;
    MgbaLogLevel log_level_ = MgbaLogLevel::Error;
    unsigned width_ = 240;
    unsigned height_ = 160;
    unsigned native_pitch_ = 256;
    int audio_sample_rate_ = 48000;
    std::vector<std::uint32_t> mgba_native_video_;
    mutable std::vector<std::uint32_t> xrgb8888_video_;
    mutable bool video_normalization_pending_ = true;
    audio::AudioSampleBatch pending_audio_;
};

} // namespace czgba::core
