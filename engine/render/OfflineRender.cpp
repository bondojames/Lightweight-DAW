// SPDX-License-Identifier: GPL-3.0-or-later
#include "engine/render/OfflineRender.h"

#include "engine/AudioEngine.h"
#include "engine/runtime/RtGuard.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <fstream>

namespace ldaw::engine {

StereoBuffer renderOffline(AudioEngine& engine, std::uint64_t frames, double sampleRate, std::uint32_t blockFrames) {
    StereoBuffer out;
    out.left.resize(frames);
    out.right.resize(frames);

    // A fixed repeating pattern: varied, but the same on every run.
    static constexpr std::uint32_t kPattern[] = {64, 128, 37, 256, 480, 1, 512, 441, 1024, 96};
    std::uint64_t done = 0;
    std::size_t patternIndex = 0;
    while (done < frames) {
        const std::uint32_t wanted = blockFrames ? blockFrames : kPattern[patternIndex++ % std::size(kPattern)];
        const auto block = static_cast<std::uint32_t>(std::min<std::uint64_t>(wanted, frames - done));
        float* channels[2] = {out.left.data() + done, out.right.data() + done};
        ProcessContext context;
        context.outputs = channels;
        context.outputChannels = 2;
        context.frames = block;
        context.sampleRate = sampleRate;
        {
            const RealtimeSection realtime;
            engine.process(context);
        }
        done += block;
    }
    return out;
}

namespace {

template <typename T>
void put(std::ofstream& out, T value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

} // namespace

bool writeWavFloat32(const std::filesystem::path& path, const StereoBuffer& buffer, std::uint32_t sampleRate) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    const auto frames = static_cast<std::uint64_t>(buffer.left.size());
    const std::uint64_t dataBytes = frames * 2 * sizeof(float);
    if (dataBytes > 0xFFFFFFFFull - 36) return false; // A plain WAV cannot hold more.

    out.write("RIFF", 4);
    put<std::uint32_t>(out, static_cast<std::uint32_t>(36 + dataBytes));
    out.write("WAVEfmt ", 8);
    put<std::uint32_t>(out, 16);
    put<std::uint16_t>(out, 3); // IEEE float
    put<std::uint16_t>(out, 2);
    put<std::uint32_t>(out, sampleRate);
    put<std::uint32_t>(out, sampleRate * 2 * sizeof(float));
    put<std::uint16_t>(out, 2 * sizeof(float));
    put<std::uint16_t>(out, 32);
    out.write("data", 4);
    put<std::uint32_t>(out, static_cast<std::uint32_t>(dataBytes));
    for (std::uint64_t i = 0; i < frames; ++i) {
        put<float>(out, buffer.left[i]);
        put<float>(out, buffer.right[i]);
    }
    return static_cast<bool>(out);
}

bool readWavFloat32(const std::filesystem::path& path, StereoBuffer& buffer, std::uint32_t& sampleRate) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    char header[44];
    if (!in.read(header, sizeof(header))) return false;
    std::uint16_t format = 0, channels = 0, bits = 0;
    std::uint32_t dataBytes = 0;
    std::memcpy(&format, header + 20, 2);
    std::memcpy(&channels, header + 22, 2);
    std::memcpy(&sampleRate, header + 24, 4);
    std::memcpy(&bits, header + 34, 2);
    std::memcpy(&dataBytes, header + 40, 4);
    if (std::memcmp(header, "RIFF", 4) != 0 || std::memcmp(header + 8, "WAVE", 4) != 0) return false;
    if (format != 3 || channels != 2 || bits != 32) return false;

    // Trust the file's actual size, not its header, before allocating.
    const auto start = in.tellg();
    in.seekg(0, std::ios::end);
    const auto available = static_cast<std::uint64_t>(in.tellg() - start);
    in.seekg(start);
    if (dataBytes > available || dataBytes % 8 != 0) return false;

    const std::size_t frames = dataBytes / 8;
    buffer.left.resize(frames);
    buffer.right.resize(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        in.read(reinterpret_cast<char*>(&buffer.left[i]), 4);
        in.read(reinterpret_cast<char*>(&buffer.right[i]), 4);
    }
    return static_cast<bool>(in);
}

std::vector<std::int64_t> findClickOnsets(const std::vector<float>& mono, float threshold, std::int64_t minSilenceFrames) {
    std::vector<std::int64_t> onsets;
    std::int64_t lastLoud = -minSilenceFrames - 1;
    for (std::int64_t i = 0; i < static_cast<std::int64_t>(mono.size()); ++i) {
        if (std::abs(mono[static_cast<std::size_t>(i)]) < threshold) continue;
        if (i - lastLoud > minSilenceFrames) onsets.push_back(i);
        lastLoud = i;
    }
    return onsets;
}

ClickReport analyzeClickSpacing(const std::vector<std::int64_t>& onsets, double sampleRate, double bpm, double toleranceFrames) {
    ClickReport report;
    report.clicks = onsets.size();
    report.expectedSpacingFrames = sampleRate * 60.0 / bpm;
    if (onsets.size() < 2) {
        report.summary = std::format("Found {} clicks, which is too few to measure.", onsets.size());
        return report;
    }
    report.meanSpacingFrames = static_cast<double>(onsets.back() - onsets.front()) / static_cast<double>(onsets.size() - 1);
    for (std::size_t i = 0; i < onsets.size(); ++i) {
        const double ideal = static_cast<double>(onsets.front()) + static_cast<double>(i) * report.expectedSpacingFrames;
        report.maxErrorFrames = std::max(report.maxErrorFrames, std::abs(static_cast<double>(onsets[i]) - ideal));
    }
    report.passed = report.maxErrorFrames <= toleranceFrames;
    report.summary = std::format("{} clicks, expected spacing {:.2f} frames, measured {:.2f}, largest error {:.2f} frames: {}",
                                 report.clicks, report.expectedSpacingFrames, report.meanSpacingFrames, report.maxErrorFrames,
                                 report.passed ? "PASS" : "FAIL");
    return report;
}

ClickReport runOfflineMetronomeTest(double sampleRate, double bpm, double seconds, const std::filesystem::path& wavPath) {
    AudioEngine engine;
    engine.prepare(sampleRate, 4096);
    engine.post({EngineCommandType::SetTempo, bpm});
    engine.post({EngineCommandType::SetMetronome, 1.0});
    engine.post({EngineCommandType::Play, 0.0});
    const StereoBuffer audio = renderOffline(engine, static_cast<std::uint64_t>(seconds * sampleRate), sampleRate);
    if (!wavPath.empty() && !writeWavFloat32(wavPath, audio, static_cast<std::uint32_t>(sampleRate))) {
        ClickReport failed;
        failed.summary = "Could not write the WAV file.";
        return failed;
    }
    const auto onsets = findClickOnsets(audio.left, 0.005f, static_cast<std::int64_t>(sampleRate * 0.05));
    // The sine starts at zero, so an onset is found a few samples into the click.
    return analyzeClickSpacing(onsets, sampleRate, bpm, 2.0);
}

} // namespace ldaw::engine
