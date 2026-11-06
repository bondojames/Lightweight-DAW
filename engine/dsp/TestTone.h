// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cmath>
#include <cstdint>
#include <numbers>

namespace ldaw::engine {

// A plain sine for checking a signal path, faded in and out so toggling cannot click.
class TestTone {
public:
    static constexpr double kFrequency = 440.0;
    static constexpr float kLevel = 0.125f; // About -18 dBFS.

    void prepare(double sampleRate) {
        phaseStep_ = 2.0 * std::numbers::pi * kFrequency / sampleRate;
        gainStep_ = 1.0f / static_cast<float>(sampleRate * 0.005);
    }

    void setEnabled(bool enabled) { target_ = enabled ? 1.0f : 0.0f; }
    bool isSilent() const { return gain_ == 0.0f && target_ == 0.0f; }

    void render(float* left, float* right, std::uint32_t frames) {
        if (isSilent()) return;
        for (std::uint32_t i = 0; i < frames; ++i) {
            if (gain_ < target_) gain_ = gain_ + gainStep_ < target_ ? gain_ + gainStep_ : target_;
            else if (gain_ > target_) gain_ = gain_ - gainStep_ > target_ ? gain_ - gainStep_ : target_;
            const float sample = kLevel * gain_ * static_cast<float>(std::sin(phase_));
            left[i] += sample;
            right[i] += sample;
            phase_ += phaseStep_;
            if (phase_ >= 2.0 * std::numbers::pi) phase_ -= 2.0 * std::numbers::pi;
        }
    }

private:
    double phase_ = 0.0;
    double phaseStep_ = 0.0;
    float gain_ = 0.0f;
    float target_ = 0.0f;
    float gainStep_ = 0.001f;
};

} // namespace ldaw::engine
