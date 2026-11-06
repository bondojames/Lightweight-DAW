// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace ldaw {

// CPU time over wall time, in percent. The two denominators are kept separate
// because they differ by a factor of the core count (spec section 7).
inline double cpuPercentOfOneLogicalCore(std::uint64_t cpu100ns, std::uint64_t wall100ns) {
    return wall100ns == 0 ? 0.0 : 100.0 * static_cast<double>(cpu100ns) / static_cast<double>(wall100ns);
}
inline double cpuPercentOfSystem(std::uint64_t cpu100ns, std::uint64_t wall100ns, unsigned logicalCores) {
    return logicalCores == 0 ? 0.0 : cpuPercentOfOneLogicalCore(cpu100ns, wall100ns) / logicalCores;
}

} // namespace ldaw
