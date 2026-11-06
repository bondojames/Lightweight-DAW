// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ldaw::engine {

// Converts beats to sample frames. Each beat's frame is computed directly from the
// tempo segment's start, never by adding up intervals, so rounding cannot build up
// into drift however long the song runs. One segment for now; Phase 02 onward can
// make this a list without changing the callers.
class TempoMap {
public:
    static constexpr double kMinBpm = 20.0;
    static constexpr double kMaxBpm = 400.0;

    void setSampleRate(double sampleRate) { sampleRate_ = sampleRate; }
    double sampleRate() const { return sampleRate_; }
    double bpm() const { return bpm_; }
    int beatsPerBar() const { return beatsPerBar_; }

    std::int64_t frameOfBeat(std::int64_t beat) const {
        return baseFrame_ + std::llround((static_cast<double>(beat) - baseBeat_) * framesPerBeat());
    }

    double beatAtFrame(std::int64_t frame) const {
        return baseBeat_ + static_cast<double>(frame - baseFrame_) / framesPerBeat();
    }

    // The tempo changes at this frame; everything before it keeps its timing.
    void setTempo(std::int64_t atFrame, double bpm) {
        baseBeat_ = beatAtFrame(atFrame);
        baseFrame_ = atFrame;
        bpm_ = std::clamp(bpm, kMinBpm, kMaxBpm);
    }

    void reset() {
        baseFrame_ = 0;
        baseBeat_ = 0.0;
    }

private:
    double framesPerBeat() const { return sampleRate_ * 60.0 / bpm_; }

    double sampleRate_ = 48000.0;
    double bpm_ = 120.0;
    int beatsPerBar_ = 4;
    std::int64_t baseFrame_ = 0;
    double baseBeat_ = 0.0;
};

} // namespace ldaw::engine
