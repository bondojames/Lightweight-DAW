// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/panels/Panels.h"

#include "BuildInfo.h"
#include "engine/device/AudioStatus.h"
#include "ui/FrameScheduler.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <cmath>

namespace ldaw::ui {
namespace {

using engine::EngineCommandType;

bool audioRunning(const UiState& state) { return state.audio.status == engine::AudioStatus::Running; }

double toMiB(std::uint64_t bytes) { return static_cast<double>(bytes) / 1048576.0; }

const char* schedulerModeName(int mode) {
    switch (static_cast<FrameScheduler::Mode>(mode)) {
    case FrameScheduler::Mode::Active: return "active";
    case FrameScheduler::Mode::Animating: return "playing";
    case FrameScheduler::Mode::Idle: return "idle";
    case FrameScheduler::Mode::Occluded: return "occluded";
    case FrameScheduler::Mode::Hidden: return "hidden";
    }
    return "?";
}

void drawMainMenu(WorkspaceManager& workspaces, UiState& state, UiActions& actions) {
    if (!ImGui::BeginMainMenuBar()) return;
    if (ImGui::BeginMenu("File")) {
        // Projects arrive in Phase 03. These exercise the native dialogs only and say so.
        if (ImGui::MenuItem("Import audio...")) actions.dialog = DialogRequest::ImportAudio;
        ImGui::Separator();
        if (ImGui::MenuItem("Test Open dialog...")) actions.dialog = DialogRequest::Open;
        if (ImGui::MenuItem("Test Save dialog...")) actions.dialog = DialogRequest::Save;
        ImGui::Separator();
        if (ImGui::MenuItem("Preferences...")) state.showPreferences = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) actions.exit = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        commands::ProjectHistory& history = *state.history;
        const std::string undoLabel = history.canUndo() ? "Undo " + history.undoName() : "Undo";
        const std::string redoLabel = history.canRedo() ? "Redo " + history.redoName() : "Redo";
        if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, history.canUndo())) history.undo();
        if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, history.canRedo())) history.redo();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        for (const PanelDef& def : panelDefs()) {
            const char* label = def.windowName;
            const char* end = ImGui::FindRenderedTextEnd(label);
            const std::string title(label, end);
            ImGui::MenuItem(title.c_str(), nullptr, &workspaces.panelOpen(def.id));
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Audio")) {
        if (ImGui::MenuItem("Audio settings...")) state.showPreferences = true;
        if (ImGui::MenuItem("Reconnect audio device")) actions.audioConfigChanged = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Run offline metronome test...", nullptr, false, !state.audio.offlineTestRunning)) actions.dialog = DialogRequest::OfflineTestWav;
        ImGui::SetItemTooltip("Renders 16 seconds with no audio device, saves a WAV, and measures the beat timing.");
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Workspace")) {
        if (ImGui::MenuItem("Production", nullptr, workspaces.current() == Workspace::Production)) workspaces.requestSwitch(Workspace::Production);
        if (ImGui::MenuItem("Live", nullptr, workspaces.current() == Workspace::Live)) workspaces.requestSwitch(Workspace::Live);
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Workspace")) workspaces.requestReset();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About LightweightDAW...")) state.showAbout = true;
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

void togglePlayback(const UiState& state, UiActions& actions) {
    actions.commands.push_back({state.engine.playing ? EngineCommandType::Stop : EngineCommandType::Play, 0.0});
}

