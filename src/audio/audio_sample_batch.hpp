#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace czgba::audio {

struct AudioSampleBatch {
    static constexpr int Channels = 2;

    int sample_rate = 48000;
    std::vector<std::int16_t> interleaved_s16;

    std::span<const std::int16_t> samples() const
    {
        return interleaved_s16;
    }

    int frame_count() const
    {
        return static_cast<int>(interleaved_s16.size() / Channels);
    }
};

} // namespace czgba::audio
