// SPDX-License-Identifier: GPL-3.0-or-later
// Golden-audio tests: render real clips offline and measure the result.
#include "core/commands/Edits.h"
#include "engine/AudioEngine.h"
#include "engine/graph/ArrangementSnapshot.h"
#include "engine/media/MediaImport.h"
#include "engine/render/OfflineRender.h"
#include "tests/unit/test_framework.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <memory>
#include <numbers>
#include <vector>

using namespace ldaw::engine;
using namespace ldaw::commands;
using namespace ldaw::model;
namespace fs = std::filesystem;

namespace {

constexpr double kRate = 48000.0;
constexpr Time kSec = kTimePerSecond;

fs::path dir() {
    const fs::path path = fs::temp_directory_path() / L"ldaw_playback_test";
    fs::create_directories(path);
    return path;
}

// Writes a file as float WAV and prepares it, as a real import would.
std::shared_ptr<MediaRuntime> makeMedia(const wchar_t* name, const std::vector<float>& left, const std::vector<float>& right, double rate = kRate) {
    StereoBuffer buffer;
    buffer.left = left;
    buffer.right = right;
    const auto path = dir() / name;
    if (!writeWavFloat32(path, buffer, static_cast<std::uint32_t>(rate))) return nullptr;
    PreparedMedia prepared;
    std::string error;
    if (!prepareMedia(path, kRate, dir() / L"cache", prepared, error)) return nullptr;
    auto runtime = std::make_shared<MediaRuntime>(prepared);
    runtime->loadAll();
    return runtime;
}

std::vector<float> ramp(std::size_t frames) {
    std::vector<float> out(frames);
    for (std::size_t i = 0; i < frames; ++i) out[i] = static_cast<float>(i) / static_cast<float>(frames) * 0.9f - 0.45f;
    return out;
}

std::vector<float> sineWave(std::size_t frames, double hz, float level = 0.5f) {
    std::vector<float> out(frames);
    for (std::size_t i = 0; i < frames; ++i) out[i] = level * static_cast<float>(std::sin(2.0 * std::numbers::pi * hz * static_cast<double>(i) / kRate));
    return out;
}

struct Rig {
    Project project;
    Id track = 0;
    std::map<Id, std::shared_ptr<MediaRuntime>> runtimes;
    TrackSlots slots;
    std::shared_ptr<const ArrangementSnapshot> snapshot;
    AudioEngine engine;

    Rig() {
        engine.prepare(kRate, 4096);
        track = addTrack(project, "T");
    }
    Id media(const std::shared_ptr<MediaRuntime>& runtime, const std::wstring& name) {
        const Id id = addMedia(project, name, framesToTime(runtime->frames(), kRate), runtime->channels(), kRate);
        runtimes[id] = runtime;
        return id;
    }
    void publish() {
        auto next = compileSnapshot(project, kRate, [&](const MediaItem& item) { return runtimes.count(item.id) ? runtimes[item.id] : nullptr; }, slots);
        engine.exchangeSnapshot(next.get());
        snapshot = std::move(next); // Offline: nothing is mid-process, so the old one can go.
    }
    StereoBuffer render(std::uint64_t frames, std::uint32_t block = 0) {
        engine.post({EngineCommandType::Play, 0.0});
        return renderOffline(engine, frames, kRate, block);
    }
};

float largestJump(const std::vector<float>& samples, std::size_t from, std::size_t to) {
    float jump = 0.0f;
    for (std::size_t i = from + 1; i < to; ++i) jump = std::max(jump, std::abs(samples[i] - samples[i - 1]));
    return jump;
}

} // namespace

TEST_CASE(clip_plays_its_exact_samples_at_its_exact_position) {
    Rig rig;
    const auto source = sineWave(48000, 440.0);
    const Id media = rig.media(makeMedia(L"a.wav", source, source), L"a");
    addClip(rig.project, rig.track, media, kSec); // Starts 1 second in.
    rig.publish();
    const StereoBuffer out = rig.render(120000);
    CHECK(out.left[47999] == 0.0f);
    // The middle of the clip, away from its tiny edge fades, is sample-exact.
    bool exact = true;
    for (std::size_t i = 200; i < 47800; ++i) exact = exact && std::abs(out.left[48000 + i] - source[i]) < 1e-5f;
    CHECK(exact);
    CHECK(out.left[48000 + 48000 + 1] == 0.0f);
}

