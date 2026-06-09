#include <filesystem>
#include <iostream>
#include <vector>

#include "core/mgba_core.hpp"
#include "storage/rom_scanner.hpp"

#ifndef CZ_GBA_SOURCE_ROOT
#define CZ_GBA_SOURCE_ROOT "."
#endif

namespace {

std::vector<std::filesystem::path> roms()
{
    std::vector<std::filesystem::path> result;
    const std::filesystem::path rom_dir = std::filesystem::path(CZ_GBA_SOURCE_ROOT) / "rom";
    if (!std::filesystem::exists(rom_dir)) {
        return result;
    }

    for (const auto& entry : std::filesystem::directory_iterator(rom_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".gba") {
            result.push_back(entry.path());
        }
    }
    return result;
}

} // namespace

int main()
{
    const auto rom_paths = roms();
    if (rom_paths.empty()) {
        std::cout << "audio core smoke skipped: no .gba file in rom/\n";
        return 0;
    }

    constexpr int kSampleRate = 48000;
    constexpr int kMaxFramesPerRead = 2048;
    constexpr int kWarmupFrames = 60;
    constexpr int kMeasuredFrames = 600;
    constexpr double kGbaFramesPerSecond = 59.727500569606;
    const double expected_frames_per_video_frame = kSampleRate / kGbaFramesPerSecond;

    bool any_rom_has_audio = false;
    int roms_checked = 0;

    for (const auto& rom_path : rom_paths) {
        czgba::core::MgbaCore core;
        if (!core.load_rom(czgba::storage::path_to_utf8(rom_path))) {
            std::cerr << "failed to load ROM for audio smoke: " << rom_path << '\n';
            return 1;
        }

        int total_audio_frames = 0;
        int non_empty_reads = 0;

        for (int i = 0; i < kWarmupFrames + kMeasuredFrames; ++i) {
            core.run_until_next_frame();
            const auto batch = core.read_audio_samples(kSampleRate, kMaxFramesPerRead);
            if (i >= kWarmupFrames) {
                total_audio_frames += batch.frame_count();
                if (batch.frame_count() > 0) {
                    ++non_empty_reads;
                }
            }
        }

        ++roms_checked;
        const double average_frames = static_cast<double>(total_audio_frames) / kMeasuredFrames;
        std::cout << czgba::storage::path_to_utf8(rom_path.filename())
                  << ": average audio frames/video frame " << average_frames
                  << ", non-empty reads " << non_empty_reads << "/" << kMeasuredFrames
                  << ", expected about " << expected_frames_per_video_frame << '\n';

        if (non_empty_reads > kMeasuredFrames * 9 / 10 &&
            average_frames > expected_frames_per_video_frame * 0.95 &&
            average_frames < expected_frames_per_video_frame * 1.05) {
            any_rom_has_audio = true;
            break;
        }
    }

    if (!any_rom_has_audio) {
        std::cerr << "audio core smoke failed: checked " << roms_checked
                  << " ROM(s), but none produced stable audio near "
                  << expected_frames_per_video_frame << " frames/video frame\n";
        return 1;
    }

    return 0;
}
