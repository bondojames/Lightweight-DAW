// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/commands/Edits.h"
#include "tests/unit/test_framework.h"

#include <cmath>
#include <limits>

using namespace ldaw::commands;
using namespace ldaw::model;

namespace {

constexpr Time kSec = kTimePerSecond;

struct Fixture {
    Project project;
    Id track = 0;
    Id media = 0;
    Id clip = 0;
    Fixture() {
        track = addTrack(project, "Audio 1");
        media = addMedia(project, "song.wav", 10 * kSec, 2, 48000.0);
        clip = addClip(project, track, media, 2 * kSec);
    }
    const Clip& get(Id id) const { return *project.findClip(id); }
};

// Which media time plays at a given timeline time. Edits must not change this.
Time mediaTimeAt(const Clip& clip, Time timeline) { return clip.sourceOffset + (timeline - clip.start); }

} // namespace

TEST_CASE(time_is_exact_at_all_supported_sample_rates) {
    for (const double rate : {44100.0, 48000.0, 88200.0, 96000.0, 192000.0}) {
        const std::int64_t frames = 123456789;
        CHECK(timeToFrames(framesToTime(frames, rate), rate) == frames);
        CHECK(framesToTime(static_cast<std::int64_t>(rate), rate) == kTimePerSecond);
    }
}

TEST_CASE(time_keeps_a_position_across_rate_changes) {
    const Time twoHours = 2 * 3600 * kSec;
    CHECK(timeToFrames(twoHours, 96000.0) == std::int64_t{2} * 3600 * 96000);
    CHECK(timeToFrames(twoHours, 44100.0) == std::int64_t{2} * 3600 * 44100);
}

TEST_CASE(time_handles_an_unusual_rate) {
    CHECK(std::abs(timeToFrames(kTimePerSecond, 44123.0) - 44123) <= 1);
    CHECK(timeToFrames(kTimePerSecond / 2, 48000.0) == 24000);
}

TEST_CASE(add_clip_covers_the_whole_media) {
    Fixture f;
    const Clip& clip = f.get(f.clip);
    CHECK(clip.start == 2 * kSec);
    CHECK(clip.length == 10 * kSec);
    CHECK(clip.sourceOffset == 0);
    CHECK(f.project.endTime() == 12 * kSec);
}

TEST_CASE(adding_the_same_file_twice_shares_media) {
    Fixture f;
    CHECK(addMedia(f.project, "song.wav", 10 * kSec, 2, 48000.0) == f.media);
    CHECK(f.project.media.size() == 1);
}

TEST_CASE(add_refuses_bad_references) {
    Fixture f;
    CHECK(addClip(f.project, 999, f.media, 0) == kNoId);
    CHECK(addClip(f.project, f.track, 999, 0) == kNoId);
    CHECK(addMedia(f.project, "x.wav", 0, 2, 48000.0) == kNoId);
    CHECK(addMedia(f.project, "x.wav", kSec, 0, 48000.0) == kNoId);
}

TEST_CASE(move_within_and_between_tracks) {
    Fixture f;
    const Id second = addTrack(f.project, "Audio 2");
    CHECK(moveClip(f.project, f.clip, f.track, 5 * kSec));
    CHECK(f.get(f.clip).start == 5 * kSec);
    CHECK(moveClip(f.project, f.clip, second, 6 * kSec));
    Id on = 0;
    f.project.findClip(f.clip, &on);
    CHECK(on == second);
    CHECK(f.project.findTrack(f.track)->clips.empty());
    CHECK(!moveClip(f.project, f.clip, 999, 0));
    CHECK(moveClip(f.project, f.clip, second, -5 * kSec));
    CHECK(f.get(f.clip).start == 0);
}

TEST_CASE(trim_start_keeps_the_audio_in_place) {
    Fixture f;
    const Time anchor = 5 * kSec;
    const Time before = mediaTimeAt(f.get(f.clip), anchor);
    CHECK(trimClipStart(f.project, f.clip, 3 * kSec));
    const Clip& clip = f.get(f.clip);
    CHECK(clip.start == 3 * kSec);
    CHECK(clip.sourceOffset == kSec);
    CHECK(clip.length == 9 * kSec);
    CHECK(mediaTimeAt(clip, anchor) == before);
}

