// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "engine/runtime/SpscQueue.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace ldaw::engine {

enum class XrunCause { Engine, Device, ExternalPlugin, Unknown, Count };

constexpr const char* xrunCauseText(XrunCause cause) {
    switch (cause) {
    case XrunCause::Engine: return "engine";
    case XrunCause::Device: return "device";
    case XrunCause::ExternalPlugin: return "external plug-in";
    case XrunCause::Unknown: return "unknown";
    case XrunCause::Count: break;
    }
    return "?";
}

struct XrunEvent {
    double streamSeconds = 0.0;
    XrunCause cause = XrunCause::Unknown;
    float loadPercent = 0.0f;
};

// Decides whether a callback missed its deadline. Pure logic, so it can be tested
// without a device. WASAPI has no underrun flag for a render stream, and an empty
// buffer at wake-up is normal in event-driven shared mode, so timing is the evidence.
class XrunDetector {
public:
    // Our own processing took longer than the audio it produced.
    bool checkCompute(double computeSeconds, double deadlineSeconds) {
        lastWasEngineMiss_ = computeSeconds > deadlineSeconds;
        return lastWasEngineMiss_;
    }

    // We woke far later than the device period, with nothing left in the buffer. If
    // our previous callback overran, that overrun already explains it.
    bool checkWake(double wakeGapSeconds, double periodSeconds, std::uint32_t paddingFrames) {
        const bool late = wakeGapSeconds > periodSeconds * kLateFactor && paddingFrames == 0;
        const bool result = late && !lastWasEngineMiss_;
        lastWasEngineMiss_ = false;
        return result;
    }

private:
    static constexpr double kLateFactor = 1.5;
    bool lastWasEngineMiss_ = false;
};

// Written by the audio thread with relaxed atomics, and read from any other thread.
class AudioTelemetry {
public:
    static constexpr std::size_t kBins = 401; // 0% to 200% in 0.5% steps; the last bin holds the rest.

    void recordCallback(double computeSeconds, double deadlineSeconds, std::uint32_t frames) noexcept {
        const double load = deadlineSeconds > 0.0 ? 100.0 * computeSeconds / deadlineSeconds : 0.0;
        auto bin = static_cast<std::size_t>(load * 2.0);
        if (bin >= kBins) bin = kBins - 1;
        bins_[bin].fetch_add(1, std::memory_order_relaxed);
        callbacks_.fetch_add(1, std::memory_order_relaxed);
        frames_.fetch_add(frames, std::memory_order_relaxed);
        lastLoad_.store(static_cast<float>(load), std::memory_order_relaxed);
        const auto current = static_cast<float>(load);
        if (current > peakLoad_.load(std::memory_order_relaxed)) peakLoad_.store(current, std::memory_order_relaxed);
    }

    void recordXrun(XrunCause cause, double streamSeconds, float loadPercent) noexcept {
        counts_[static_cast<std::size_t>(cause)].fetch_add(1, std::memory_order_relaxed);
        if (!events_.tryPush({streamSeconds, cause, loadPercent})) eventsDropped_.fetch_add(1, std::memory_order_relaxed);
    }

    void reset() noexcept {
        for (auto& bin : bins_) bin.store(0, std::memory_order_relaxed);
        for (auto& count : counts_) count.store(0, std::memory_order_relaxed);
        callbacks_.store(0, std::memory_order_relaxed);
        frames_.store(0, std::memory_order_relaxed);
        lastLoad_.store(0.0f, std::memory_order_relaxed);
        peakLoad_.store(0.0f, std::memory_order_relaxed);
        XrunEvent drop;
        while (events_.tryPop(drop)) {}
    }

    std::uint64_t callbacks() const { return callbacks_.load(std::memory_order_relaxed); }
    std::uint64_t frames() const { return frames_.load(std::memory_order_relaxed); }
    float lastLoad() const { return lastLoad_.load(std::memory_order_relaxed); }
    float peakLoad() const { return peakLoad_.load(std::memory_order_relaxed); }
    std::uint64_t xruns(XrunCause cause) const { return counts_[static_cast<std::size_t>(cause)].load(std::memory_order_relaxed); }
    std::uint64_t totalXruns() const {
        std::uint64_t total = 0;
        for (const auto& count : counts_) total += count.load(std::memory_order_relaxed);
        return total;
    }

    bool popXrun(XrunEvent& out) { return events_.tryPop(out); }

    // The load (percent of deadline) at or below which this fraction of callbacks fell.
    double loadPercentile(double fraction) const {
        std::uint64_t total = 0;
        std::array<std::uint64_t, kBins> copy{};
        for (std::size_t i = 0; i < kBins; ++i) {
            copy[i] = bins_[i].load(std::memory_order_relaxed);
            total += copy[i];
        }
        if (total == 0) return 0.0;
        const auto needed = static_cast<std::uint64_t>(fraction * static_cast<double>(total) + 0.5);
        std::uint64_t seen = 0;
        for (std::size_t i = 0; i < kBins; ++i) {
            seen += copy[i];
            if (seen >= needed && seen > 0) return static_cast<double>(i + 1) * 0.5; // Upper edge of the bin.
        }
        return 200.0;
    }

private:
    std::array<std::atomic<std::uint64_t>, kBins> bins_{};
    std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(XrunCause::Count)> counts_{};
    std::atomic<std::uint64_t> callbacks_{0};
    std::atomic<std::uint64_t> frames_{0};
    std::atomic<float> lastLoad_{0.0f};
    std::atomic<float> peakLoad_{0.0f};
    std::atomic<std::uint64_t> eventsDropped_{0};
    SpscQueue<XrunEvent, 64> events_;
};

} // namespace ldaw::engine
