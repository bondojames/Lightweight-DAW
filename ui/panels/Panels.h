// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "engine/AudioEngine.h"
#include "engine/device/AudioService.h"
#include "engine/telemetry/AudioTelemetry.h"
#include "platform/win32/ProcessMetrics.h"
#include "ui/WorkspaceManager.h"
#include "ui/arrangement/TimelinePanel.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ldaw::ui {

enum class DialogRequest { None, Open, Save, OfflineTestWav, ImportAudio };

// What the UI asked for this frame. The UI never performs these itself.
struct UiActions {
    bool exit = false;
    bool preferencesChanged = false;
    bool simulateDeviceLoss = false;
    bool audioConfigChanged = false;
    DialogRequest dialog = DialogRequest::None;
    std::vector<engine::EngineCommand> commands;
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

    // Audio, read from telemetry and never from inside the audio thread's data.
    commands::ProjectHistory* history = nullptr;
    engine::AudioService* audioService = nullptr;
    TimelineState timeline;
    std::string importMessage;
    std::uint64_t diskMissFrames = 0;
    std::uint64_t streamedBlocks = 0;
    engine::AudioServiceState audio;
    engine::EngineState engine;
    double loadP50 = 0.0, loadP95 = 0.0, loadP99 = 0.0;
    float loadPeak = 0.0f, outputPeak = 0.0f;
    std::uint64_t callbacks = 0, xrunsEngine = 0, xrunsDevice = 0, xrunsUnknown = 0;
    std::vector<engine::XrunEvent> recentXruns;

    // The chosen audio settings; what the driver really gave is in audio.stream.
    std::string audioDeviceId;
    int audioRate = 0;
    int audioFrames = 256;
    double bpmInput = 120.0;

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
