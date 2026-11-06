// SPDX-License-Identifier: GPL-3.0-or-later
// Confirms the Phase 00 interfaces compile with no GUI or driver dependency.
#include "core/diagnostics/CpuMath.h"
#include "engine/device/IAudioBackend.h"
#include "engine/midi/IMidiBackend.h"
#include "plugins/api/IPluginHostAdapter.h"
#include "tests/unit/test_framework.h"

#include <string_view>
#include <type_traits>

using namespace ldaw;

TEST_CASE(audio_status_reports_not_initialized) {
    CHECK(std::string_view(engine::audioStatusText(engine::AudioStatus::NotInitialized)) == "not initialized");
}

TEST_CASE(interfaces_are_abstract) {
    CHECK(std::is_abstract_v<engine::IAudioBackend>);
    CHECK(std::is_abstract_v<engine::IAudioProcessor>);
    CHECK(std::is_abstract_v<engine::IMidiBackend>);
    CHECK(std::is_abstract_v<plugins::IPluginHostAdapter>);
}

TEST_CASE(cpu_percent_denominators_differ_by_core_count) {
    CHECK(cpuPercentOfOneLogicalCore(5, 10) == 50.0);
    CHECK(cpuPercentOfSystem(8, 10, 16) == 5.0);
    CHECK(cpuPercentOfOneLogicalCore(5, 0) == 0.0);
    CHECK(cpuPercentOfSystem(5, 10, 0) == 0.0);
}