TEST_CASE(mono_media_plays_on_both_sides) {
    Rig rig;
    const auto left = sineWave(9600, 440.0);
    // A mono import, via the WAV reader's two-channel rule: identical sides.
    const Id media = rig.media(makeMedia(L"m.wav", left, left), L"m");
    addClip(rig.project, rig.track, media, 0);
    rig.publish();
    const StereoBuffer out = rig.render(9600);
    CHECK(out.left == out.right);
}

TEST_CASE(tracks_are_aligned_to_the_sample) {
    Rig rig;
    const auto source = sineWave(96000, 997.0, 0.25f);
    const auto runtime = makeMedia(L"align.wav", source, source);
    const Id media = rig.media(runtime, L"align");
    const Id second = addTrack(rig.project, "T2");
    addClip(rig.project, rig.track, media, 0);
    addClip(rig.project, second, media, 0);
    rig.publish();
    const StereoBuffer both = rig.render(96000);
    // Identical files on two tracks must sum to exactly double, with no phase offset.
    bool doubled = true;
    for (std::size_t i = 1000; i < 95000; ++i) doubled = doubled && std::abs(both.left[i] - 2.0f * source[i]) < 1e-4f;
    CHECK(doubled);
}

TEST_CASE(a_clip_nulls_against_its_inverted_self_on_another_track) {
    Rig rig;
    const auto source = sineWave(48000, 1000.0);
    std::vector<float> inverted = source;
    for (float& s : inverted) s = -s;
    const Id a = rig.media(makeMedia(L"p.wav", source, source), L"p");
    const Id b = rig.media(makeMedia(L"n.wav", inverted, inverted), L"n");
    const Id second = addTrack(rig.project, "T2");
    addClip(rig.project, rig.track, a, 0);
    addClip(rig.project, second, b, 0);
    rig.publish();
    const StereoBuffer out = rig.render(48000);
    float largest = 0.0f;
    for (std::size_t i = 1000; i < 47000; ++i) largest = std::max(largest, std::abs(out.left[i]));
    CHECK(largest < 1e-5f);
}

TEST_CASE(split_is_inaudible) {
    const auto source = sineWave(96000, 220.0);
    Rig whole;
    const Id wholeMedia = whole.media(makeMedia(L"s.wav", source, source), L"s");
    addClip(whole.project, whole.track, wholeMedia, 0);
    whole.publish();
    const StereoBuffer reference = whole.render(96000);

    Rig cut;
    const Id cutMedia = cut.media(makeMedia(L"s.wav", source, source), L"s");
    const Id clip = addClip(cut.project, cut.track, cutMedia, 0);
    const Time at = kSec;
    CHECK(splitClip(cut.project, clip, at) != kNoId);
    cut.publish();
    const StereoBuffer out = cut.render(96000);

    // Away from the join they match exactly, and across it there's no click.
    bool same = true;
    for (std::size_t i = 100; i < 47000; ++i) same = same && out.left[i] == reference.left[i];
    CHECK(same);
    const float normal = largestJump(reference.left, 47000, 49000);
    CHECK(largestJump(out.left, 47000, 49000) <= normal * 1.5f);
}

TEST_CASE(cutting_into_a_loud_waveform_does_not_click) {
    Rig rig;
    std::vector<float> dc(48000, 0.8f); // Worst case: an abrupt cut would be a full-scale step.
    const Id media = rig.media(makeMedia(L"dc.wav", dc, dc), L"dc");
    const Id clip = addClip(rig.project, rig.track, media, kSec / 10);
    CHECK(trimClipEnd(rig.project, clip, kSec / 2));
    rig.publish();
    const StereoBuffer out = rig.render(48000);
    CHECK(largestJump(out.left, 0, out.left.size()) < 0.02f);
}