TEST_CASE(trim_start_stops_at_the_media_start) {
    Fixture f;
    CHECK(trimClipStart(f.project, f.clip, 3 * kSec));
    CHECK(trimClipStart(f.project, f.clip, 0));
    CHECK(f.get(f.clip).start == 2 * kSec);
    CHECK(f.get(f.clip).sourceOffset == 0);
}

TEST_CASE(trim_start_cannot_pass_the_end) {
    Fixture f;
    CHECK(trimClipStart(f.project, f.clip, 500 * kSec));
    CHECK(f.get(f.clip).length > 0);
    CHECK(f.get(f.clip).start < 12 * kSec);
}

TEST_CASE(trim_end_stops_at_the_media_end) {
    Fixture f;
    CHECK(trimClipEnd(f.project, f.clip, 500 * kSec));
    CHECK(f.get(f.clip).length == 10 * kSec);
    CHECK(trimClipEnd(f.project, f.clip, 4 * kSec));
    CHECK(f.get(f.clip).length == 2 * kSec);
    CHECK(trimClipEnd(f.project, f.clip, -500 * kSec));
    CHECK(f.get(f.clip).length > 0);
}

TEST_CASE(split_makes_two_clips_playing_the_same_audio) {
    Fixture f;
    const Id right = splitClip(f.project, f.clip, 5 * kSec);
    CHECK(right != kNoId);
    const Clip& a = f.get(f.clip);
    const Clip& b = f.get(right);
    CHECK(a.start + a.length == b.start);
    CHECK(b.sourceOffset == 3 * kSec);
    CHECK(a.length + b.length == 10 * kSec);
    CHECK(mediaTimeAt(b, 7 * kSec) == 5 * kSec);
    CHECK(a.mediaId == b.mediaId);
}

TEST_CASE(split_refuses_points_outside_the_clip) {
    Fixture f;
    CHECK(splitClip(f.project, f.clip, kSec) == kNoId);
    CHECK(splitClip(f.project, f.clip, 2 * kSec) == kNoId);
    CHECK(splitClip(f.project, f.clip, 12 * kSec) == kNoId);
    CHECK(splitClip(f.project, 999, 5 * kSec) == kNoId);
}

TEST_CASE(duplicate_lands_right_after_the_original) {
    Fixture f;
    trimClipStart(f.project, f.clip, 3 * kSec);
    const Id copy = duplicateClip(f.project, f.clip);
    const Clip& a = f.get(f.clip);
    const Clip& b = f.get(copy);
    CHECK(b.start == a.start + a.length);
    CHECK(b.sourceOffset == a.sourceOffset);
    CHECK(b.length == a.length);
    CHECK(b.id != a.id);
}

TEST_CASE(delete_clip_and_track) {
    Fixture f;
    CHECK(deleteClip(f.project, f.clip));
    CHECK(!deleteClip(f.project, f.clip));
    CHECK(f.project.findClip(f.clip) == nullptr);
    CHECK(removeTrack(f.project, f.track));
    CHECK(!removeTrack(f.project, f.track));
}

TEST_CASE(gain_and_pan_are_validated) {
    Fixture f;
    CHECK(setTrackGain(f.project, f.track, 0.5f));
    CHECK(f.project.findTrack(f.track)->gain == 0.5f);
    CHECK(setTrackGain(f.project, f.track, 1000.0f));
    CHECK(f.project.findTrack(f.track)->gain < 20.0f);
    CHECK(setTrackGain(f.project, f.track, -1.0f));
    CHECK(f.project.findTrack(f.track)->gain == 0.0f);
    CHECK(!setTrackGain(f.project, f.track, std::nanf("")));
    CHECK(setTrackPan(f.project, f.track, 9.0f));
    CHECK(f.project.findTrack(f.track)->pan == 1.0f);
    CHECK(!setTrackPan(f.project, f.track, std::nanf("")));
    CHECK(!setClipGain(f.project, f.clip, std::numeric_limits<float>::infinity()));
}

