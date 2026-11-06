// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace ldaw::engine {

struct ProcessContext {
    float* const* inputs = nullptr;
    float* const* outputs = nullptr;
    std::uint32_t inputChannels = 0;
    std::uint32_t outputChannels = 0;
    std::uint32_t frames = 0;
    std::int64_t transportFrame = 0; // Authoritative 64-bit sample clock.
    double sampleRate = 0.0;
};

// process() runs on the audio thread: no allocation, locks, or I/O (spec 4.4).
class IAudioProcessor {
public:
    virtual ~IAudioProcessor() = default;
    virtual void prepare(double sampleRate, std::uint32_t maxFrames) = 0;
    virtual void process(const ProcessContext& context) noexcept = 0;
    virtual std::uint32_t latencyFrames() const noexcept = 0; // For delay compensation.
    virtual void release() = 0;
};

} // namespace ldaw::engine