TEST_CASE(trim_and_slip_play_the_right_part_of_the_source) {
    Rig rig;
    const auto source = ramp(96000);
    const Id media = rig.media(makeMedia(L"r.wav", source, source), L"r");
    const Id clip = addClip(rig.project, rig.track, media, 0);
    CHECK(trimClipStart(rig.project, clip, kSec / 2));
    rig.publish();
    const StereoBuffer out = rig.render(96000);
    CHECK(out.left[23000] == 0.0f);
    CHECK(std::abs(out.left[30000] - source[30000]) < 1e-5f);
    CHECK(std::abs(out.left[90000] - source[90000]) < 1e-5f);
}

TEST_CASE(playback_starts_from_any_position) {
    Rig rig;
    const auto source = ramp(96000);
    const Id media = rig.media(makeMedia(L"r.wav", source, source), L"r");
    addClip(rig.project, rig.track, media, 0);
    rig.publish();
    rig.engine.post({EngineCommandType::Seek, 50000.0});
    const StereoBuffer out = rig.render(3000);
    CHECK(rig.engine.state().positionFrames == 53000);
    // The very first frames are inside the 10 ms gain ramp-up; compare after it.
    for (const std::size_t i : {std::size_t{1000}, std::size_t{2000}, std::size_t{2999}}) {
        CHECK(std::abs(out.left[i] - source[50000 + i]) < 1e-5f);
    }
}

TEST_CASE(loop_wraps_sample_accurately) {
    Rig rig;
    const auto source = ramp(48000);
    const Id media = rig.media(makeMedia(L"l.wav", source, source), L"l");
    addClip(rig.project, rig.track, media, 0);
    CHECK(setLoop(rig.project, true, framesToTime(10000, kRate), framesToTime(20000, kRate)));
    rig.publish();
    rig.engine.post({EngineCommandType::Seek, 10000.0});
    StereoBuffer out = rig.render(35000, 256);
    CHECK(std::abs(rig.engine.state().positionFrames - (10000 + 35000 % 10000)) == 0);
    // After wrapping twice, sample 25000 of the output is source sample 15000 again.
    CHECK(std::abs(out.left[25000] - source[15000]) < 1e-5f);
    CHECK(std::abs(out.left[15000] - source[15000]) < 1e-5f);
}

TEST_CASE(loop_output_does_not_depend_on_block_size) {
    const auto source = ramp(48000);
    const auto runtime = makeMedia(L"l2.wav", source, source);
    std::vector<float> reference;
    for (const std::uint32_t block : {0u, 64u, 256u, 1024u, 4096u}) {
        Rig rig;
        const Id media = rig.media(runtime, L"l2");
        addClip(rig.project, rig.track, media, 0);
        setLoop(rig.project, true, framesToTime(1000, kRate), framesToTime(9000, kRate));
        rig.publish();
        const StereoBuffer out = rig.render(40000, block);
        if (reference.empty()) reference = out.left;
        else CHECK(out.left == reference);
    }
}

TEST_CASE(mute_and_solo_rules) {
    Rig rig;
    const auto source = sineWave(48000, 440.0, 0.25f);
    const Id media = rig.media(makeMedia(L"ms.wav", source, source), L"ms");
    const Id second = addTrack(rig.project, "T2");
    addClip(rig.project, rig.track, media, 0);
    addClip(rig.project, second, media, 0);

    setTrackMute(rig.project, rig.track, true);
    rig.publish();
    StereoBuffer out = rig.render(12000);
    CHECK(std::abs(out.left[8000] - source[8000]) < 1e-3f); // Only one track sounds.

    setTrackMute(rig.project, rig.track, false);
    setTrackSolo(rig.project, second, true);
    rig.publish();
    out = rig.render(12000);
    CHECK(std::abs(out.left[6000] - source[12000 + 6000]) < 1e-3f); // Soloed one only.
}

