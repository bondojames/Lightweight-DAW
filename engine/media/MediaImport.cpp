// SPDX-License-Identifier: GPL-3.0-or-later
#include "engine/media/MediaImport.h"

#include "engine/media/AudioDecoder.h"

#include <samplerate.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <format>
#include <fstream>
#include <functional>
#include <system_error>

namespace ldaw::engine {
namespace {

constexpr std::uint32_t kPeaksFileMagic = 0x4B504C44; // Reserved so a future peak cache can be recognised.

std::int16_t toPeakValue(float sample) {
    return static_cast<std::int16_t>(std::lround(std::clamp(sample, -1.0f, 1.0f) * 32767.0f));
}

// Builds level 0 incrementally as audio arrives.
class PeakBuilder {
public:
    explicit PeakBuilder(std::uint32_t channels) : channels_(channels), bins_(channels), state_(channels) {}

    void add(const float* interleaved, std::uint32_t frames) {
        for (std::uint32_t frame = 0; frame < frames; ++frame) {
            for (std::uint32_t ch = 0; ch < channels_; ++ch) {
                const float sample = interleaved[frame * channels_ + ch];
                State& s = state_[ch];
                s.min = std::min(s.min, sample);
                s.max = std::max(s.max, sample);
                s.squares += static_cast<double>(sample) * sample;
            }
            if (++count_ == WaveformPeaks::kFramesPerBin) flush();
        }
        frames_ += frames;
    }

    std::shared_ptr<const WaveformPeaks> finish() {
        if (count_ > 0) flush();
        auto peaks = std::make_shared<WaveformPeaks>();
        peaks->channels = channels_;
        peaks->frames = frames_;
        WaveformPeaks::Level base;
        base.framesPerBin = WaveformPeaks::kFramesPerBin;
        base.bins = std::move(bins_);
        peaks->levels.push_back(std::move(base));
        // Coarser levels keep drawing cost flat when zoomed far out.
        while (peaks->levels.back().bins[0].size() > WaveformPeaks::kLevelFactor * 4) {
            const WaveformPeaks::Level& fine = peaks->levels.back();
            WaveformPeaks::Level coarse;
            coarse.framesPerBin = fine.framesPerBin * WaveformPeaks::kLevelFactor;
            coarse.bins.resize(channels_);
            for (std::uint32_t ch = 0; ch < channels_; ++ch) {
                const auto& source = fine.bins[ch];
                for (std::size_t i = 0; i < source.size(); i += WaveformPeaks::kLevelFactor) {
                    const std::size_t end = std::min(i + WaveformPeaks::kLevelFactor, source.size());
                    PeakBin bin{32767, -32768, 0};
                    double squares = 0.0;
                    for (std::size_t k = i; k < end; ++k) {
                        bin.min = std::min(bin.min, source[k].min);
                        bin.max = std::max(bin.max, source[k].max);
                        squares += static_cast<double>(source[k].rms) * source[k].rms;
                    }
                    bin.rms = static_cast<std::uint16_t>(std::sqrt(squares / static_cast<double>(end - i)));
                    coarse.bins[ch].push_back(bin);
                }
            }
            peaks->levels.push_back(std::move(coarse));
        }
        return peaks;
    }

private:
    struct State {
        float min = 1.0f;
        float max = -1.0f;
        double squares = 0.0;
    };

    void flush() {
        for (std::uint32_t ch = 0; ch < channels_; ++ch) {
            State& s = state_[ch];
            PeakBin bin;
            bin.min = toPeakValue(s.min);
            bin.max = toPeakValue(s.max);
            bin.rms = static_cast<std::uint16_t>(std::lround(std::min(1.0, std::sqrt(s.squares / count_)) * 32767.0));
            bins_[ch].push_back(bin);
            s = State{};
        }
        count_ = 0;
    }

    std::uint32_t channels_;
    std::vector<std::vector<PeakBin>> bins_;
    std::vector<State> state_;
    std::uint32_t count_ = 0;
    std::int64_t frames_ = 0;
};

std::uint64_t fnv1a(const std::wstring& text, std::uint64_t seed) {
    std::uint64_t hash = seed;
    for (const wchar_t c : text) {
        hash ^= static_cast<std::uint64_t>(c);
        hash *= 1099511628211ull;
    }
    return hash;
}

// Changes when the file does, so an edited source can't reuse stale audio.
std::string cacheKey(const std::filesystem::path& source, double rate) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(source, ec);
    const auto time = std::filesystem::last_write_time(source, ec).time_since_epoch().count();
    std::uint64_t hash = fnv1a(std::filesystem::absolute(source, ec).wstring(), 14695981039346656037ull);
    hash ^= static_cast<std::uint64_t>(size) * 1099511628211ull;
    hash ^= static_cast<std::uint64_t>(time) * 0x9E3779B97F4A7C15ull;
    return std::format("{:016x}-{:.0f}", hash, rate);
}

} // namespace

