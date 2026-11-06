// SPDX-License-Identifier: GPL-3.0-or-later
#include "engine/AudioEngine.h"
#include "engine/render/OfflineRender.h"
#include "engine/runtime/RtGuard.h"
#include "engine/runtime/SpscQueue.h"
#include "engine/telemetry/AudioTelemetry.h"
#include "tests/unit/test_framework.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <new>
#include <thread>

using namespace ldaw::engine;

// --- Proves "no heap allocation in the callback": every new and delete in this test
// program is counted when it happens inside a RealtimeSection.
static std::atomic<int> g_realtimeAllocations{0};

void* operator new(std::size_t size) {
    if (g_inRealtimeSection) g_realtimeAllocations.fetch_add(1);
    if (void* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept {
    if (memory && g_inRealtimeSection) g_realtimeAllocations.fetch_add(1);
    std::free(memory);
}
void operator delete(void* memory, std::size_t) noexcept { operator delete(memory); }

namespace {

constexpr double kRate = 48000.0;

void startMetronome(AudioEngine& engine, double rate = kRate, double bpm = 120.0) {
    engine.prepare(rate, 4096);
    engine.post({EngineCommandType::SetTempo, bpm});
    engine.post({EngineCommandType::SetMetronome, 1.0});
    engine.post({EngineCommandType::Play, 0.0});
}

bool allFinite(const std::vector<float>& samples) {
    return std::all_of(samples.begin(), samples.end(), [](float s) { return std::isfinite(s); });
}

float largestJump(const std::vector<float>& samples) {
    float jump = 0.0f;
    for (std::size_t i = 1; i < samples.size(); ++i) jump = std::max(jump, std::abs(samples[i] - samples[i - 1]));
    return jump;
}

} // namespace

TEST_CASE(tempo_beats_have_no_drift_after_two_hours_at_96k) {
    TempoMap tempo;
    tempo.setSampleRate(96000.0);
    tempo.setTempo(0, 120.0);
    const std::int64_t twoHoursOfBeats = 2 * 60 * 60 * 2;
    CHECK(tempo.frameOfBeat(twoHoursOfBeats) == std::int64_t{2} * 60 * 60 * 96000);
}

TEST_CASE(tempo_non_integer_spacing_stays_exact) {
    TempoMap tempo;
    tempo.setSampleRate(44100.0);
    tempo.setTempo(0, 133.0);
    const double framesPerBeat = 44100.0 * 60.0 / 133.0;
    for (std::int64_t beat : {std::int64_t{1}, std::int64_t{1000}, std::int64_t{1000000}}) {
        CHECK(std::abs(static_cast<double>(tempo.frameOfBeat(beat)) - static_cast<double>(beat) * framesPerBeat) <= 0.5);
    }
}

TEST_CASE(tempo_change_keeps_earlier_beats) {
    TempoMap tempo;
    tempo.setTempo(0, 120.0);
    const std::int64_t beat4 = tempo.frameOfBeat(4);
    tempo.setTempo(beat4, 60.0);
    CHECK(tempo.frameOfBeat(4) == beat4);
    CHECK(tempo.frameOfBeat(5) == beat4 + 48000);
}

TEST_CASE(tempo_is_clamped_to_a_safe_range) {
    TempoMap tempo;
    tempo.setTempo(0, 0.0);
    CHECK(tempo.bpm() == TempoMap::kMinBpm);
    tempo.setTempo(0, 1.0e9);
    CHECK(tempo.bpm() == TempoMap::kMaxBpm);
}

TEST_CASE(metronome_beat_spacing_is_exact_offline) {
    for (const double rate : {44100.0, 48000.0, 96000.0}) {
        const ClickReport report = runOfflineMetronomeTest(rate, 120.0, 60.0, {});
        CHECK(report.clicks >= 119);
        CHECK(report.passed);
        CHECK(std::abs(report.meanSpacingFrames - rate / 2.0) < 0.1);
    }
}

TEST_CASE(metronome_click_lands_on_its_exact_frame) {
    AudioEngine engine;
    startMetronome(engine);
    const StereoBuffer audio = renderOffline(engine, 200000, kRate);
    // Click samples are zero at their first frame, and non-zero on the very next one.
    for (const std::size_t beatFrame : {std::size_t{0}, std::size_t{24000}, std::size_t{48000}, std::size_t{72000}, std::size_t{96000}}) {
        CHECK(audio.left[beatFrame + 1] != 0.0f);
        if (beatFrame > 0) CHECK(audio.left[beatFrame - 1] == 0.0f);
    }
}

TEST_CASE(metronome_accents_the_first_beat_of_each_bar) {
    AudioEngine engine;
    startMetronome(engine);
    const StereoBuffer audio = renderOffline(engine, 240000, kRate);
    const auto peakAt = [&](std::size_t start) {
        return *std::max_element(audio.left.begin() + static_cast<std::ptrdiff_t>(start),
                                 audio.left.begin() + static_cast<std::ptrdiff_t>(start + 1000));
    };
    const float accent = peakAt(0);
    CHECK(accent > peakAt(24000) * 1.5f);
    CHECK(peakAt(96000) == accent); // Beat 5 starts the second bar.
}

TEST_CASE(engine_output_does_not_depend_on_block_size) {
    AudioEngine a, b, c;
    startMetronome(a);
    startMetronome(b);
    startMetronome(c);
    const StereoBuffer varied = renderOffline(a, 300000, kRate);
    const StereoBuffer fixed = renderOffline(b, 300000, kRate, 256);
    const StereoBuffer whole = renderOffline(c, 300000, kRate, 300000);
    CHECK(varied.left == fixed.left);
    CHECK(varied.left == whole.left);
    CHECK(varied.right == whole.right);
}

TEST_CASE(engine_makes_no_heap_allocations_while_processing) {
    AudioEngine engine;
    startMetronome(engine);
    engine.post({EngineCommandType::SetTestTone, 1.0});
    const int before = g_realtimeAllocations.load();
    StereoBuffer audio = renderOffline(engine, 200000, kRate);
    engine.post({EngineCommandType::SetTempo, 90.0});
    engine.post({EngineCommandType::Rewind, 0.0});
    engine.post({EngineCommandType::Stop, 0.0});
    audio = renderOffline(engine, 48000, kRate);
    CHECK(g_realtimeAllocations.load() == before);
}

TEST_CASE(allocation_counter_really_detects_allocations) {
    const int before = g_realtimeAllocations.load();
    {
        const RealtimeSection realtime;
        auto* volatile allocation = new int(5);
        delete allocation;
    }
    CHECK(g_realtimeAllocations.load() == before + 2);
    g_realtimeAllocations.store(before);
}

TEST_CASE(stop_fades_without_a_jump_and_goes_silent) {
    AudioEngine engine;
    engine.prepare(kRate, 4096);
    engine.post({EngineCommandType::SetMetronome, 1.0});
    engine.post({EngineCommandType::Play, 0.0});
    (void)renderOffline(engine, 60, kRate); // Stop in the loudest part of the first click.
    engine.post({EngineCommandType::Stop, 0.0});
    const StereoBuffer after = renderOffline(engine, 4800, kRate, 64);
    CHECK(largestJump(after.left) < 0.5f);
    CHECK(!engine.state().playing);
    CHECK(after.left.back() == 0.0f);
}

TEST_CASE(position_only_advances_while_playing) {
    AudioEngine engine;
    engine.prepare(kRate, 4096);
    (void)renderOffline(engine, 1000, kRate);
    CHECK(engine.state().positionFrames == 0);
    engine.post({EngineCommandType::Play, 0.0});
    (void)renderOffline(engine, 1000, kRate);
    CHECK(engine.state().positionFrames == 1000);
    engine.post({EngineCommandType::Rewind, 0.0});
    (void)renderOffline(engine, 10, kRate);
    CHECK(engine.state().positionFrames == 10);
}

TEST_CASE(sample_rate_change_keeps_musical_position_and_stops) {
    AudioEngine engine;
    engine.prepare(48000.0, 4096);
    engine.post({EngineCommandType::Play, 0.0});
    (void)renderOffline(engine, 48000, 48000.0);
    engine.prepare(96000.0, 4096);
    CHECK(engine.state().positionFrames == 96000);
    CHECK(!engine.state().playing);
    engine.post({EngineCommandType::SetMetronome, 1.0});
    engine.post({EngineCommandType::Play, 0.0});
    (void)renderOffline(engine, 96000, 96000.0); // Next beats must still fall on the grid.
    CHECK(engine.state().positionFrames == 192000);
}

TEST_CASE(test_tone_has_the_right_pitch_and_fades_cleanly) {
    AudioEngine engine;
    engine.prepare(kRate, 4096);
    engine.post({EngineCommandType::SetTestTone, 1.0});
    const StereoBuffer on = renderOffline(engine, 48000, kRate);
    int rising = 0;
    for (std::size_t i = 1; i < on.left.size(); ++i) {
        if (on.left[i - 1] < 0.0f && on.left[i] >= 0.0f) ++rising;
    }
    CHECK(rising >= 439 && rising <= 441);
    CHECK(largestJump(on.left) < 0.02f);
    CHECK(*std::max_element(on.left.begin(), on.left.end()) <= TestTone::kLevel);

    engine.post({EngineCommandType::SetTestTone, 0.0});
    const StereoBuffer off = renderOffline(engine, 4800, kRate);
    CHECK(largestJump(off.left) < 0.02f);
    CHECK(off.left.back() == 0.0f);
}

TEST_CASE(output_stays_finite_and_in_range_at_extreme_tempos) {
    for (const double bpm : {TempoMap::kMinBpm, TempoMap::kMaxBpm, -5.0, 1.0e12}) {
        AudioEngine engine;
        startMetronome(engine, kRate, bpm);
        engine.post({EngineCommandType::SetTestTone, 1.0});
        const StereoBuffer audio = renderOffline(engine, 96000, kRate);
        CHECK(allFinite(audio.left));
        CHECK(*std::max_element(audio.left.begin(), audio.left.end()) <= 1.0f);
    }
}

TEST_CASE(output_gain_zero_is_silent_but_time_still_runs) {
    AudioEngine engine;
    startMetronome(engine);
    engine.post({EngineCommandType::SetOutputGain, 0.0});
    const StereoBuffer audio = renderOffline(engine, 48000, kRate);
    CHECK(*std::max_element(audio.left.begin(), audio.left.end()) == 0.0f);
    CHECK(engine.state().positionFrames == 48000);
}

TEST_CASE(output_peak_is_reported_then_cleared) {
    AudioEngine engine;
    startMetronome(engine);
    (void)renderOffline(engine, 4800, kRate);
    CHECK(engine.takeOutputPeak() > 0.1f);
    CHECK(engine.takeOutputPeak() == 0.0f);
}

TEST_CASE(full_command_queue_reports_failure_instead_of_blocking) {
    AudioEngine engine;
    bool sawFull = false;
    for (int i = 0; i < 1000 && !sawFull; ++i) sawFull = !engine.post({EngineCommandType::SetTempo, 120.0});
    CHECK(sawFull);
}

TEST_CASE(spsc_queue_preserves_order_across_threads) {
    static SpscQueue<std::uint32_t, 64> queue;
    constexpr std::uint32_t kCount = 200000;
    std::thread producer([] {
        for (std::uint32_t i = 0; i < kCount;) {
            if (queue.tryPush(i)) ++i;
        }
    });
    bool inOrder = true;
    for (std::uint32_t expected = 0; expected < kCount;) {
        std::uint32_t value = 0;
        if (queue.tryPop(value)) {
            inOrder = inOrder && value == expected;
            ++expected;
        }
    }
    producer.join();
    CHECK(inOrder);
}

TEST_CASE(wav_round_trip_including_non_ascii_path) {
    StereoBuffer buffer;
    buffer.left = {0.0f, 0.5f, -0.25f};
    buffer.right = {1.0f, -1.0f, 0.125f};
    const auto path = std::filesystem::temp_directory_path() / L"ldaw_tést.wav";
    CHECK(writeWavFloat32(path, buffer, 44100));
    StereoBuffer back;
    std::uint32_t rate = 0;
    CHECK(readWavFloat32(path, back, rate));
    CHECK(rate == 44100);
    CHECK(back.left == buffer.left);
    CHECK(back.right == buffer.right);
    std::filesystem::remove(path);
}

TEST_CASE(wav_reader_rejects_bad_files) {
    const auto path = std::filesystem::temp_directory_path() / L"ldaw_bad.wav";
    StereoBuffer buffer;
    buffer.left = {0.5f};
    buffer.right = {0.5f};
    CHECK(writeWavFloat32(path, buffer, 48000));
    { // Claim far more data than the file holds.
        std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
        const std::uint32_t huge = 0xFFFFFFF8u;
        file.seekp(40);
        file.write(reinterpret_cast<const char*>(&huge), 4);
    }
    StereoBuffer back;
    std::uint32_t rate = 0;
    CHECK(!readWavFloat32(path, back, rate));
    std::filesystem::remove(path);
    CHECK(!readWavFloat32(path, back, rate));
}

TEST_CASE(click_analysis_catches_a_late_click) {
    const std::vector<std::int64_t> good{0, 24000, 48000, 72000};
    CHECK(analyzeClickSpacing(good, 48000.0, 120.0, 1.0).passed);
    const std::vector<std::int64_t> late{0, 24000, 48050, 72000};
    const ClickReport report = analyzeClickSpacing(late, 48000.0, 120.0, 1.0);
    CHECK(!report.passed);
    CHECK(report.maxErrorFrames == 50.0);
    CHECK(!analyzeClickSpacing({5}, 48000.0, 120.0, 1.0).passed);
}

TEST_CASE(onset_finder_ignores_the_body_of_a_click) {
    std::vector<float> mono(1000, 0.0f);
    for (int i = 100; i < 150; ++i) mono[static_cast<std::size_t>(i)] = (i % 2) ? 0.5f : 0.0f;
    mono[800] = 0.5f;
    const auto onsets = findClickOnsets(mono, 0.1f, 100);
    CHECK(onsets.size() == 2);
    CHECK(onsets[0] == 101);
    CHECK(onsets[1] == 800);
}

TEST_CASE(telemetry_percentiles_and_peak) {
    AudioTelemetry telemetry;
    for (int i = 0; i < 99; ++i) telemetry.recordCallback(0.001, 0.01, 480); // 10%
    telemetry.recordCallback(0.008, 0.01, 480); // 80%
    CHECK(telemetry.callbacks() == 100);
    CHECK(telemetry.frames() == 48000);
    CHECK(std::abs(telemetry.loadPercentile(0.50) - 10.5) <= 0.5);
    CHECK(std::abs(telemetry.loadPercentile(0.99) - 10.5) <= 0.5);
    CHECK(std::abs(telemetry.loadPercentile(1.00) - 80.5) <= 0.5);
    CHECK(std::abs(telemetry.peakLoad() - 80.0f) < 0.01f);
    telemetry.reset();
    CHECK(telemetry.callbacks() == 0);
    CHECK(telemetry.loadPercentile(0.5) == 0.0);
}

TEST_CASE(telemetry_survives_absurd_load_values) {
    AudioTelemetry telemetry;
    telemetry.recordCallback(100.0, 0.001, 1);
    telemetry.recordCallback(0.0, 0.0, 1);
    CHECK(telemetry.callbacks() == 2);
    CHECK(telemetry.loadPercentile(1.0) == 200.5);
}

TEST_CASE(xruns_are_counted_by_cause_and_queued) {
    AudioTelemetry telemetry;
    telemetry.recordXrun(XrunCause::Engine, 1.5, 120.0f);
    telemetry.recordXrun(XrunCause::Device, 2.5, 5.0f);
    telemetry.recordXrun(XrunCause::Device, 3.5, 5.0f);
    CHECK(telemetry.xruns(XrunCause::Engine) == 1);
    CHECK(telemetry.xruns(XrunCause::Device) == 2);
    CHECK(telemetry.totalXruns() == 3);
    XrunEvent event;
    CHECK(telemetry.popXrun(event));
    CHECK(event.cause == XrunCause::Engine);
    CHECK(event.streamSeconds == 1.5);
}

TEST_CASE(xrun_detector_separates_our_overrun_from_a_late_device) {
    XrunDetector detector;
    constexpr double period = 0.01;
    // Normal callback: quick work, on-time wake.
    CHECK(!detector.checkCompute(0.001, period));
    CHECK(!detector.checkWake(period, period, 0));
    // A late wake with an empty buffer is not ours.
    CHECK(detector.checkWake(0.03, period, 0));
    // A late wake with audio still queued did not glitch.
    CHECK(!detector.checkWake(0.03, period, 240));
    // Our overrun counts once, not again at the late wake it causes.
    CHECK(detector.checkCompute(0.02, period));
    CHECK(!detector.checkWake(0.02, period, 0));
    // After that, late wakes count again.
    CHECK(detector.checkWake(0.03, period, 0));
}