TEST_CASE(pan_is_constant_power_and_centre_is_unity) {
    Rig rig;
    const auto source = sineWave(48000, 440.0, 0.25f);
    const Id media = rig.media(makeMedia(L"pan.wav", source, source), L"pan");
    addClip(rig.project, rig.track, media, 0);
    rig.publish();
    StereoBuffer out = rig.render(12000);
    CHECK(std::abs(out.left[8000] - source[8000]) < 1e-4f);
    CHECK(std::abs(out.right[8000] - source[8000]) < 1e-4f);

    setTrackPan(rig.project, rig.track, -1.0f);
    rig.publish();
    out = rig.render(12000);
    float rightPeak = 0.0f, leftPeak = 0.0f;
    for (std::size_t i = 2000; i < 12000; ++i) {
        rightPeak = std::max(rightPeak, std::abs(out.right[i]));
        leftPeak = std::max(leftPeak, std::abs(out.left[i]));
    }
    CHECK(rightPeak < 1e-4f);
    CHECK(std::abs(leftPeak - 0.25f * std::numbers::sqrt2_v<float>) < 1e-2f); // +3 dB at the side.
}

TEST_CASE(gain_changes_are_smoothed_while_playing) {
    Rig rig;
    std::vector<float> dc(96000, 0.5f);
    const Id media = rig.media(makeMedia(L"g.wav", dc, dc), L"g");
    addClip(rig.project, rig.track, media, 0);
    rig.publish();
    (void)rig.render(4800);
    setTrackGain(rig.project, rig.track, 0.1f);
    rig.publish();
    const StereoBuffer out = rig.render(4800);
    CHECK(largestJump(out.left, 0, out.left.size()) < 0.0011f); // A 10 ms ramp on a 0.5 signal.
    CHECK(std::abs(out.left.back() - 0.05f) < 1e-3f);
}

TEST_CASE(a_missing_block_is_silent_and_counted_not_a_stall) {
    Rig rig;
    const auto source = sineWave(200000, 440.0);
    auto runtime = makeMedia(L"miss.wav", source, source);
    delete[] runtime->evict(1); // Offline, so nothing can still be reading it.
    const Id media = rig.media(runtime, L"miss");
    addClip(rig.project, rig.track, media, 0);
    rig.publish();
    const StereoBuffer out = rig.render(200000);
    CHECK(rig.engine.diskMissFrames() == static_cast<std::uint64_t>(MediaRuntime::kBlockFrames));
    CHECK(out.left[MediaRuntime::kBlockFrames + 100] == 0.0f);
    CHECK(out.left[2 * MediaRuntime::kBlockFrames + 500] != 0.0f);
}

TEST_CASE(clip_that_runs_past_a_block_boundary_is_seamless) {
    Rig rig;
    const auto source = ramp(static_cast<std::size_t>(MediaRuntime::kBlockFrames) * 2 + 100);
    const Id media = rig.media(makeMedia(L"b.wav", source, source), L"b");
    addClip(rig.project, rig.track, media, 0);
    rig.publish();
    const StereoBuffer out = rig.render(source.size());
    bool exact = true;
    for (std::size_t i = 1000; i < source.size() - 1000; ++i) exact = exact && std::abs(out.left[i] - source[i]) < 1e-5f;
    CHECK(exact);
    CHECK(rig.engine.diskMissFrames() == 0);
}

TEST_CASE(clips_with_no_ready_media_are_left_out) {
    Rig rig;
    const Id media = addMedia(rig.project, "notready.wav", kSec, 2, kRate);
    addClip(rig.project, rig.track, media, 0);
    rig.publish();
    CHECK(rig.snapshot->tracks.size() == 1);
    CHECK(rig.snapshot->tracks[0].clips.empty());
    const StereoBuffer out = rig.render(1000);
    CHECK(out.left[500] == 0.0f);
}

TEST_CASE(track_slots_stay_with_their_track) {
    TrackSlots slots;
    const auto a = slots.slotFor(10);
    const auto b = slots.slotFor(20);
    CHECK(a != b);
    CHECK(slots.slotFor(10) == a);
    slots.retainOnly({20});
    CHECK(slots.slotFor(20) == b);
    CHECK(slots.slotFor(30) == a); // The freed slot is reused.
}