const WaveformPeaks::Level* WaveformPeaks::levelFor(double framesPerPixel) const {
    const Level* best = levels.empty() ? nullptr : &levels.front();
    for (const Level& level : levels) {
        if (static_cast<double>(level.framesPerBin) <= framesPerPixel) best = &level;
    }
    return best;
}

bool prepareMedia(const std::filesystem::path& source, double targetRate, const std::filesystem::path& cacheDirectory, PreparedMedia& out, std::string& errorOut, ImportProgress* progress) {
    (void)kPeaksFileMagic;
    std::error_code ec;
    std::filesystem::create_directories(cacheDirectory, ec);
    const auto cachePath = cacheDirectory / (cacheKey(source, targetRate) + ".pcm");
    auto tempPath = cachePath;
    tempPath += L".tmp";

    std::ofstream cache(tempPath, std::ios::binary | std::ios::trunc);
    if (!cache) {
        errorOut = "Could not write to the audio cache folder.";
        return false;
    }

    DecodedFormat format;
    std::unique_ptr<PeakBuilder> peaks;
    SRC_STATE* resampler = nullptr;
    struct ResamplerCloser {
        SRC_STATE*& state;
        ~ResamplerCloser() {
            if (state) src_delete(state);
        }
    } closer{resampler};

    std::vector<float> resampled;
    std::uint32_t outChannels = 0;
    std::int64_t written = 0;
    std::int64_t decodedFrames = 0;
    bool resampleError = false;

    const auto write = [&](const float* data, std::uint32_t frames) {
        cache.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(frames) * outChannels * static_cast<std::streamsize>(sizeof(float)));
        peaks->add(data, frames);
        written += frames;
    };

    const auto feed = [&](const float* data, std::uint32_t frames, bool last) {
        const double ratio = targetRate / format.sampleRate;
        SRC_DATA job{};
        job.src_ratio = ratio;
        job.end_of_input = last ? 1 : 0;
        // libsamplerate only flushes its filter's tail if this pointer is non-null,
        // even when there are no frames left to read.
        static constexpr float kNoInput[2] = {0.0f, 0.0f};
        job.data_in = data ? data : kNoInput;
        job.input_frames = frames;
        // Room for this chunk at the new rate, plus what the filter still holds.
        resampled.resize((static_cast<std::size_t>(std::ceil(frames * ratio)) + 8192) * outChannels);
        for (;;) {
            job.data_out = resampled.data();
            job.output_frames = static_cast<long>(resampled.size() / outChannels);
            if (src_process(resampler, &job) != 0) {
                resampleError = true;
                return;
            }
            if (job.output_frames_gen > 0) write(resampled.data(), static_cast<std::uint32_t>(job.output_frames_gen));
            job.data_in += job.input_frames_used * outChannels;
            job.input_frames -= job.input_frames_used;
            if (job.input_frames <= 0 && (!last || job.output_frames_gen == 0)) break;
        }
    };

    const auto sink = [&](const float* interleaved, std::uint32_t frames, std::uint32_t channels) {
        if (!peaks) {
            outChannels = channels;
            peaks = std::make_unique<PeakBuilder>(outChannels);
            if (format.sampleRate != targetRate) {
                int status = 0;
                resampler = src_new(SRC_SINC_MEDIUM_QUALITY, static_cast<int>(outChannels), &status);
                if (!resampler) {
                    resampleError = true;
                    return false;
                }
            }
        }
        decodedFrames += frames;
        if (resampler) feed(interleaved, frames, false);
        else write(interleaved, frames);
        if (progress) {
            if (progress->cancel.load(std::memory_order_relaxed)) return false;
            if (format.frames > 0) progress->fraction.store(std::min(0.99f, static_cast<float>(decodedFrames) / static_cast<float>(format.frames)), std::memory_order_relaxed);
        }
        return !resampleError && static_cast<bool>(cache);
    };

    if (!decodeAudioFile(source, format, sink, errorOut)) {
        cache.close();
        std::filesystem::remove(tempPath, ec);
        return false;
    }
    if (!peaks || decodedFrames == 0) {
        cache.close();
        std::filesystem::remove(tempPath, ec);
        errorOut = "The file contains no audio.";
        return false;
    }
    if (resampler) feed(nullptr, 0, true);
    cache.close();
    if (resampleError || !cache) {
        std::filesystem::remove(tempPath, ec);
        errorOut = resampleError ? "Sample-rate conversion failed." : "The disk ran out of space while preparing the audio.";
        return false;
    }
    std::filesystem::rename(tempPath, cachePath, ec);
    if (ec) {
        std::filesystem::remove(tempPath, ec);
        errorOut = "Could not finish writing the audio cache file.";
        return false;
    }

    out.sourcePath = source;
    out.cachePath = cachePath;
    out.sampleRate = targetRate;
    out.sourceSampleRate = format.sampleRate;
    out.channels = outChannels;
    out.sourceChannels = format.channels;
    out.frames = written;
    out.decoderName = format.decoderName;
    out.peaks = peaks->finish();
    if (progress) progress->fraction.store(1.0f, std::memory_order_relaxed);
    return true;
}

} // namespace ldaw::engine
