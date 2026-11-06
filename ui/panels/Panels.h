// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "platform/win32/ProcessMetrics.h"
#include "ui/WorkspaceManager.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ldaw::ui {

enum class DialogRequest { None, Open, Save };

// What the UI asked for this frame. The UI never performs these itself.
struct UiActions {
    bool exit = false;
    bool preferencesChanged = false;
    bool simulateDeviceLoss = false;
    DialogRequest dialog = DialogRequest::None;
};

// A read-only view of app state, plus the UI's own harmless view state.
struct UiState {
    platform::ProcessSample process{};
    double fps = 0.0, buildMs = 0.0, renderMs = 0.0, presentMs = 0.0;
    std::uint64_t framesTotal = 0;
    int drawCalls = 0, vertices = 0, viewportCount = 1, monitorCount = 0;
    int schedulerMode = 0;
    int deviceRecoveries = 0;
    float dpiScale = 1.0f;
    std::string adapterName, featureLevel, dataDir, lastDialogResult;
    bool warp = false;
    std::vector<std::string> droppedFiles;
    Workspace currentWorkspace = Workspace::Production;

    int themeId = 0, activeHz = 60, idleHz = 10;
    bool showAbout = false, showPreferences = false;
    bool detachDiagnosticsOnce = false;

    static constexpr std::size_t kHistory = 120;
    std::array<float, kHistory> cpuHistory{};
    std::size_t cpuHistoryNext = 0;
    void pushCpuHistory(float value) {
        cpuHistory[cpuHistoryNext] = value;
        cpuHistoryNext = (cpuHistoryNext + 1) % kHistory;
    }
};

// Draws the whole interface for one frame.
UiActions drawInterface(WorkspaceManager& workspaces, UiState& state);

} // namespace ldaw::ui
