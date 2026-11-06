// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "engine/transport/TempoMap.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

namespace ldaw::engine {

// Sample-accurate click. The first beat of each bar is higher and louder.
class Metronome {
public:
    static constexpr float kClickGain = 0.25f; // About -12 dBFS.
    static constexpr float kAccentGain = 0.5f; // About -6 dBFS.

    // Allocates, so never call it from the audio callback.
    void prepare(double sampleRate) {
        const auto length = static_cast<std::size_t>(sampleRate * 0.03);
        buildClick(click_, length, sampleRate, 1000.0, kClickGain);
        buildClick(accent_, length, sampleRate, 1500.0, kAccentGain);
        reset();
    }

    void reset() {
        active_ = nullptr;
        activeOffset_ = 0;
    }

    // Adds clicks for the frames [startFrame, startFrame + frames). Allocation-free.
    void render(float* left, float* right, std::uint32_t frames, std::int64_t startFrame, const TempoMap& tempo) {
        const std::int64_t endFrame = startFrame + frames;
        auto beat = static_cast<std::int64_t>(std::floor(tempo.beatAtFrame(startFrame)));
        std::int64_t cursor = startFrame;
        for (;; ++beat) {
            const std::int64_t beatFrame = tempo.frameOfBeat(beat);
            if (beatFrame >= endFrame) break;
            if (beatFrame < startFrame) continue;
            mix(left, right, cursor - startFrame, beatFrame - cursor);
            const auto beatInBar = ((beat % tempo.beatsPerBar()) + tempo.beatsPerBar()) % tempo.beatsPerBar();
            active_ = (beatInBar == 0) ? &accent_ : &click_;
            activeOffset_ = 0;
            cursor = beatFrame;
        }
        mix(left, right, cursor - startFrame, endFrame - cursor);
    }

private:
    static void buildClick(std::vector<float>& out, std::size_t length, double sampleRate, double frequency, float gain) {
        out.resize(length);
        const double decayFrames = static_cast<double>(length) / 6.0;
        for (std::size_t i = 0; i < length; ++i) {
            const double t = static_cast<double>(i);
            // Starting a sine at zero and ending on a near-silent decay avoids clicks of its own.
            const double fadeOut = static_cast<double>(length - i) / static_cast<double>(length);
            out[i] = gain * static_cast<float>(std::sin(2.0 * std::numbers::pi * frequency * t / sampleRate) *
                                               std::exp(-t / decayFrames) * fadeOut);
        }
    }

    void mix(float* left, float* right, std::int64_t offset, std::int64_t count) {
        if (!active_) return;
        const auto size = static_cast<std::int64_t>(active_->size());
        const float* data = active_->data();
        for (std::int64_t i = 0; i < count && activeOffset_ < size; ++i, ++activeOffset_) {
            left[offset + i] += data[activeOffset_];
            right[offset + i] += data[activeOffset_];
        }
        if (activeOffset_ >= size) active_ = nullptr;
    }

    std::vector<float> click_;
    std::vector<float> accent_;
    const std::vector<float>* active_ = nullptr;
    std::int64_t activeOffset_ = 0;
};

} // namespace ldaw::engine
