#include "core/mgba_core.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>

extern "C" {
#include <mgba/core/blip_buf.h>
#include <mgba/core/config.h>
#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba/core/serialize.h>
#include <mgba/internal/gba/input.h>
#include <mgba-util/vfs.h>
}

namespace czgba::core {
namespace {

constexpr int kMaxAudioFramesPerEmulatedFrame = 4096;

int mgba_log_mask(MgbaLogLevel level)
{
    switch (level) {
    case MgbaLogLevel::Debug:
        return mLOG_ALL;
    case MgbaLogLevel::Info:
        return mLOG_FATAL | mLOG_ERROR | mLOG_WARN | mLOG_INFO;
    case MgbaLogLevel::Warn:
        return mLOG_FATAL | mLOG_ERROR | mLOG_WARN;
    case MgbaLogLevel::Error:
    default:
        return mLOG_FATAL | mLOG_ERROR;
    }
}

} // namespace

MgbaCore::MgbaCore(MgbaLogLevel log_level)
    : log_level_(log_level)
{
}

MgbaCore::~MgbaCore()
{
    destroy_core();
    release_logger();
}

bool MgbaCore::load_rom(const std::string& rom_path)
{
    destroy_core();
    ensure_logger();

    core_ = mCoreFind(rom_path.c_str());
    if (core_ == nullptr) {
        std::cerr << "mCoreFind failed for ROM: " << rom_path << '\n';
        return false;
    }

    if (!core_->init(core_)) {
        std::cerr << "mGBA core init failed\n";
        destroy_core();
        return false;
    }
    mCoreInitConfig(core_, "cardputer-zero-gba");
    config_initialized_ = true;
    mCoreOptions opts{};
    opts.skipBios = true;
    opts.audioBuffers = 4096;
    opts.sampleRate = static_cast<unsigned>(audio_sample_rate_);
    opts.audioSync = false;
    opts.videoSync = false;
    opts.volume = 0x100;
    opts.logLevel = mgba_log_mask(log_level_);
    mCoreConfigLoadDefaults(&core_->config, &opts);
    mCoreLoadConfig(core_);
    mCoreConfigSetOverrideIntValue(&core_->config, "logLevel", opts.logLevel);
    mCoreConfigSetOverrideIntValue(&core_->config, "logLevel.gba.dma", opts.logLevel);
    mCoreConfigSetOverrideIntValue(&core_->config, "logLevel.gba.memory", opts.logLevel);
    mCoreConfigSetOverrideIntValue(&core_->config, "logLevel.gba.bios", opts.logLevel);
    mCoreConfigSetOverrideIntValue(&core_->config, "logLevel.gba.video", opts.logLevel);
    mCoreLoadForeignConfig(core_, &core_->config);
    if (logger_ != nullptr) {
        mStandardLoggerConfig(logger_, &core_->config);
        logger_->logToStdout = true;
    }

    core_->desiredVideoDimensions(core_, &width_, &height_);
    if (width_ == 0 || height_ == 0) {
        width_ = 240;
        height_ = 160;
    }
    native_pitch_ = std::max<unsigned>(256, width_);

    mgba_native_video_.assign(static_cast<std::size_t>(native_pitch_) * height_, 0);
    xrgb8888_video_.assign(static_cast<std::size_t>(width_) * height_, 0);
    core_->setVideoBuffer(core_, reinterpret_cast<color_t*>(mgba_native_video_.data()), native_pitch_);

    if (!mCoreLoadFile(core_, rom_path.c_str())) {
        std::cerr << "mCoreLoadFile failed for ROM: " << rom_path << '\n';
        destroy_core();
        return false;
    }

    video_normalization_pending_ = true;
    core_->reset(core_);
    configure_audio(audio_sample_rate_);
    return true;
}

void MgbaCore::unload_rom()
{
    destroy_core();
}

void MgbaCore::reset()
{
    if (core_ != nullptr) {
        pending_audio_.interleaved_s16.clear();
        video_normalization_pending_ = true;
        core_->reset(core_);
        configure_audio(audio_sample_rate_);
    }
}

void MgbaCore::run_until_next_frame()
{
    if (core_ == nullptr) {
        return;
    }

    core_->runFrame(core_);
    video_normalization_pending_ = true;
    append_audio_samples(kMaxAudioFramesPerEmulatedFrame);
}

void MgbaCore::set_input(const GbaInputState& input)
{
    if (core_ != nullptr) {
        core_->setKeys(core_, input_to_mgba_keys(input));
    }
}

GbaVideoFrame MgbaCore::video_frame() const
{
    if (video_normalization_pending_ && !mgba_native_video_.empty()) {
        normalize_video();
        video_normalization_pending_ = false;
    }
    return {
        xrgb8888_video_.empty() ? nullptr : xrgb8888_video_.data(),
        static_cast<int>(width_),
        static_cast<int>(height_),
        static_cast<int>(width_)
    };
}

audio::AudioSampleBatch MgbaCore::read_audio_samples(int sample_rate, int max_frames)
{
    audio::AudioSampleBatch batch;
    batch.sample_rate = sample_rate;

    if (max_frames <= 0) {
        return batch;
    }

    if (sample_rate != audio_sample_rate_) {
        configure_audio(sample_rate);
    }

    if (pending_audio_.interleaved_s16.empty()) {
        return batch;
    }

    const auto max_samples =
        static_cast<std::size_t>(max_frames) * static_cast<std::size_t>(audio::AudioSampleBatch::Channels);
    const auto samples_to_copy = std::min(max_samples, pending_audio_.interleaved_s16.size());
    batch.interleaved_s16.assign(
        pending_audio_.interleaved_s16.begin(),
        pending_audio_.interleaved_s16.begin() + static_cast<std::ptrdiff_t>(samples_to_copy));
    pending_audio_.interleaved_s16.erase(
        pending_audio_.interleaved_s16.begin(),
        pending_audio_.interleaved_s16.begin() + static_cast<std::ptrdiff_t>(samples_to_copy));
    return batch;
}

bool MgbaCore::load_sram(const std::string& path)
{
    if (core_ == nullptr) {
        return false;
    }
    return mCoreLoadSaveFile(core_, path.c_str(), false);
}

bool MgbaCore::save_sram(const std::string& path)
{
    if (core_ == nullptr || core_->savedataClone == nullptr) {
        return false;
    }

    void* sram = nullptr;
    const size_t size = core_->savedataClone(core_, &sram);
    if (size == 0 || sram == nullptr) {
        return false;
    }

    VFile* vf = VFileOpen(path.c_str(), O_CREAT | O_TRUNC | O_WRONLY);
    if (vf == nullptr) {
        std::free(sram);
        return false;
    }

    const ssize_t written = vf->write(vf, sram, size);
    vf->close(vf);
    std::free(sram);
    return written == static_cast<ssize_t>(size);
}

bool MgbaCore::load_state(const std::string& path)
{
    if (core_ == nullptr) {
        return false;
    }

    VFile* vf = VFileOpen(path.c_str(), O_RDONLY);
    if (vf == nullptr) {
        return false;
    }
    const bool ok = mCoreLoadStateNamed(core_, vf, SAVESTATE_SAVEDATA | SAVESTATE_RTC);
    vf->close(vf);
    return ok;
}

bool MgbaCore::save_state(const std::string& path)
{
    if (core_ == nullptr) {
        return false;
    }

    VFile* vf = VFileOpen(path.c_str(), O_CREAT | O_TRUNC | O_WRONLY);
    if (vf == nullptr) {
        return false;
    }
    const bool ok = mCoreSaveStateNamed(core_, vf, SAVESTATE_SAVEDATA | SAVESTATE_RTC);
    vf->close(vf);
    return ok;
}

bool MgbaCore::load_cheats(const std::string& path)
{
    (void)path;
    return false;
}

bool MgbaCore::set_cheat_enabled(const std::string& cheat_id, bool enabled)
{
    (void)cheat_id;
    (void)enabled;
    return false;
}

std::string MgbaCore::game_title() const
{
    if (core_ == nullptr || core_->getGameTitle == nullptr) {
        return {};
    }
    char title[17]{};
    core_->getGameTitle(core_, title);
    return title;
}

std::string MgbaCore::game_code() const
{
    if (core_ == nullptr || core_->getGameCode == nullptr) {
        return {};
    }
    char code[17]{};
    core_->getGameCode(core_, code);
    return code;
}

void MgbaCore::destroy_core()
{
    if (core_ != nullptr) {
        core_->unloadROM(core_);
        if (config_initialized_) {
            mCoreConfigDeinit(&core_->config);
            config_initialized_ = false;
        }
        core_->deinit(core_);
        core_ = nullptr;
    }
    mgba_native_video_.clear();
    xrgb8888_video_.clear();
    video_normalization_pending_ = true;
    pending_audio_.interleaved_s16.clear();
    pending_audio_.sample_rate = audio_sample_rate_;
}

void MgbaCore::ensure_logger()
{
    if (logger_ != nullptr) {
        return;
    }

    logger_ = new mStandardLogger{};
    mStandardLoggerInit(logger_);
    mLogSetDefaultLogger(&logger_->d);
}

void MgbaCore::release_logger()
{
    if (logger_ == nullptr) {
        return;
    }

    if (mLogGetContext() == &logger_->d) {
        mLogSetDefaultLogger(nullptr);
    }
    mStandardLoggerDeinit(logger_);
    delete logger_;
    logger_ = nullptr;
}

void MgbaCore::normalize_video() const
{
    const std::size_t desired = static_cast<std::size_t>(width_) * height_;
    if (xrgb8888_video_.size() != desired) {
        xrgb8888_video_.resize(desired);
    }

    for (unsigned y = 0; y < height_; ++y) {
        for (unsigned x = 0; x < width_; ++x) {
            const std::uint32_t native = mgba_native_video_[static_cast<std::size_t>(y) * native_pitch_ + x];
            const std::uint8_t r = static_cast<std::uint8_t>(native & 0xffu);
            const std::uint8_t g = static_cast<std::uint8_t>((native >> 8u) & 0xffu);
            const std::uint8_t b = static_cast<std::uint8_t>((native >> 16u) & 0xffu);
            xrgb8888_video_[static_cast<std::size_t>(y) * width_ + x] =
                (static_cast<std::uint32_t>(r) << 16u) |
                (static_cast<std::uint32_t>(g) << 8u) |
                static_cast<std::uint32_t>(b);
        }
    }
}

void MgbaCore::configure_audio(int sample_rate)
{
    if (core_ == nullptr || core_->getAudioChannel == nullptr || core_->frequency == nullptr) {
        return;
    }

    audio_sample_rate_ = std::max(8000, sample_rate);
    if (core_->setAudioBufferSize != nullptr) {
        core_->setAudioBufferSize(core_, 4096);
    }
    if (blip_t* left = core_->getAudioChannel(core_, 0)) {
        blip_set_rates(left, core_->frequency(core_), audio_sample_rate_);
    }
    if (blip_t* right = core_->getAudioChannel(core_, 1)) {
        blip_set_rates(right, core_->frequency(core_), audio_sample_rate_);
    }
    pending_audio_.sample_rate = audio_sample_rate_;
}

void MgbaCore::append_audio_samples(int max_frames)
{
    if (core_ == nullptr || max_frames <= 0 || core_->getAudioChannel == nullptr) {
        return;
    }

    blip_t* left = core_->getAudioChannel(core_, 0);
    blip_t* right = core_->getAudioChannel(core_, 1);
    if (left == nullptr || right == nullptr) {
        return;
    }

    const int available = std::max(0, std::min({
        max_frames,
        blip_samples_avail(left),
        blip_samples_avail(right)
    }));
    if (available <= 0) {
        return;
    }

    const auto old_size = pending_audio_.interleaved_s16.size();
    pending_audio_.interleaved_s16.resize(
        old_size + static_cast<std::size_t>(available) * audio::AudioSampleBatch::Channels);
    blip_read_samples(left, pending_audio_.interleaved_s16.data() + old_size, available, 1);
    blip_read_samples(right, pending_audio_.interleaved_s16.data() + old_size + 1, available, 1);
}

std::uint32_t MgbaCore::input_to_mgba_keys(const GbaInputState& input)
{
    std::uint32_t keys = 0;
    if (input.a) keys |= 1u << GBA_KEY_A;
    if (input.b) keys |= 1u << GBA_KEY_B;
    if (input.select) keys |= 1u << GBA_KEY_SELECT;
    if (input.start) keys |= 1u << GBA_KEY_START;
    if (input.right) keys |= 1u << GBA_KEY_RIGHT;
    if (input.left) keys |= 1u << GBA_KEY_LEFT;
    if (input.up) keys |= 1u << GBA_KEY_UP;
    if (input.down) keys |= 1u << GBA_KEY_DOWN;
    if (input.r) keys |= 1u << GBA_KEY_R;
    if (input.l) keys |= 1u << GBA_KEY_L;
    return keys;
}

} // namespace czgba::core
