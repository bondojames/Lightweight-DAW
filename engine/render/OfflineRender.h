// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ldaw::engine {

class AudioEngine;

struct StereoBuffer {
    std::vector<float> left;
    std::vector<float> right;
};

// Runs the engine with no device, as fast as the CPU allows. Block sizes vary on
// purpose unless blockFrames is given, because a real driver may vary them too.
StereoBuffer renderOffline(AudioEngine& engine, std::uint64_t frames, double sampleRate, std::uint32_t blockFrames = 0);

// 32-bit float stereo WAV, written directly. libsndfile arrives with file import in Phase 02.
bool writeWavFloat32(const std::filesystem::path& path, const StereoBuffer& buffer, std::uint32_t sampleRate);
bool readWavFloat32(const std::filesystem::path& path, StereoBuffer& buffer, std::uint32_t& sampleRate);

// Finds the start of each click in a recording: a sample above the threshold that
// follows at least minSilenceFrames below it.
std::vector<std::int64_t> findClickOnsets(const std::vector<float>& mono, float threshold, std::int64_t minSilenceFrames);

struct ClickReport {
    std::size_t clicks = 0;
    double expectedSpacingFrames = 0.0;
    double meanSpacingFrames = 0.0;
    double maxErrorFrames = 0.0; // Largest distance of any click from its ideal place.
    bool passed = false;
    std::string summary;
};

// Checks click timing against the tempo, relative to the first click found.
ClickReport analyzeClickSpacing(const std::vector<std::int64_t>& onsets, double sampleRate, double bpm, double toleranceFrames);

// The self-contained offline test offered when no audio device is available: renders
// the metronome, writes a WAV if a path is given, and reads the timing back from the audio.
ClickReport runOfflineMetronomeTest(double sampleRate, double bpm, double seconds, const std::filesystem::path& wavPath);

} // namespace ldaw::engine
