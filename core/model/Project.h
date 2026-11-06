// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ldaw::model {

// Audio positions are stored in a unit that divides exactly into every supported
// sample rate, so a device rate change can't move anything (ADR-020).
using Time = std::int64_t;
constexpr Time kTimePerSecond = 705600000;

// Rounds to the nearest frame. Exact for 44.1, 48, 88.2, 96 and 192 kHz.
inline std::int64_t timeToFrames(Time time, double sampleRate) {
    const auto rate = static_cast<std::int64_t>(sampleRate);
    if (rate > 0 && kTimePerSecond % rate == 0 && static_cast<double>(rate) == sampleRate) {
        const std::int64_t perFrame = kTimePerSecond / rate;
        return (time >= 0 ? time + perFrame / 2 : time - perFrame / 2) / perFrame;
    }
    return std::llround(static_cast<double>(time) * sampleRate / static_cast<double>(kTimePerSecond));
}

inline Time framesToTime(std::int64_t frames, double sampleRate) {
    const auto rate = static_cast<std::int64_t>(sampleRate);
    if (rate > 0 && kTimePerSecond % rate == 0 && static_cast<double>(rate) == sampleRate) return frames * (kTimePerSecond / rate);
    return std::llround(static_cast<double>(frames) * static_cast<double>(kTimePerSecond) / sampleRate);
}

inline double timeToSeconds(Time time) { return static_cast<double>(time) / static_cast<double>(kTimePerSecond); }
inline Time secondsToTime(double seconds) { return std::llround(seconds * static_cast<double>(kTimePerSecond)); }

using Id = std::uint64_t;
constexpr Id kNoId = 0;

struct MediaItem {
    Id id = kNoId;
    std::filesystem::path path;
    Time length = 0;
    std::uint32_t channels = 0;
    double sourceSampleRate = 0.0;
};

// A window onto a media file. Editing changes only these numbers, never the file.
struct Clip {
    Id id = kNoId;
    Id mediaId = kNoId;
    Time start = 0;        // On the timeline.
    Time length = 0;
    Time sourceOffset = 0; // Where in the media this clip starts.
    float gain = 1.0f;     // Linear.
};

struct Track {
    Id id = kNoId;
    std::string name;
    float gain = 1.0f;  // Linear.
    float pan = 0.0f;   // -1 is left, +1 is right.
    bool mute = false;
    bool solo = false;
    std::vector<Clip> clips;
};

// The intended state of the song. It holds no DSP objects and no UI state; the audio
// engine runs from a snapshot compiled from this.
struct Project {
    Id nextId = 1;
    std::vector<MediaItem> media;
    std::vector<Track> tracks;
    bool loopEnabled = false;
    Time loopStart = 0;
    Time loopEnd = 0;

    const MediaItem* findMedia(Id id) const;
    const Track* findTrack(Id id) const;
    Track* findTrack(Id id);
    const Clip* findClip(Id id, Id* trackIdOut = nullptr) const;
    Clip* findClip(Id id, Id* trackIdOut = nullptr);
    Time endTime() const;
};

} // namespace ldaw::model
