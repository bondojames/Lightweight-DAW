// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "engine/dsp/Metronome.h"
#include "engine/dsp/TestTone.h"
#include "engine/graph/ArrangementSnapshot.h"
#include "engine/graph/IAudioProcessor.h"
#include "engine/runtime/SpscQueue.h"
#include "engine/transport/TempoMap.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace ldaw::engine {

enum class EngineCommandType { Play, Stop, Rewind, Seek, SetMetronome, SetTestTone, SetTempo, SetOutputGain };

struct EngineCommand {
    EngineCommandType type = EngineCommandType::Stop;
    double value = 0.0; // For Seek: the frame.
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
    static constexpr std::int64_t kNoPendingPreload = -1;

    AudioEngine();

    bool post(const EngineCommand& command) { return commands_.tryPush(command); }
    EngineState state() const;
    // Largest absolute output sample since the last call.
    float takeOutputPeak() { return outputPeak_.exchange(0.0f, std::memory_order_relaxed); }

    // Publishes a new arrangement; it takes effect at the next block. Returns the old one,
    // which stays in use until processCount has advanced by 2. Never call from the audio thread.
    const ArrangementSnapshot* exchangeSnapshot(const ArrangementSnapshot* snapshot) { return snapshot_.exchange(snapshot, std::memory_order_acq_rel); }
    std::uint64_t processCount() const { return processCount_.load(std::memory_order_acquire); }

    // When a disk streamer is attached, Play and Seek wait briefly (bounded) for it to load
    // the audio at the new position, so playback never starts with a gap.
    void setPreloadGate(bool enabled) { preloadGate_.store(enabled, std::memory_order_relaxed); }
    std::int64_t pendingPreloadFrame() const { return pendingPreload_.load(std::memory_order_acquire); }
    void markPreloaded(std::int64_t frame) { preloadedFrame_.store(frame, std::memory_order_release); }

    // Audio frames that played as silence because the disk couldn't keep up.
    std::uint64_t diskMissFrames() const { return diskMisses_.load(std::memory_order_relaxed); }

    // prepare and release must not run while process is running. A sample-rate change
    // keeps the musical position; the transport stops, since the device is changing.
    void prepare(double sampleRate, std::uint32_t maxFrames) override;
    void process(const ProcessContext& context) noexcept override;
    std::uint32_t latencyFrames() const noexcept override { return 0; }
    void release() override {}

private:
    static constexpr std::uint32_t kChunkFrames = 1024;

    struct TrackState {
        float left = 0.0f;
        float right = 0.0f;
    };

    void applyCommand(const EngineCommand& command) noexcept;
    void beginPreloadWait() noexcept;
    void renderChunk(float* left, float* right, std::uint32_t frames, std::int64_t position, const ArrangementSnapshot* snapshot) noexcept;
    void renderTracks(float* left, float* right, std::uint32_t frames, std::int64_t position, const ArrangementSnapshot& snapshot) noexcept;
    void publish() noexcept;

    SpscQueue<EngineCommand, 256> commands_;
    TempoMap tempo_;
    Metronome metronome_;
    TestTone testTone_;

    std::int64_t position_ = 0;
    bool playing_ = false;
    bool stopping_ = false;
    bool waitingForPreload_ = false;
    std::int64_t preloadWaitLeft_ = 0;
    bool metronomeEnabled_ = false;
    bool testToneEnabled_ = false;
    float transportGain_ = 1.0f;
    float transportGainStep_ = 0.001f;
    float outputGain_ = 1.0f;
    float trackGainStep_ = 0.001f;

    std::array<TrackState, ArrangementSnapshot::kMaxTracks> trackStates_{};
    std::array<float, kChunkFrames> busLeft_{};
    std::array<float, kChunkFrames> busRight_{};

    std::atomic<const ArrangementSnapshot*> snapshot_{nullptr};
    std::atomic<std::uint64_t> processCount_{0};
    std::atomic<bool> preloadGate_{false};
    std::atomic<std::int64_t> pendingPreload_{kNoPendingPreload};
    std::atomic<std::int64_t> preloadedFrame_{kNoPendingPreload};
    std::atomic<std::uint64_t> diskMisses_{0};

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