void drawTransportBar(UiState& state, UiActions& actions) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const bool running = audioRunning(state);

    // Space plays and stops, unless the user is typing or moving by keyboard.
    const ImGuiIO& io = ImGui::GetIO();
    if (running && !io.WantTextInput && !io.NavVisible && ImGui::Shortcut(ImGuiKey_Space, ImGuiInputFlags_RouteGlobal)) {
        togglePlayback(state, actions);
    }

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
    const float height = ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y;
    if (ImGui::BeginViewportSideBar("##transport", viewport, ImGuiDir_Up, height, flags)) {
        if (ImGui::BeginMenuBar()) {
            ImGui::BeginDisabled(!running);
            if (ImGui::SmallButton(state.engine.playing ? "Playing" : "Play")) actions.commands.push_back({EngineCommandType::Play, 0.0});
            if (ImGui::SmallButton("Stop")) actions.commands.push_back({EngineCommandType::Stop, 0.0});
            if (ImGui::SmallButton("Rewind")) actions.commands.push_back({EngineCommandType::Rewind, 0.0});
            ImGui::EndDisabled();
            // Disabled, not faked: Record must capture real audio or do nothing.
            ImGui::BeginDisabled();
            ImGui::SmallButton("Record");
            ImGui::EndDisabled();
            ImGui::SetItemTooltip("Recording arrives in Phase 04.");
            ImGui::Separator();

            const auto beat = static_cast<std::int64_t>(std::floor(state.engine.positionBeats));
            const double seconds = state.engine.sampleRate > 0.0 ? static_cast<double>(state.engine.positionFrames) / state.engine.sampleRate : 0.0;
            ImGui::Text("%3lld.%d   %02d:%06.3f", static_cast<long long>(beat / 4 + 1), static_cast<int>(beat % 4) + 1,
                        static_cast<int>(seconds / 60.0), std::fmod(seconds, 60.0));
            ImGui::Separator();

            ImGui::BeginDisabled(!running);
            bool metronome = state.engine.metronome;
            if (ImGui::Checkbox("Metronome", &metronome)) actions.commands.push_back({EngineCommandType::SetMetronome, metronome ? 1.0 : 0.0});
            bool tone = state.engine.testTone;
            if (ImGui::Checkbox("Test tone", &tone)) actions.commands.push_back({EngineCommandType::SetTestTone, tone ? 1.0 : 0.0});
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 4.0f);
            if (!ImGui::IsAnyItemActive() || !ImGui::IsWindowFocused()) state.bpmInput = state.engine.bpm > 0.0 ? state.engine.bpm : state.bpmInput;
            ImGui::InputDouble("BPM", &state.bpmInput, 0.0, 0.0, "%.1f");
            if (ImGui::IsItemDeactivatedAfterEdit()) actions.commands.push_back({EngineCommandType::SetTempo, state.bpmInput});
            ImGui::TextUnformatted("4/4");
            ImGui::EndDisabled();
            ImGui::Separator();

            ImGui::Text("Audio: %s", engine::audioStatusText(state.audio.status));
            if (!state.audio.message.empty()) ImGui::SetItemTooltip("%s", state.audio.message.c_str());
            ImGui::Separator();
            if (running) ImGui::Text("Audio load: %.1f%% (p99)", state.loadP99);
            else ImGui::TextUnformatted("Audio load: n/a");
            ImGui::Separator();
            if (state.diskMissFrames > 0) {
                ImGui::TextUnformatted("DISK TOO SLOW");
                ImGui::SetItemTooltip("Audio couldn't be read from disk in time, so part of it played as silence.");
                ImGui::Separator();
            }
            if (!state.importMessage.empty()) {
                ImGui::TextUnformatted("Import failed");
                ImGui::SetItemTooltip("%s", state.importMessage.c_str());
                ImGui::Separator();
            }
            if (state.xrunsEngine + state.xrunsDevice + state.xrunsUnknown > 0) {
                ImGui::Text("Dropouts: %llu", static_cast<unsigned long long>(state.xrunsEngine + state.xrunsDevice + state.xrunsUnknown));
                ImGui::Separator();
            }
            ImGui::Text("Process CPU: %.1f%% of system", state.process.processCpuOfSystem);
            ImGui::Separator();
            ImGui::Text("RAM: %.0f MiB", toMiB(state.process.workingSetBytes));
            if (state.warp) {
                ImGui::Separator();
                ImGui::TextUnformatted("SOFTWARE RENDERING (diagnostic)");
            }
            ImGui::EndMenuBar();
        }
    }
    ImGui::End();
}

void drawNotImplemented(const char* what, int phase) {
    ImGui::TextWrapped("%s arrives in Phase %02d. Nothing here is simulated.", what, phase);
}

