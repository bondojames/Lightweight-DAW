// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/commands/Edits.h"

#include <algorithm>
#include <cmath>

namespace ldaw::commands {

using model::Clip;
using model::kNoId;
using model::MediaItem;
using model::Track;

namespace {

// Shortest clip an edit may leave, so a clip can't vanish or become impossible to grab.
constexpr Time kMinClipLength = model::kTimePerSecond / 1000;
constexpr float kMaxGain = 15.85f; // +24 dB

Id newId(Project& project) { return project.nextId++; }

} // namespace

Id addTrack(Project& project, const std::string& name) {
    Track track;
    track.id = newId(project);
    track.name = name;
    project.tracks.push_back(std::move(track));
    return project.tracks.back().id;
}

bool removeTrack(Project& project, Id trackId) {
    const auto it = std::find_if(project.tracks.begin(), project.tracks.end(), [&](const Track& t) { return t.id == trackId; });
    if (it == project.tracks.end()) return false;
    project.tracks.erase(it);
    return true;
}

Id addMedia(Project& project, const std::filesystem::path& path, Time length, std::uint32_t channels, double sourceSampleRate) {
    if (length <= 0 || channels == 0) return kNoId;
    for (const MediaItem& item : project.media) {
        if (item.path == path) return item.id; // Same file, same media.
    }
    MediaItem item;
    item.id = newId(project);
    item.path = path;
    item.length = length;
    item.channels = channels;
    item.sourceSampleRate = sourceSampleRate;
    project.media.push_back(std::move(item));
    return project.media.back().id;
}

Id addClip(Project& project, Id trackId, Id mediaId, Time start) {
    Track* track = project.findTrack(trackId);
    const MediaItem* media = project.findMedia(mediaId);
    if (!track || !media) return kNoId;
    Clip clip;
    clip.id = newId(project);
    clip.mediaId = mediaId;
    clip.start = std::max<Time>(0, start);
    clip.length = media->length;
    track->clips.push_back(clip);
    return clip.id;
}

bool deleteClip(Project& project, Id clipId) {
    for (Track& track : project.tracks) {
        const auto it = std::find_if(track.clips.begin(), track.clips.end(), [&](const Clip& c) { return c.id == clipId; });
        if (it != track.clips.end()) {
            track.clips.erase(it);
            return true;
        }
    }
    return false;
}

bool moveClip(Project& project, Id clipId, Id targetTrackId, Time newStart) {
    Id fromTrackId = kNoId;
    Clip* found = project.findClip(clipId, &fromTrackId);
    Track* target = project.findTrack(targetTrackId);
    if (!found || !target) return false;
    Clip clip = *found;
    clip.start = std::max<Time>(0, newStart);
    if (fromTrackId == targetTrackId) {
        *found = clip;
        return true;
    }
    deleteClip(project, clipId);
    project.findTrack(targetTrackId)->clips.push_back(clip);
    return true;
}

bool trimClipStart(Project& project, Id clipId, Time newStart) {
    Clip* clip = project.findClip(clipId);
    if (!clip) return false;
    const Time end = clip->start + clip->length;
    // The left edge can't pass the right edge, the timeline start, or the start of the media.
    const Time earliest = std::max<Time>(0, clip->start - clip->sourceOffset);
    const Time latest = end - kMinClipLength;
    if (latest < earliest) return false;
    const Time start = std::clamp(newStart, earliest, latest);
    clip->sourceOffset += start - clip->start;
    clip->length = end - start;
    clip->start = start;
    return true;
}

bool trimClipEnd(Project& project, Id clipId, Time newEnd) {
    Clip* clip = project.findClip(clipId);
    if (!clip) return false;
    const MediaItem* media = project.findMedia(clip->mediaId);
    if (!media) return false;
    const Time latest = clip->start + (media->length - clip->sourceOffset);
    const Time earliest = clip->start + kMinClipLength;
    if (latest < earliest) return false;
    clip->length = std::clamp(newEnd, earliest, latest) - clip->start;
    return true;
}

Id splitClip(Project& project, Id clipId, Time at) {
    Id trackId = kNoId;
    Clip* clip = project.findClip(clipId, &trackId);
    if (!clip) return kNoId;
    if (at < clip->start + kMinClipLength || at > clip->start + clip->length - kMinClipLength) return kNoId;
    Clip right = *clip;
    right.id = newId(project);
    right.start = at;
    right.sourceOffset = clip->sourceOffset + (at - clip->start);
    right.length = clip->start + clip->length - at;
    clip->length = at - clip->start;
    project.findTrack(trackId)->clips.push_back(right);
    return right.id;
}

Id duplicateClip(Project& project, Id clipId) {
    Id trackId = kNoId;
    const Clip* clip = project.findClip(clipId, &trackId);
    if (!clip) return kNoId;
    Clip copy = *clip;
    copy.id = newId(project);
    copy.start = clip->start + clip->length; // Directly after the original.
    project.findTrack(trackId)->clips.push_back(copy);
    return copy.id;
}

bool setClipGain(Project& project, Id clipId, float gain) {
    Clip* clip = project.findClip(clipId);
    if (!clip || !std::isfinite(gain)) return false;
    clip->gain = std::clamp(gain, 0.0f, kMaxGain);
    return true;
}

bool setTrackGain(Project& project, Id trackId, float gain) {
    Track* track = project.findTrack(trackId);
    if (!track || !std::isfinite(gain)) return false;
    track->gain = std::clamp(gain, 0.0f, kMaxGain);
    return true;
}

bool setTrackPan(Project& project, Id trackId, float pan) {
    Track* track = project.findTrack(trackId);
    if (!track || !std::isfinite(pan)) return false;
    track->pan = std::clamp(pan, -1.0f, 1.0f);
    return true;
}

bool setTrackMute(Project& project, Id trackId, bool mute) {
    Track* track = project.findTrack(trackId);
    if (!track) return false;
    track->mute = mute;
    return true;
}

bool setTrackSolo(Project& project, Id trackId, bool solo) {
    Track* track = project.findTrack(trackId);
    if (!track) return false;
    track->solo = solo;
    return true;
}

bool setLoop(Project& project, bool enabled, Time start, Time end) {
    if (start < 0 || (enabled && end <= start)) return false;
    project.loopEnabled = enabled;
    project.loopStart = start;
    project.loopEnd = end;
    return true;
}

ProjectHistory::ProjectHistory() : current_(std::make_shared<const Project>()) {}

bool ProjectHistory::apply(const std::string& name, const Edit& edit) {
    auto next = std::make_shared<Project>(*current_);
    if (!edit(*next)) return false;
    groupOpen_ = false;
    undo_.push_back({name, current_});
    if (undo_.size() > kMaxUndo) undo_.erase(undo_.begin());
    redo_.clear();
    current_ = std::move(next);
    ++revision_;
    return true;
}

bool ProjectHistory::applyGrouped(const std::string& name, const Edit& edit) {
    if (!groupOpen_ || undo_.empty() || undo_.back().name != name) {
        if (!apply(name, edit)) return false;
        groupOpen_ = true;
        return true;
    }
    auto next = std::make_shared<Project>(*current_);
    if (!edit(*next)) return false;
    current_ = std::move(next);
    ++revision_;
    return true;
}

const std::string& ProjectHistory::undoName() const {
    static const std::string kEmpty;
    return undo_.empty() ? kEmpty : undo_.back().name;
}

const std::string& ProjectHistory::redoName() const {
    static const std::string kEmpty;
    return redo_.empty() ? kEmpty : redo_.back().name;
}

bool ProjectHistory::undo() {
    if (undo_.empty()) return false;
    redo_.push_back({undo_.back().name, current_});
    current_ = undo_.back().project;
    undo_.pop_back();
    ++revision_;
    return true;
}

bool ProjectHistory::redo() {
    if (redo_.empty()) return false;
    undo_.push_back({redo_.back().name, current_});
    current_ = redo_.back().project;
    redo_.pop_back();
    ++revision_;
    return true;
}

} // namespace ldaw::commands
