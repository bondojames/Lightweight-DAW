// SPDX-License-Identifier: GPL-3.0-or-later
#include "engine/AudioEngine.h"

#include <algorithm>
#include <cmath>

namespace ldaw::engine {

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
    transportGain_ = 1.0f;
    playing_ = false;
    stopping_ = false;
    publish();
}

void AudioEngine::applyCommand(const EngineCommand& command) noexcept {
    switch (command.type) {
    case EngineCommandType::Play:
        playing_ = true;
        stopping_ = false;
        transportGain_ = 1.0f;
        break;
    case EngineCommandType::Stop:
        if (playing_) stopping_ = true; // Fades out over a few milliseconds in process.
        break;
    case EngineCommandType::Rewind:
        position_ = 0;
        tempo_.reset();
        metronome_.reset();
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

void AudioEngine::process(const ProcessContext& context) noexcept {
    EngineCommand command;
    for (int i = 0; i < 32 && commands_.tryPop(command); ++i) applyCommand(command); // Bounded work per block.

    float* left = context.outputs[0];
    float* right = context.outputChannels > 1 ? context.outputs[1] : context.outputs[0];
    const std::uint32_t frames = context.frames;
    std::fill_n(left, frames, 0.0f);
    if (right != left) std::fill_n(right, frames, 0.0f);

    if (playing_) {
        if (metronomeEnabled_) metronome_.render(left, right, frames, position_, tempo_);
        position_ += frames;
        if (stopping_) {
            for (std::uint32_t i = 0; i < frames; ++i) {
                transportGain_ = std::max(0.0f, transportGain_ - transportGainStep_);
                left[i] *= transportGain_;
                if (right != left) right[i] *= transportGain_;
            }
            if (transportGain_ <= 0.0f) {
                playing_ = false;
                stopping_ = false;
                transportGain_ = 1.0f;
                metronome_.reset();
            }
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