void drawDiagnostics(UiState& state, UiActions& actions) {
    ImGui::Text("Audio: %s", engine::audioStatusText(state.audio.status));
    if (!state.audio.message.empty()) ImGui::TextWrapped("%s", state.audio.message.c_str());
    if (audioRunning(state) && ImGui::BeginTable("##audio", 2, ImGuiTableFlags_SizingStretchProp)) {
        const engine::AudioStreamInfo& stream = state.audio.stream;
        const auto row = [](const char* label, const char* format, auto... values) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();
            ImGui::Text(format, values...);
        };
        row("Device", "%s (%s)", stream.deviceName.c_str(), stream.backendName.c_str());
        row("Stream", "%.0f Hz, %u frames (%.2f ms), %u channels, %s%s", stream.sampleRate, stream.periodFrames,
            1000.0 * stream.periodFrames / stream.sampleRate, stream.channels, stream.deviceFormat.c_str(),
            stream.systemResampling ? ", Windows resampling" : "");
        row("Priority", "%s", stream.realtimePriority ? "real-time (Pro Audio)" : "normal: real-time priority was refused");
        row("Audio load", "p50 %.1f%%, p95 %.1f%%, p99 %.1f%%, peak %.1f%% of deadline", state.loadP50, state.loadP95, state.loadP99, state.loadPeak);
        row("Dropouts", "%llu engine, %llu device, %llu unknown, over %llu callbacks", static_cast<unsigned long long>(state.xrunsEngine),
            static_cast<unsigned long long>(state.xrunsDevice), static_cast<unsigned long long>(state.xrunsUnknown), static_cast<unsigned long long>(state.callbacks));
        row("Output peak", "%.3f", state.outputPeak);
        row("Disk streaming", "%llu blocks loaded, %llu frames missed", static_cast<unsigned long long>(state.streamedBlocks), static_cast<unsigned long long>(state.diskMissFrames));
        ImGui::EndTable();
    }
    for (const engine::XrunEvent& event : state.recentXruns) {
        ImGui::BulletText("Dropout at %.3f s: %s, load %.0f%%", event.streamSeconds, engine::xrunCauseText(event.cause), event.loadPercent);
    }
    if (state.audio.offlineTestRunning) ImGui::TextUnformatted("Offline test running...");
    else if (!state.audio.offlineTestResult.empty()) ImGui::TextWrapped("Offline test: %s", state.audio.offlineTestResult.c_str());
    ImGui::Separator();

    if (ImGui::BeginTable("##diag", 2, ImGuiTableFlags_SizingStretchProp)) {
        const auto row = [](const char* label, const char* format, auto... values) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();
            ImGui::Text(format, values...);
        };
        row("Process CPU", "%.2f%% of one core, %.2f%% of system (%u logical cores)", state.process.processCpuOfOneCore,
            state.process.processCpuOfSystem, state.process.logicalCores);
        row("UI thread CPU", "%.2f%% of one core", state.process.uiThreadCpuOfOneCore);
        row("Memory", "%.1f MiB working set, %.1f MiB peak, %.1f MiB private", toMiB(state.process.workingSetBytes),
            toMiB(state.process.peakWorkingSetBytes), toMiB(state.process.privateBytes));
        row("UI frames", "%.1f per second (%s), %llu total", state.fps, schedulerModeName(state.schedulerMode),
            static_cast<unsigned long long>(state.framesTotal));
        row("Frame time", "build %.2f ms, render %.2f ms, present %.2f ms incl. vsync wait", state.buildMs, state.renderMs, state.presentMs);
        row("Draw data", "%d draw calls, %d vertices", state.drawCalls, state.vertices);
        row("GPU", "%s, D3D feature level %s%s", state.adapterName.c_str(), state.featureLevel.c_str(), state.warp ? " (WARP)" : "");
        row("Windows", "%d viewports, %d monitors, main DPI scale %.2f", state.viewportCount, state.monitorCount, state.dpiScale);
        row("Data folder", "%s", state.dataDir.c_str());
        row("GPU recoveries", "%d", state.deviceRecoveries);
        ImGui::EndTable();
    }

    char overlay[32];
    ImFormatString(overlay, sizeof(overlay), "%.1f%% of one core", state.process.processCpuOfOneCore);
    ImGui::PlotLines("##cpu", state.cpuHistory.data(), static_cast<int>(state.cpuHistory.size()),
                     static_cast<int>(state.cpuHistoryNext), overlay, 0.0f, 100.0f, ImVec2(-1.0f, ImGui::GetFrameHeight() * 2.0f));

    ImGui::SeparatorText("Shell tests");
    if (ImGui::Button("Simulate GPU device reset")) actions.simulateDeviceLoss = true;
    ImGui::SetItemTooltip("Runs the real recovery path by discarding and rebuilding the graphics device. It does not crash a GPU driver.");
    ImGui::TextWrapped("Last file dialog: %s", state.lastDialogResult.empty() ? "(none)" : state.lastDialogResult.c_str());
    ImGui::TextUnformatted("Files dropped on the main window:");
    if (state.droppedFiles.empty()) ImGui::TextDisabled("(none)");
    for (const std::string& file : state.droppedFiles) ImGui::BulletText("%s", file.c_str());
}

