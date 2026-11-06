// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <psapi.h>

#include "platform/win32/ProcessMetrics.h"

#include "core/diagnostics/CpuMath.h"

#include <algorithm>

namespace ldaw::platform {
namespace {

std::uint64_t toUint64(const FILETIME& t) {
    return (std::uint64_t{t.dwHighDateTime} << 32) | t.dwLowDateTime;
}

std::uint64_t wallClock100ns() {
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    return toUint64(now);
}

std::uint64_t processCpu100ns() {
    FILETIME create, exit, kernel, user;
    GetProcessTimes(GetCurrentProcess(), &create, &exit, &kernel, &user);
    return toUint64(kernel) + toUint64(user);
}

std::uint64_t threadCpu100ns() {
    FILETIME create, exit, kernel, user;
    GetThreadTimes(GetCurrentThread(), &create, &exit, &kernel, &user);
    return toUint64(kernel) + toUint64(user);
}

} // namespace

ProcessMetrics::ProcessMetrics(std::size_t windowSamples) {
    window_ = std::clamp<std::size_t>(windowSamples, 1, kMaxWindow);
    logicalCores_ = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    points_[0] = {wallClock100ns(), processCpu100ns(), threadCpu100ns()};
    next_ = 1 % kMaxWindow;
    count_ = 1;
}

ProcessSample ProcessMetrics::sample() {
    const Point current{wallClock100ns(), processCpu100ns(), threadCpu100ns()};
    // Oldest retained point, at most window_ samples back.
    const std::size_t back = std::min(count_, window_);
    const Point& oldest = points_[(next_ + kMaxWindow - back) % kMaxWindow];
    const std::uint64_t wallDelta = current.wall > oldest.wall ? current.wall - oldest.wall : 0;

    ProcessSample out;
    out.logicalCores = logicalCores_;
    out.processCpuOfOneCore = cpuPercentOfOneLogicalCore(current.process - oldest.process, wallDelta);
    out.processCpuOfSystem = cpuPercentOfSystem(current.process - oldest.process, wallDelta, logicalCores_);
    out.uiThreadCpuOfOneCore = cpuPercentOfOneLogicalCore(current.thread - oldest.thread, wallDelta);

    points_[next_] = current;
    next_ = (next_ + 1) % kMaxWindow;
    count_ = std::min(count_ + 1, kMaxWindow);

    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) {
        out.workingSetBytes = counters.WorkingSetSize;
        out.peakWorkingSetBytes = counters.PeakWorkingSetSize;
        out.privateBytes = counters.PrivateUsage;
    }
    return out;
}

} // namespace ldaw::platform
