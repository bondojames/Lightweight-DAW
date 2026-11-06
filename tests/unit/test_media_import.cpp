// SPDX-License-Identifier: GPL-3.0-or-later
#include "engine/media/AudioDecoder.h"
#include "engine/media/MediaImport.h"
#include "tests/unit/test_framework.h"

#define ENABLE_SNDFILE_WINDOWS_PROTOTYPES 1
#include <windows.h>
#include <sndfile.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <vector>

using namespace ldaw::engine;
namespace fs = std::filesystem;

namespace {

fs::path testDir() {
    const fs::path dir = fs::temp_directory_path() / L"ldaw_media_tést";
    fs::create_directories(dir);
    return dir;
}

std::vector<float> sine(std::uint32_t frames, std::uint32_t channels, double rate, double hz, float level = 0.5f) {
    std::vector<float> out(static_cast<std::size_t>(frames) * channels);
    for (std::uint32_t i = 0; i < frames; ++i) {
        const float s = level * static_cast<float>(std::sin(2.0 * std::numbers::pi * hz * i / rate));
        for (std::uint32_t ch = 0; ch < channels; ++ch) out[static_cast<std::size_t>(i) * channels + ch] = (ch == 1) ? -s : s;
    }
    return out;
}

bool writeSndfile(const fs::path& path, const std::vector<float>& data, std::uint32_t channels, int rate, int format) {
    SF_INFO info{};
    info.samplerate = rate;
    info.channels = static_cast<int>(channels);
    info.format = format;
    SNDFILE* file = sf_wchar_open(path.c_str(), SFM_WRITE, &info);
    if (!file) return false;
    const sf_count_t frames = static_cast<sf_count_t>(data.size() / channels);
    const sf_count_t wrote = sf_writef_float(file, data.data(), frames);
    sf_close(file);
    return wrote == frames;
}

std::vector<float> readCache(const PreparedMedia& media) {
    std::ifstream in(media.cachePath, std::ios::binary);
    std::vector<float> out(static_cast<std::size_t>(media.frames) * media.channels);
    in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size() * sizeof(float)));
    return out;
}

int risingCrossings(const std::vector<float>& data, std::uint32_t channels) {
    int count = 0;
    for (std::size_t i = channels; i < data.size(); i += channels) {
        if (data[i - channels] < 0.0f && data[i] >= 0.0f) ++count;
    }
    return count;
}

} // namespace

TEST_CASE(import_stereo_wav_at_native_rate_is_sample_exact) {
    const auto path = testDir() / L"stéreo.wav";
    const auto samples = sine(48000, 2, 48000.0, 440.0);
    CHECK(writeSndfile(path, samples, 2, 48000, SF_FORMAT_WAV | SF_FORMAT_FLOAT));
    PreparedMedia media;
    std::string error;
    CHECK(prepareMedia(path, 48000.0, testDir() / L"cache", media, error));
    CHECK(media.channels == 2);
    CHECK(media.frames == 48000);
    CHECK(media.sampleRate == 48000.0);
    CHECK(media.decoderName == "libsndfile");
    CHECK(readCache(media) == samples);
}

TEST_CASE(import_mono_stays_mono) {
    const auto path = testDir() / L"mono.wav";
    CHECK(writeSndfile(path, sine(4800, 1, 48000.0, 440.0), 1, 48000, SF_FORMAT_WAV | SF_FORMAT_PCM_16));
    PreparedMedia media;
    std::string error;
    CHECK(prepareMedia(path, 48000.0, testDir() / L"cache", media, error));
    CHECK(media.channels == 1);
    CHECK(media.sourceChannels == 1);
}

TEST_CASE(import_aiff_works) {
    const auto path = testDir() / L"tone.aiff";
    CHECK(writeSndfile(path, sine(4800, 2, 44100.0, 440.0), 2, 44100, SF_FORMAT_AIFF | SF_FORMAT_PCM_24));
    PreparedMedia media;
    std::string error;
    CHECK(prepareMedia(path, 44100.0, testDir() / L"cache", media, error));
    CHECK(media.frames == 4800);
    CHECK(media.channels == 2);
}