void drawPanels(WorkspaceManager& workspaces, UiState& state, UiActions& actions) {
    for (const PanelDef& def : panelDefs()) {
        bool& open = workspaces.panelOpen(def.id);
        if (!open) continue;

        if (def.id == PanelId::Diagnostics && state.detachDiagnosticsOnce) {
            state.detachDiagnosticsOnce = false; // Test hook: forces a real second OS window.
            const ImGuiViewport* main = ImGui::GetMainViewport();
            ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
            ImGui::SetNextWindowPos(ImVec2(main->Pos.x + main->Size.x + 8.0f, main->Pos.y), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(400.0f, 400.0f), ImGuiCond_Always);
        }

        // The timeline uses the wheel for zooming and scrolling, so the window mustn't.
        const ImGuiWindowFlags flags = def.id == PanelId::Timeline ? (ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse) : ImGuiWindowFlags_None;
        if (ImGui::Begin(def.windowName, &open, flags)) {
            TimelineContext context{*state.history, state.audioService->media(), state.engine, audioRunning(state),
                                    [&actions](const engine::EngineCommand& command) { actions.commands.push_back(command); }};
            switch (def.id) {
            case PanelId::Library: drawNotImplemented("The file library", 3); break;
            case PanelId::Timeline: drawTimelinePanel(state.timeline, context); break;
            case PanelId::Session: drawNotImplemented("The live clip grid", 10); break;
            case PanelId::Inspector: drawInspectorPanel(state.timeline, context); break;
            case PanelId::PianoRoll: drawNotImplemented("The piano roll", 5); break;
            case PanelId::Mixer: drawNotImplemented("The mixer", 7); break;
            case PanelId::Diagnostics: drawDiagnostics(state, actions); break;
            case PanelId::Count: break;
            }
        }
        ImGui::End();
    }
}

void drawAbout(UiState& state) {
    if (!state.showAbout) return;
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetFontSize() * 30.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::Begin("About LightweightDAW###window.about", &state.showAbout, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::Text("LightweightDAW %s (working title)", LDAW_VERSION);
        ImGui::Text("Build %s", LDAW_COMMIT);
        ImGui::Spacing();
        ImGui::TextWrapped("Free software under the GNU General Public License, version 3 or later, with a permission for loading non-GPL plug-ins and drivers. See LICENSE and LICENSE-EXCEPTION.md beside the program.");
        ImGui::TextWrapped("Uses Dear ImGui %s (MIT). See THIRD_PARTY_NOTICES.md beside the program.", ImGui::GetVersion());
        ImGui::Spacing();
        ImGui::TextWrapped("No account, cloud service or telemetry.");
    }
    ImGui::End();
}