TEST_CASE(needed_blocks_cover_the_lookahead_and_loop_start) {
    const auto source = ramp(static_cast<std::size_t>(MediaRuntime::kBlockFrames) * 8);
    Rig rig;
    auto runtime = makeMedia(L"nb.wav", source, source);
    const Id media = rig.media(runtime, L"nb");
    addClip(rig.project, rig.track, media, 0);
    rig.publish();
    const std::int64_t B = MediaRuntime::kBlockFrames;
    auto needed = neededBlocks(*rig.snapshot, 2 * B + 5, B);
    CHECK(needed.count({runtime.get(), 2}) == 1);
    CHECK(needed.count({runtime.get(), 3}) == 1);
    CHECK(needed.count({runtime.get(), 1}) == 0);
    CHECK(needed.count({runtime.get(), 5}) == 0);

    setLoop(rig.project, true, framesToTime(100, kRate), framesToTime(4 * B, kRate));
    rig.publish();
    needed = neededBlocks(*rig.snapshot, 4 * B - 10, B);
    CHECK(needed.count({runtime.get(), 3}) == 1);
    CHECK(needed.count({runtime.get(), 0}) == 1); // The loop start is ready before the wrap.
}

TEST_CASE(no_heap_or_disk_use_while_playing_clips) {
    extern int ldawRealtimeAllocations();
    Rig rig;
    const auto source = sineWave(200000, 440.0);
    const Id media = rig.media(makeMedia(L"rt.wav", source, source), L"rt");
    const Id second = addTrack(rig.project, "T2");
    addClip(rig.project, rig.track, media, 0);
    addClip(rig.project, second, media, kSec);
    setLoop(rig.project, true, 0, 2 * kSec);
    rig.publish();
    rig.engine.post({EngineCommandType::SetMetronome, 1.0});
    const int allocationsBefore = ldawRealtimeAllocations();
    const std::uint64_t readsBefore = g_realtimeDiskReads.load();
    (void)rig.render(400000, 128);
    CHECK(ldawRealtimeAllocations() == allocationsBefore);
    CHECK(g_realtimeDiskReads.load() == readsBefore);
}

TEST_CASE(a_stale_preload_answer_does_not_start_playback_early) {
    Rig rig;
    const auto source = sineWave(48000, 440.0);
    const Id media = rig.media(makeMedia(L"gate.wav", source, source), L"gate");
    addClip(rig.project, rig.track, media, 0);
    rig.publish();
    rig.engine.setPreloadGate(true);

    // An earlier Play at position 0 was answered.
    rig.engine.post({EngineCommandType::Play, 0.0});
    (void)renderOffline(rig.engine, 128, kRate, 128);
    rig.engine.markPreloaded(0);
    (void)renderOffline(rig.engine, 128, kRate, 128);
    CHECK(rig.engine.state().positionFrames == 128);

    // Stopped and rewound, then played again: it must wait for a new answer.
    rig.engine.post({EngineCommandType::Stop, 0.0});
    (void)renderOffline(rig.engine, 4800, kRate, 128);
    rig.engine.post({EngineCommandType::Rewind, 0.0});
    rig.engine.post({EngineCommandType::Play, 0.0});
    (void)renderOffline(rig.engine, 256, kRate, 128);
    CHECK(rig.engine.state().positionFrames == 0);
    CHECK(rig.engine.pendingPreloadFrame() == 0);
    rig.engine.markPreloaded(0);
    (void)renderOffline(rig.engine, 128, kRate, 128);
    CHECK(rig.engine.state().positionFrames == 128);
    rig.engine.setPreloadGate(false);
}

TEST_CASE(preload_wait_gives_up_after_a_bounded_time) {
    Rig rig;
    const auto source = sineWave(48000, 440.0);
    const Id media = rig.media(makeMedia(L"gate2.wav", source, source), L"gate2");
    addClip(rig.project, rig.track, media, 0);
    rig.publish();
    rig.engine.setPreloadGate(true);
    rig.engine.post({EngineCommandType::Play, 0.0});
    (void)renderOffline(rig.engine, 48000, kRate, 128); // One second, and no streamer ever answers.
    CHECK(rig.engine.state().positionFrames > 0);
    rig.engine.setPreloadGate(false);
}