TEST_CASE(import_resamples_with_correct_length_and_pitch) {
    const auto path = testDir() / L"tone44.wav";
    CHECK(writeSndfile(path, sine(44100, 2, 44100.0, 1000.0), 2, 44100, SF_FORMAT_WAV | SF_FORMAT_FLOAT));
    for (const double target : {48000.0, 96000.0, 22050.0}) {
        PreparedMedia media;
        std::string error;
        CHECK(prepareMedia(path, target, testDir() / L"cache", media, error));
        CHECK(std::abs(media.frames - static_cast<std::int64_t>(target)) <= 4); // One second of audio.
        CHECK(media.sourceSampleRate == 44100.0);
        const auto data = readCache(media);
        const int crossings = risingCrossings(data, 2);
        CHECK(crossings >= 998 && crossings <= 1002); // Still 1 kHz after resampling.
        const float peak = *std::max_element(data.begin(), data.end());
        CHECK(peak > 0.45f && peak < 0.55f);
    }
}

TEST_CASE(import_multichannel_keeps_first_two) {
    const auto path = testDir() / L"six.wav";
    std::vector<float> data(600 * 6, 0.0f);
    for (std::size_t i = 0; i < 600; ++i) {
        data[i * 6] = 0.25f;
        data[i * 6 + 1] = -0.25f;
        data[i * 6 + 5] = 0.9f;
    }
    CHECK(writeSndfile(path, data, 6, 48000, SF_FORMAT_WAV | SF_FORMAT_FLOAT));
    PreparedMedia media;
    std::string error;
    CHECK(prepareMedia(path, 48000.0, testDir() / L"cache", media, error));
    CHECK(media.channels == 2);
    CHECK(media.sourceChannels == 6);
    const auto cached = readCache(media);
    CHECK(cached[0] == 0.25f && cached[1] == -0.25f);
}

TEST_CASE(source_file_is_never_modified) {
    const auto path = testDir() / L"untouched.wav";
    CHECK(writeSndfile(path, sine(4800, 2, 44100.0, 440.0), 2, 44100, SF_FORMAT_WAV | SF_FORMAT_PCM_16));
    const auto before = fs::last_write_time(path);
    const auto size = fs::file_size(path);
    PreparedMedia media;
    std::string error;
    CHECK(prepareMedia(path, 48000.0, testDir() / L"cache", media, error));
    CHECK(fs::last_write_time(path) == before);
    CHECK(fs::file_size(path) == size);
    CHECK(media.cachePath.parent_path() != path.parent_path());
}

TEST_CASE(bad_files_fail_with_a_message_and_leave_nothing_behind) {
    const auto dir = testDir();
    const auto cache = dir / L"cache_bad";
    fs::remove_all(cache);
    const auto garbage = dir / L"garbage.wav";
    { std::ofstream(garbage, std::ios::binary) << "this is not audio at all, but it is long enough to look like a header......"; }
    const auto emptyFile = dir / L"empty.wav";
    { std::ofstream(emptyFile, std::ios::binary).flush(); }
    const auto unknown = dir / L"tone.xyz";
    { std::ofstream(unknown, std::ios::binary) << "data"; }

    for (const fs::path& bad : {garbage, emptyFile, unknown, dir / L"does_not_exist.wav"}) {
        PreparedMedia media;
        std::string error;
        CHECK(!prepareMedia(bad, 48000.0, cache, media, error));
        CHECK(!error.empty());
    }
    int leftovers = 0;
    for (const auto& entry : fs::directory_iterator(cache)) {
        (void)entry;
        ++leftovers;
    }
    CHECK(leftovers == 0);
}