void drawPreferences(UiState& state, UiActions& actions) {
    if (!state.showPreferences) return;
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetFontSize() * 26.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::Begin("Preferences###window.preferences", &state.showPreferences, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::SeparatorText("Appearance");
        if (ImGui::RadioButton("Dark", &state.themeId, 0)) actions.preferencesChanged = true;
        ImGui::SameLine();
        if (ImGui::RadioButton("Light", &state.themeId, 1)) actions.preferencesChanged = true;
        ImGui::SameLine();
        if (ImGui::RadioButton("High contrast", &state.themeId, 2)) actions.preferencesChanged = true;

        ImGui::SeparatorText("UI refresh");
        ImGui::TextUnformatted("While interacting:");
        ImGui::SameLine();
        if (ImGui::RadioButton("30 Hz", &state.activeHz, 30)) actions.preferencesChanged = true;
        ImGui::SameLine();
        if (ImGui::RadioButton("60 Hz", &state.activeHz, 60)) actions.preferencesChanged = true;
        ImGui::TextUnformatted("While idle:");
        ImGui::SameLine();
        if (ImGui::RadioButton("5 Hz", &state.idleHz, 5)) actions.preferencesChanged = true;
        ImGui::SameLine();
        if (ImGui::RadioButton("10 Hz", &state.idleHz, 10)) actions.preferencesChanged = true;
        ImGui::SameLine();
        if (ImGui::RadioButton("15 Hz", &state.idleHz, 15)) actions.preferencesChanged = true;

        ImGui::SeparatorText("Audio");
        std::string currentName = "Windows default";
        for (const engine::AudioDeviceInfo& device : state.audio.devices) {
            if (device.id == state.audioDeviceId) currentName = device.name;
        }
        if (ImGui::BeginCombo("Output device", currentName.c_str())) {
            if (ImGui::Selectable("Windows default", state.audioDeviceId.empty())) {
                state.audioDeviceId.clear();
                actions.audioConfigChanged = true;
            }
            for (const engine::AudioDeviceInfo& device : state.audio.devices) {
                ImGui::PushID(device.id.c_str());
                if (ImGui::Selectable(device.name.c_str(), device.id == state.audioDeviceId)) {
                    state.audioDeviceId = device.id;
                    actions.audioConfigChanged = true;
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        static constexpr int kRates[] = {0, 44100, 48000, 96000};
        const auto rateLabel = [](int rate) {
            return rate == 0 ? std::string("Device setting") : std::to_string(rate) + " Hz";
        };
        if (ImGui::BeginCombo("Sample rate", rateLabel(state.audioRate).c_str())) {
            for (const int rate : kRates) {
                if (ImGui::Selectable(rateLabel(rate).c_str(), rate == state.audioRate)) {
                    state.audioRate = rate;
                    actions.audioConfigChanged = true;
                }
            }
            ImGui::EndCombo();
        }
        static constexpr int kSizes[] = {128, 256, 512, 1024};
        if (ImGui::BeginCombo("Buffer size", std::to_string(state.audioFrames).c_str())) {
            for (const int size : kSizes) {
                if (ImGui::Selectable(std::to_string(size).c_str(), size == state.audioFrames)) {
                    state.audioFrames = size;
                    actions.audioConfigChanged = true;
                }
            }
            ImGui::EndCombo();
        }
        if (audioRunning(state)) {
            const engine::AudioStreamInfo& stream = state.audio.stream;
            ImGui::Text("Running at %.0f Hz with %u frames (%.2f ms).", stream.sampleRate, stream.periodFrames, 1000.0 * stream.periodFrames / stream.sampleRate);
            if (static_cast<int>(stream.periodFrames) != state.audioFrames) {
                ImGui::TextWrapped("This device's driver does not offer %d frames in shared mode, so the closest available size is used.", state.audioFrames);
            }
        } else {
            ImGui::Text("Status: %s", engine::audioStatusText(state.audio.status));
            if (!state.audio.message.empty()) ImGui::TextWrapped("%s", state.audio.message.c_str());
        }
    }
    ImGui::End();
}

void drawNotice(WorkspaceManager& workspaces) {
    if (workspaces.notice().empty()) return;
    ImGui::OpenPopup("Workspace layout reset");
    if (ImGui::BeginPopupModal("Workspace layout reset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(workspaces.notice().c_str());
        if (ImGui::Button("OK")) {
            workspaces.clearNotice();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace

UiActions drawInterface(WorkspaceManager& workspaces, UiState& state) {
    UiActions actions;

    // Undo and redo work anywhere, except while typing.
    if (!ImGui::GetIO().WantTextInput) {
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal)) state.history->undo();
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteGlobal) || ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal)) state.history->redo();
    }

    drawMainMenu(workspaces, state, actions);
    drawTransportBar(state, actions);

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImGuiID dockspaceId = ImGui::GetID("ldaw.root.dockspace");
    workspaces.buildFactoryLayoutIfNeeded(dockspaceId, viewport->WorkSize.x, viewport->WorkSize.y);
    ImGui::DockSpaceOverViewport(dockspaceId, viewport);

    drawPanels(workspaces, state, actions);
    workspaces.rescueWindowsIfNeeded();
    drawAbout(state);
    drawPreferences(state, actions);
    drawNotice(workspaces);
    return actions;
}

} // namespace ldaw::ui
