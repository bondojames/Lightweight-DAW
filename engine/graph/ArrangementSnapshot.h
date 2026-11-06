// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/model/Project.h"
#include "engine/media/MediaRuntime.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace ldaw::engine {

struct ClipRt {
    std::int64_t start = 0;
    std::int64_t end = 0;
    std::int64_t sourceOffset = 0;
    float gain = 1.0f;
    const MediaRuntime* media = nullptr;
};

struct TrackRt {
    std::uint32_t slot = 0; // Index of this track's persistent state in the engine.
    float gainLeft = 1.0f;
    float gainRight = 1.0f;
    std::vector<ClipRt> clips; // Sorted by start.
    std::int64_t longestClip = 0;
};

// An immutable, frames-at-this-rate compilation of a Project, built away from the audio
// thread. The audio thread sees a whole new version or the old one, never a mix. It only
// reads this, and it never frees it.
struct ArrangementSnapshot {
    static constexpr std::uint32_t kMaxTracks = 256;
    double sampleRate = 0.0;
    std::vector<TrackRt> tracks;
    bool loopEnabled = false;
    std::int64_t loopStart = 0;
    std::int64_t loopEnd = 0;
    std::vector<std::shared_ptr<MediaRuntime>> media; // Keeps every clip's media alive.
};

using MediaLookup = std::function<std::shared_ptr<MediaRuntime>(const model::MediaItem&)>;

// Stable per-track slots, so gain smoothing state follows a track when others are removed.
class TrackSlots {
public:
    std::uint32_t slotFor(model::Id trackId);
    void retainOnly(const std::set<model::Id>& alive);

private:
    std::map<model::Id, std::uint32_t> slots_;
};

// Clips whose media isn't ready yet are left out; they appear once it is.
std::shared_ptr<const ArrangementSnapshot> compileSnapshot(const model::Project& project, double sampleRate, const MediaLookup& lookup, TrackSlots& slots);

// The blocks a worker should have loaded for playback at this position, including
// the start of the loop region when the playhead is heading for its end.
std::set<std::pair<const MediaRuntime*, std::int64_t>> neededBlocks(const ArrangementSnapshot& snapshot, std::int64_t position, std::int64_t lookaheadFrames);

} // namespace ldaw::engine
