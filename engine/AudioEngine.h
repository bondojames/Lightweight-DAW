// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "engine/dsp/Metronome.h"
#include "engine/dsp/TestTone.h"
#include "engine/graph/IAudioProcessor.h"
#include "engine/runtime/SpscQueue.h"
#include "engine/transport/TempoMap.h"

#include <atomic>
#include <cstdint>

namespace ldaw::engine {

enum class EngineCommandType { Play, Stop, Rewind, SetMetronome, SetTestTone, SetTempo, SetOutputGain };

struct EngineCommand {
    EngineCommandType type = EngineCommandType::Stop;
    double value = 0.0;
};

// Read-only state for other threads, each field individually atomic.
struct EngineState {
    std::int64_t positionFrames = 0;
    double positionBeats = 0.0;
    double sampleRate = 0.0;
    double bpm = 0.0;
    bool playing = false;
    bool metronome = false;
    bool testTone = false;
};

// The sound-making core. The same code runs for a live device and for offline render,
// and it knows nothing about windows, GPUs or drivers. One thread may post commands,
// and they take effect at the start of the next processing block.
class AudioEngine final : public IAudioProcessor {
public:
    AudioEngine();

    bool post(const EngineCommand& command) { return commands_.tryPush(command); }
    EngineState state() const;
    // Largest absolute output sample since the last call.
    float takeOutputPeak() { return outputPeak_.exchange(0.0f, std::memory_order_relaxed); }

    // prepare and release must not run while process is running. A sample-rate change
    // keeps the musical position; the transport stops, since the device is changing.
    void prepare(double sampleRate, std::uint32_t maxFrames) override;
    void process(const ProcessContext& context) noexcept override;
    std::uint32_t latencyFrames() const noexcept override { return 0; }
    void release() override {}

private:
    void applyCommand(const EngineCommand& command) noexcept;
    void publish() noexcept;

    SpscQueue<EngineCommand, 256> commands_;
    TempoMap tempo_;
    Metronome metronome_;
    TestTone testTone_;

    std::int64_t position_ = 0;
    bool playing_ = false;
    bool stopping_ = false;
    bool metronomeEnabled_ = false;
    bool testToneEnabled_ = false;
    float transportGain_ = 1.0f;
    float transportGainStep_ = 0.001f;
    float outputGain_ = 1.0f;

    std::atomic<std::int64_t> publishedPosition_{0};
    std::atomic<double> publishedBeat_{0.0};
    std::atomic<double> publishedSampleRate_{0.0};
    std::atomic<double> publishedBpm_{120.0};
    std::atomic<bool> publishedPlaying_{false};
    std::atomic<bool> publishedMetronome_{false};
    std::atomic<bool> publishedTestTone_{false};
    std::atomic<float> outputPeak_{0.0f};
};

} // namespace ldaw::engine
