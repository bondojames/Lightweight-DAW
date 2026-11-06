// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace ldaw::engine {

// Min, max and RMS for each span of frames, for one channel, as 16-bit values.
struct PeakBin {
    std::int16_t min = 0;
    std::int16_t max = 0;
    std::uint16_t rms = 0;
};

// Waveform overview of a media file. Immutable once built, so any thread may read it.
struct WaveformPeaks {
    static constexpr std::uint32_t kFramesPerBin = 256;
    static constexpr std::uint32_t kLevelFactor = 8;
    std::uint32_t channels = 0;
    std::int64_t frames = 0;
    // levels[0] holds kFramesPerBin frames per bin, and each later level is kLevelFactor
    // coarser. Bins for a level run channel by channel: bins[ch][index].
    struct Level {
        std::uint32_t framesPerBin = 0;
        std::vector<std::vector<PeakBin>> bins;
    };
    std::vector<Level> levels;

    // The coarsest level that still gives at least one bin per wanted span.
    const Level* levelFor(double framesPerPixel) const;
};

// The decoded audio, at one sample rate, in the cache folder. The source is never touched.
struct PreparedMedia {
    std::filesystem::path sourcePath;
    std::filesystem::path cachePath; // Raw interleaved float32.
    double sampleRate = 0.0;
    double sourceSampleRate = 0.0;
    std::uint32_t channels = 0; // 1 or 2.
    std::uint32_t sourceChannels = 0;
    std::int64_t frames = 0;
    std::string decoderName;
    std::shared_ptr<const WaveformPeaks> peaks;
};

struct ImportProgress {
    std::atomic<float> fraction{0.0f};
    std::atomic<bool> cancel{false};
};

// Decodes any supported file, resamples it to targetRate if needed, writes the cache file
// and builds the waveform. Runs on a worker thread. A matching cache file is reused.
bool prepareMedia(const std::filesystem::path& source, double targetRate, const std::filesystem::path& cacheDirectory, PreparedMedia& out, std::string& errorOut, ImportProgress* progress = nullptr);

} // namespace ldaw::engine