TEST_CASE(loop_region_must_make_sense) {
    Fixture f;
    CHECK(setLoop(f.project, true, kSec, 2 * kSec));
    CHECK(f.project.loopEnabled);
    CHECK(!setLoop(f.project, true, 2 * kSec, kSec));
    CHECK(f.project.loopEnd == 2 * kSec); // Unchanged by the refused edit.
    CHECK(!setLoop(f.project, true, -kSec, kSec));
    CHECK(setLoop(f.project, false, 0, 0));
}

TEST_CASE(undo_and_redo_restore_exact_states) {
    ProjectHistory history;
    Id track = 0;
    CHECK(history.apply("Add track", [&](Project& p) {
        track = addTrack(p, "A");
        return true;
    }));
    const auto v1 = history.current();
    CHECK(history.apply("Rename", [&](Project& p) { return setTrackGain(p, track, 0.5f); }));
    const auto v2 = history.current();
    CHECK(history.canUndo());
    CHECK(history.undoName() == "Rename");
    CHECK(history.undo());
    CHECK(history.current() == v1);
    CHECK(history.redoName() == "Rename");
    CHECK(history.redo());
    CHECK(history.current() == v2);
    CHECK(!history.canRedo());
}

TEST_CASE(a_refused_edit_records_nothing) {
    ProjectHistory history;
    const auto before = history.current();
    const auto revision = history.revision();
    CHECK(!history.apply("Nope", [](Project& p) { return deleteClip(p, 999); }));
    CHECK(history.current() == before);
    CHECK(history.revision() == revision);
    CHECK(!history.canUndo());
}

TEST_CASE(a_new_edit_clears_redo) {
    ProjectHistory history;
    history.apply("A", [](Project& p) { addTrack(p, "A"); return true; });
    history.undo();
    CHECK(history.canRedo());
    history.apply("B", [](Project& p) { addTrack(p, "B"); return true; });
    CHECK(!history.canRedo());
}

TEST_CASE(old_versions_are_unaffected_by_later_edits) {
    ProjectHistory history;
    history.apply("A", [](Project& p) { addTrack(p, "A"); return true; });
    const auto old = history.current();
    history.apply("B", [](Project& p) { addTrack(p, "B"); return true; });
    CHECK(old->tracks.size() == 1);
    CHECK(history.current()->tracks.size() == 2);
}

TEST_CASE(undo_depth_is_bounded) {
    ProjectHistory history;
    for (int i = 0; i < 400; ++i) history.apply("A", [](Project& p) { addTrack(p, "A"); return true; });
    int undos = 0;
    while (history.undo()) ++undos;
    CHECK(undos == 256);
}

TEST_CASE(grouped_edits_are_one_undo_step) {
    ProjectHistory history;
    Id track = 0;
    history.apply("Add", [&](Project& p) { track = addTrack(p, "A"); return true; });
    const auto before = history.current();
    history.applyGrouped("Gain", [&](Project& p) { return setTrackGain(p, track, 0.9f); });
    history.applyGrouped("Gain", [&](Project& p) { return setTrackGain(p, track, 0.5f); });
    history.applyGrouped("Gain", [&](Project& p) { return setTrackGain(p, track, 0.25f); });
    history.endGroup();
    CHECK(history.current()->findTrack(track)->gain == 0.25f);
    CHECK(history.undo());
    CHECK(history.current() == before);
    CHECK(history.redo());
    CHECK(history.current()->findTrack(track)->gain == 0.25f);
}

TEST_CASE(a_second_drag_is_a_separate_undo_step) {
    ProjectHistory history;
    Id track = 0;
    history.apply("Add", [&](Project& p) { track = addTrack(p, "A"); return true; });
    history.applyGrouped("Gain", [&](Project& p) { return setTrackGain(p, track, 0.5f); });
    history.endGroup();
    history.applyGrouped("Gain", [&](Project& p) { return setTrackGain(p, track, 0.25f); });
    history.endGroup();
    CHECK(history.undo());
    CHECK(history.current()->findTrack(track)->gain == 0.5f);
}
