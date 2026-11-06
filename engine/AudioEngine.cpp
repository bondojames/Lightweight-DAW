// SPDX-License-Identifier: GPL-3.0-or-later
#include "engine/AudioEngine.h"

#include <algorithm>
#include <cmath>

namespace ldaw::engine {
namespace {

constexpr double kPreloadWaitSeconds = 0.3; // After this, play anyway and count any gap.
constexpr double kTrackGainSmoothSeconds = 0.01;

float moveToward(float value, float target, float step) {
    if (value < target) return std::min(value + step, target);
    if (value > target) return std::max(value - step, target);
    return value;
}

} // namespace

AudioEngine::AudioEngine() { prepare(48000.0, 4096); }

EngineState AudioEngine::state() const {
    EngineState out;
    out.positionFrames = publishedPosition_.load(std::memory_order_relaxed);
    out.positionBeats = publishedBeat_.load(std::memory_order_relaxed);
    out.sampleRate = publishedSampleRate_.load(std::memory_order_relaxed);
    out.bpm = publishedBpm_.load(std::memory_order_relaxed);
    out.playing = publishedPlaying_.load(std::memory_order_relaxed);
    out.metronome = publishedMetronome_.load(std::memory_order_relaxed);
    out.testTone = publishedTestTone_.load(std::memory_order_relaxed);
    return out;
}

void AudioEngine::prepare(double sampleRate, std::uint32_t) {
    const double oldRate = tempo_.sampleRate();
    if (oldRate != sampleRate && oldRate > 0.0) {
        // Keep the place in the music by rescaling, and rebuild the tempo segment there.
        const double beat = tempo_.beatAtFrame(position_);
        const double bpm = tempo_.bpm();
        tempo_ = TempoMap{};
        tempo_.setSampleRate(sampleRate);
        tempo_.setTempo(0, bpm);
        position_ = tempo_.frameOfBeat(0) + std::llround(beat * sampleRate * 60.0 / bpm);
    } else {
        tempo_.setSampleRate(sampleRate);
    }
    metronome_.prepare(sampleRate);
    testTone_.prepare(sampleRate);
    testTone_.setEnabled(testToneEnabled_);
    transportGainStep_ = 1.0f / static_cast<float>(sampleRate * 0.005);
    trackGainStep_ = 1.0f / static_cast<float>(sampleRate * kTrackGainSmoothSeconds);
    transportGain_ = 1.0f;
    playing_ = false;
    stopping_ = false;
    waitingForPreload_ = false;
    pendingPreload_.store(kNoPendingPreload, std::memory_order_release);
    publish();
}

void AudioEngine::beginPreloadWait() noexcept {
    if (!preloadGate_.load(std::memory_order_relaxed) || !snapshot_.load(std::memory_order_acquire)) {
        waitingForPreload_ = false;
        pendingPreload_.store(kNoPendingPreload, std::memory_order_release);
        return;
    }
    // Forget any earlier answer: it may be for this same position, but for an older arrangement.
    preloadedFrame_.store(kNoPendingPreload, std::memory_order_release);
    waitingForPreload_ = true;
    preloadWaitLeft_ = static_cast<std::int64_t>(tempo_.sampleRate() * kPreloadWaitSeconds);
    pendingPreload_.store(position_, std::memory_order_release);
}

void AudioEngine::applyCommand(const EngineCommand& command) noexcept {
    switch (command.type) {
    case EngineCommandType::Play:
        if (!playing_ || stopping_) {
            playing_ = true;
            stopping_ = false;
            transportGain_ = 1.0f;
            beginPreloadWait();
        }
        break;
    case EngineCommandType::Stop:
        if (playing_) stopping_ = true; // Fades out over a few milliseconds in process.
        break;
    case EngineCommandType::Rewind:
        position_ = 0;
        tempo_.reset();
        metronome_.reset();
        if (playing_) beginPreloadWait();
        break;
    case EngineCommandType::Seek:
        position_ = std::max<std::int64_t>(0, static_cast<std::int64_t>(command.value));
        metronome_.reset();
        if (playing_) beginPreloadWait();
        break;
    case EngineCommandType::SetMetronome:
        metronomeEnabled_ = command.value != 0.0;
        break;
    case EngineCommandType::SetTestTone:
        testToneEnabled_ = command.value != 0.0;
        testTone_.setEnabled(testToneEnabled_);
        break;
    case EngineCommandType::SetTempo:
        tempo_.setTempo(position_, command.value);
        break;
    case EngineCommandType::SetOutputGain:
        outputGain_ = static_cast<float>(std::clamp(command.value, 0.0, 1.0));
        break;
    }
}

void AudioEngine::renderTracks(float* left, float* right, std::uint32_t frames, std::int64_t position, const ArrangementSnapshot& snapshot) noexcept {
    const std::int64_t end = position + frames;
    std::uint64_t missing = 0;
    for (const TrackRt& track : snapshot.tracks) {
        TrackState& state = trackStates_[track.slot];
        if (state.left == 0.0f && state.right == 0.0f && track.gainLeft == 0.0f && track.gainRight == 0.0f) continue; // Muted and settled.

        std::fill_n(busLeft_.data(), frames, 0.0f);
        std::fill_n(busRight_.data(), frames, 0.0f);

        // Clips are sorted by start: find the last one starting before this range ends, then
        // walk back only as far as the longest clip could possibly reach.
        const auto last = std::lower_bound(track.clips.begin(), track.clips.end(), end, [](const ClipRt& clip, std::int64_t value) { return clip.start < value; });
        for (auto it = last; it != track.clips.begin();) {
            --it;
            const ClipRt& clip = *it;
            if (clip.start + track.longestClip <= position) break;
            const std::int64_t from = std::max(position, clip.start);
            const std::int64_t to = std::min(end, clip.end);
            if (from >= to) continue;
            const auto offset = static_cast<std::uint32_t>(from - position);
            missing += clip.media->readInto(clip.sourceOffset + (from - clip.start), busLeft_.data() + offset, busRight_.data() + offset,
                                            static_cast<std::uint32_t>(to - from), clip.gain, from - clip.start, clip.end - from);
        }

        for (std::uint32_t i = 0; i < frames; ++i) {
            state.left = moveToward(state.left, track.gainLeft, trackGainStep_);
            state.right = moveToward(state.right, track.gainRight, trackGainStep_);
            left[i] += busLeft_[i] * state.left;
            right[i] += busRight_[i] * state.right;
        }
    }
    if (missing > 0) diskMisses_.fetch_add(missing, std::memory_order_relaxed);
}

void AudioEngine::renderChunk(float* left, float* right, std::uint32_t frames, std::int64_t position, const ArrangementSnapshot* snapshot) noexcept {
    if (metronomeEnabled_) metronome_.render(left, right, frames, position, tempo_);
    if (snapshot) renderTracks(left, right, frames, position, *snapshot);
}

void AudioEngine::process(const ProcessContext& context) noexcept {
    EngineCommand command;
    for (int i = 0; i < 32 && commands_.tryPop(command); ++i) applyCommand(command); // Bounded work per block.

    float* left = context.outputs[0];
    float* right = context.outputChannels > 1 ? context.outputs[1] : context.outputs[0];
    const std::uint32_t frames = context.frames;
    std::fill_n(left, frames, 0.0f);
    if (right != left) std::fill_n(right, frames, 0.0f);

    const ArrangementSnapshot* snapshot = snapshot_.load(std::memory_order_acquire);

    if (waitingForPreload_) {
        preloadWaitLeft_ -= frames;
        if (preloadedFrame_.load(std::memory_order_acquire) == position_ || preloadWaitLeft_ <= 0 || !snapshot) {
            waitingForPreload_ = false;
            pendingPreload_.store(kNoPendingPreload, std::memory_order_release);
        }
    }

    if (playing_ && !waitingForPreload_) {
        const bool loop = snapshot && snapshot->loopEnabled;
        std::uint32_t done = 0;
        while (done < frames) {
            std::uint32_t chunk = std::min(frames - done, kChunkFrames);
            // Split exactly at the loop end, so the wrap is sample-accurate.
            if (loop && position_ < snapshot->loopEnd) chunk = static_cast<std::uint32_t>(std::min<std::int64_t>(chunk, snapshot->loopEnd - position_));
            renderChunk(left + done, right + done, chunk, position_, snapshot);
            position_ += chunk;
            done += chunk;
            if (loop && position_ == snapshot->loopEnd) position_ = snapshot->loopStart;
        }
    }

    if (playing_ && stopping_) {
        for (std::uint32_t i = 0; i < frames; ++i) {
            transportGain_ = std::max(0.0f, transportGain_ - transportGainStep_);
            left[i] *= transportGain_;
            if (right != left) right[i] *= transportGain_;
        }
        if (transportGain_ <= 0.0f) {
            playing_ = false;
            stopping_ = false;
            waitingForPreload_ = false;
            pendingPreload_.store(kNoPendingPreload, std::memory_order_release);
            transportGain_ = 1.0f;
            metronome_.reset();
        }
    }

    testTone_.render(left, right, frames);

    float peak = 0.0f;
    for (std::uint32_t i = 0; i < frames; ++i) {
        left[i] *= outputGain_;
        if (right != left) right[i] *= outputGain_;
        peak = std::max({peak, std::abs(left[i]), std::abs(right[i])});
    }
    if (peak > outputPeak_.load(std::memory_order_relaxed)) outputPeak_.store(peak, std::memory_order_relaxed);

    publish();
    processCount_.fetch_add(1, std::memory_order_acq_rel);
}

void AudioEngine::publish() noexcept {
    publishedPosition_.store(position_, std::memory_order_relaxed);
    publishedBeat_.store(tempo_.beatAtFrame(position_), std::memory_order_relaxed);
    publishedSampleRate_.store(tempo_.sampleRate(), std::memory_order_relaxed);
    publishedBpm_.store(tempo_.bpm(), std::memory_order_relaxed);
    publishedPlaying_.store(playing_, std::memory_order_relaxed);
    publishedMetronome_.store(metronomeEnabled_, std::memory_order_relaxed);
    publishedTestTone_.store(testToneEnabled_, std::memory_order_relaxed);
}

} // namespace ldaw::engine
