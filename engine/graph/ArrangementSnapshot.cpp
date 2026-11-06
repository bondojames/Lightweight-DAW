// SPDX-License-Identifier: GPL-3.0-or-later
#include "engine/graph/ArrangementSnapshot.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace ldaw::engine {

std::uint32_t TrackSlots::slotFor(model::Id trackId) {
    const auto it = slots_.find(trackId);
    if (it != slots_.end()) return it->second;
    std::set<std::uint32_t> used;
    for (const auto& [id, slot] : slots_) used.insert(slot);
    std::uint32_t slot = 0;
    while (used.count(slot) != 0) ++slot;
    slots_[trackId] = slot;
    return slot;
}

void TrackSlots::retainOnly(const std::set<model::Id>& alive) {
    for (auto it = slots_.begin(); it != slots_.end();) {
        it = alive.count(it->first) == 0 ? slots_.erase(it) : std::next(it);
    }
}

std::shared_ptr<const ArrangementSnapshot> compileSnapshot(const model::Project& project, double sampleRate, const MediaLookup& lookup, TrackSlots& slots) {
    auto snapshot = std::make_shared<ArrangementSnapshot>();
    snapshot->sampleRate = sampleRate;

    std::set<model::Id> alive;
    for (const model::Track& track : project.tracks) alive.insert(track.id);
    slots.retainOnly(alive);

    bool anySolo = false;
    for (const model::Track& track : project.tracks) anySolo = anySolo || track.solo;

    std::map<model::Id, std::shared_ptr<MediaRuntime>> runtimes;
    for (const model::MediaItem& item : project.media) {
        if (auto runtime = lookup(item)) {
            runtimes[item.id] = runtime;
            snapshot->media.push_back(std::move(runtime));
        }
    }

    for (const model::Track& track : project.tracks) {
        if (snapshot->tracks.size() >= ArrangementSnapshot::kMaxTracks) break;
        TrackRt rt;
        rt.slot = slots.slotFor(track.id);
        // Constant power, scaled so a centred track plays at its original level.
        const float audible = (track.mute || (anySolo && !track.solo)) ? 0.0f : 1.0f;
        const double angle = (static_cast<double>(track.pan) + 1.0) * std::numbers::pi / 4.0;
        rt.gainLeft = audible * track.gain * static_cast<float>(std::cos(angle) * std::numbers::sqrt2);
        rt.gainRight = audible * track.gain * static_cast<float>(std::sin(angle) * std::numbers::sqrt2);

        for (const model::Clip& clip : track.clips) {
            const auto media = runtimes.find(clip.mediaId);
            if (media == runtimes.end()) continue;
            ClipRt out;
            out.start = model::timeToFrames(clip.start, sampleRate);
            out.end = model::timeToFrames(clip.start + clip.length, sampleRate);
            out.sourceOffset = model::timeToFrames(clip.sourceOffset, sampleRate);
            out.gain = clip.gain;
            out.media = media->second.get();
            if (out.end <= out.start) continue;
            rt.longestClip = std::max(rt.longestClip, out.end - out.start);
            rt.clips.push_back(out);
        }
        std::sort(rt.clips.begin(), rt.clips.end(), [](const ClipRt& a, const ClipRt& b) { return a.start < b.start; });
        snapshot->tracks.push_back(std::move(rt));
    }

    snapshot->loopStart = model::timeToFrames(project.loopStart, sampleRate);
    snapshot->loopEnd = model::timeToFrames(project.loopEnd, sampleRate);
    snapshot->loopEnabled = project.loopEnabled && snapshot->loopEnd > snapshot->loopStart;
    return snapshot;
}

std::set<std::pair<const MediaRuntime*, std::int64_t>> neededBlocks(const ArrangementSnapshot& snapshot, std::int64_t position, std::int64_t lookaheadFrames) {
    std::set<std::pair<const MediaRuntime*, std::int64_t>> needed;
    const auto cover = [&](std::int64_t from, std::int64_t to) {
        for (const TrackRt& track : snapshot.tracks) {
            for (const ClipRt& clip : track.clips) {
                const std::int64_t start = std::max(from, clip.start);
                const std::int64_t end = std::min(to, clip.end);
                if (start >= end) continue;
                const std::int64_t first = clip.sourceOffset + (start - clip.start);
                const std::int64_t last = clip.sourceOffset + (end - clip.start) - 1;
                const std::int64_t lastBlock = std::min((std::max<std::int64_t>(last, 0)) / MediaRuntime::kBlockFrames, clip.media->blockCount() - 1);
                for (std::int64_t block = std::max<std::int64_t>(first, 0) / MediaRuntime::kBlockFrames; block <= lastBlock; ++block) {
                    needed.insert({clip.media, block});
                }
            }
        }
    };

    if (snapshot.loopEnabled && position < snapshot.loopEnd) {
        // Heading into the loop end: also have the loop start ready for the wrap.
        const std::int64_t beforeEnd = std::min(lookaheadFrames, snapshot.loopEnd - position);
        cover(position, position + beforeEnd);
        const std::int64_t length = snapshot.loopEnd - snapshot.loopStart;
        cover(snapshot.loopStart, snapshot.loopStart + std::min(lookaheadFrames, length));
    } else {
        cover(position, position + lookaheadFrames);
    }
    return needed;
}

} // namespace ldaw::engine
