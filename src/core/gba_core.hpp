#pragma once

#include <string>

#include "audio/audio_sample_batch.hpp"
#include "core/gba_input.hpp"
#include "core/gba_video_frame.hpp"

namespace czgba::core {

class GbaCore {
public:
    virtual ~GbaCore() = default;

    virtual bool load_rom(const std::string& rom_path) = 0;
    virtual void unload_rom() = 0;
    virtual void reset() = 0;
    virtual void run_until_next_frame() = 0;
    virtual void set_input(const GbaInputState& input) = 0;
    virtual GbaVideoFrame video_frame() const = 0;
    virtual audio::AudioSampleBatch read_audio_samples(int sample_rate, int max_frames) = 0;

    virtual bool load_sram(const std::string& path) = 0;
    virtual bool save_sram(const std::string& path) = 0;
    virtual bool load_state(const std::string& path) = 0;
    virtual bool save_state(const std::string& path) = 0;
    virtual bool load_cheats(const std::string& path) = 0;
    virtual bool set_cheat_enabled(const std::string& cheat_id, bool enabled) = 0;

    virtual std::string game_title() const = 0;
    virtual std::string game_code() const = 0;
};

} // namespace czgba::core
