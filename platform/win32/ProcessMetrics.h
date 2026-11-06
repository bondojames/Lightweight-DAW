// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace ldaw::platform {

// Real numbers from the OS. Windows updates CPU times in ticks of about 15.6 ms, so
// a light app measured over one second often reads exactly zero. sample() therefore
// averages over the last few calls.
struct ProcessSample {
    double processCpuOfOneCore = 0.0;
    double processCpuOfSystem = 0.0;
    double uiThreadCpuOfOneCore = 0.0;
    std::uint64_t workingSetBytes = 0;
    std::uint64_t peakWorkingSetBytes = 0;
    std::uint64_t privateBytes = 0;
    unsigned logicalCores = 0;
};

class ProcessMetrics {
public:
    // Construct on the UI thread: that is the thread reported as the UI thread.
    // windowSamples = 1 measures everything since the previous sample() or construction.
    explicit ProcessMetrics(std::size_t windowSamples = 8);
    ProcessSample sample();

private:
    struct Point {
        std::uint64_t wall = 0, process = 0, thread = 0;
    };
    static constexpr std::size_t kMaxWindow = 16;

    std::array<Point, kMaxWindow> points_{};
    std::size_t window_ = 8;
    std::size_t next_ = 0;
    std::size_t count_ = 0;
    unsigned logicalCores_ = 1;
};

} // namespace ldaw::platform
