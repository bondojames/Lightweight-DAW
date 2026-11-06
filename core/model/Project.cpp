// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/model/Project.h"

#include <algorithm>

namespace ldaw::model {

const MediaItem* Project::findMedia(Id id) const {
    for (const MediaItem& item : media) {
        if (item.id == id) return &item;
    }
    return nullptr;
}

const Track* Project::findTrack(Id id) const {
    for (const Track& track : tracks) {
        if (track.id == id) return &track;
    }
    return nullptr;
}

Track* Project::findTrack(Id id) { return const_cast<Track*>(static_cast<const Project*>(this)->findTrack(id)); }

const Clip* Project::findClip(Id id, Id* trackIdOut) const {
    for (const Track& track : tracks) {
        for (const Clip& clip : track.clips) {
            if (clip.id == id) {
                if (trackIdOut) *trackIdOut = track.id;
                return &clip;
            }
        }
    }
    return nullptr;
}

Clip* Project::findClip(Id id, Id* trackIdOut) { return const_cast<Clip*>(static_cast<const Project*>(this)->findClip(id, trackIdOut)); }

Time Project::endTime() const {
    Time end = 0;
    for (const Track& track : tracks) {
        for (const Clip& clip : track.clips) end = std::max(end, clip.start + clip.length);
    }
    return end;
}

} // namespace ldaw::model