TEST_CASE(truncated_wav_does_not_crash_at_any_length) {
    const auto dir = testDir();
    const auto good = dir / L"whole.wav";
    CHECK(writeSndfile(good, sine(2000, 2, 48000.0, 440.0), 2, 48000, SF_FORMAT_WAV | SF_FORMAT_PCM_16));
    std::ifstream in(good, std::ios::binary);
    const std::string whole((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const auto cut = dir / L"cut.wav";
    for (std::size_t length = 0; length < 200 && length < whole.size(); length += 3) {
        { std::ofstream(cut, std::ios::binary | std::ios::trunc).write(whole.data(), static_cast<std::streamsize>(length)); }
        PreparedMedia media;
        std::string error;
        (void)prepareMedia(cut, 48000.0, dir / L"cache_cut", media, error);
    }
    fs::remove_all(dir / L"cache_cut");
    CHECK(true);
}

TEST_CASE(cancelling_an_import_stops_it_cleanly) {
    const auto path = testDir() / L"cancel.wav";
    CHECK(writeSndfile(path, sine(96000, 2, 48000.0, 440.0), 2, 48000, SF_FORMAT_WAV | SF_FORMAT_FLOAT));
    ImportProgress progress;
    progress.cancel = true;
    PreparedMedia media;
    std::string error;
    CHECK(!prepareMedia(path, 48000.0, testDir() / L"cache", media, error, &progress));
}

TEST_CASE(peaks_cover_the_file_and_have_the_right_values) {
    const auto path = testDir() / L"peaks.wav";
    CHECK(writeSndfile(path, sine(480000, 2, 48000.0, 100.0, 0.5f), 2, 48000, SF_FORMAT_WAV | SF_FORMAT_FLOAT));
    PreparedMedia media;
    std::string error;
    CHECK(prepareMedia(path, 48000.0, testDir() / L"cache", media, error));
    const auto& peaks = *media.peaks;
    CHECK(peaks.channels == 2);
    CHECK(peaks.frames == 480000);
    CHECK(peaks.levels.size() >= 3);
    const auto& base = peaks.levels.front();
    CHECK(base.framesPerBin == WaveformPeaks::kFramesPerBin);
    CHECK(base.bins[0].size() == 480000 / WaveformPeaks::kFramesPerBin);
    const auto& coarse = peaks.levels.back();
    const auto& bin = coarse.bins[0][0];
    CHECK(std::abs(bin.max - 16384) < 200);
    CHECK(std::abs(bin.min + 16384) < 200);
    CHECK(std::abs(static_cast<int>(bin.rms) - 11585) < 400); // 0.5 / sqrt(2)
}

TEST_CASE(peak_level_choice_gets_coarser_as_you_zoom_out) {
    const auto path = testDir() / L"levels.wav";
    CHECK(writeSndfile(path, sine(480000, 1, 48000.0, 100.0), 1, 48000, SF_FORMAT_WAV | SF_FORMAT_FLOAT));
    PreparedMedia media;
    std::string error;
    CHECK(prepareMedia(path, 48000.0, testDir() / L"cache", media, error));
    const auto& peaks = *media.peaks;
    CHECK(peaks.levelFor(1.0)->framesPerBin == WaveformPeaks::kFramesPerBin);
    CHECK(peaks.levelFor(256.0 * 8.0)->framesPerBin == 256u * 8u);
    CHECK(peaks.levelFor(1.0e9)->framesPerBin == peaks.levels.back().framesPerBin);
}

// Encoded with Windows' own encoders, so these show up as a pass only where they exist.
TEST_CASE(import_flac_m4a_mp3_through_media_foundation) {
    if (!mediaFoundationAvailable()) {
        std::printf("  (skipped: this Windows edition has no Media Foundation)\n");
        return;
    }
    const auto samples = sine(48000, 2, 48000.0, 1000.0);
    int imported = 0;
    for (const wchar_t* name : {L"mf.flac", L"mf.m4a", L"mf.mp3"}) {
        const auto path = testDir() / name;
        fs::remove(path);
        if (!encodeWithMediaFoundation(path, samples.data(), 48000, 2, 48000)) {
            std::printf("  (skipped %ls: no encoder here)\n", name);
            continue;
        }
        PreparedMedia media;
        std::string error;
        const bool ok = prepareMedia(path, 48000.0, testDir() / L"cache", media, error);
        if (!ok) std::printf("  %ls failed: %s\n", name, error.c_str());
        CHECK(ok);
        if (!ok) continue;
        ++imported;
        CHECK(media.decoderName == "Windows Media Foundation");
        CHECK(media.channels == 2);
        CHECK(std::abs(media.frames - 48000) < 4800); // Lossy codecs add a little padding.
        const int crossings = risingCrossings(readCache(media), 2);
        CHECK(crossings > 900 && crossings < 1100);
    }
    std::printf("  (%d of 3 Media Foundation formats imported)\n", imported);
}
