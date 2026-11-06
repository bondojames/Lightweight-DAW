// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/panels/Panels.h"

#include "BuildInfo.h"
#include "engine/device/AudioStatus.h"
#include "ui/FrameScheduler.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace ldaw::ui {
namespace {

constexpr engine::AudioStatus kAudioStatus = engine::AudioStatus::NotInitialized; // Until Phase 01.

double toMiB(std::uint64_t bytes) { return static_cast<double>(bytes) / 1048576.0; }

const char* schedulerModeName(int mode) {
    switch (static_cast<FrameScheduler::Mode>(mode)) {
    case FrameScheduler::Mode::Active: return "active";
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
        if (ImGui::MenuItem("Test Open dialog...")) actions.dialog = DialogRequest::Open;
        if (ImGui::MenuItem("Test Save dialog...")) actions.dialog = DialogRequest::Save;
        ImGui::Separator();
        if (ImGui::MenuItem("Preferences...")) state.showPreferences = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) actions.exit = true;
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

void drawTransportBar(const UiState& state) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
    const float height = ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y;
    if (ImGui::BeginViewportSideBar("##transport", viewport, ImGuiDir_Up, height, flags)) {
        if (ImGui::BeginMenuBar()) {
            // Disabled, not faked: pressing Play must make real sound or do nothing.
            ImGui::BeginDisabled();
            ImGui::SmallButton("Play");
            ImGui::SmallButton("Stop");
            ImGui::SmallButton("Record");
            ImGui::EndDisabled();
            ImGui::SetItemTooltip("Transport arrives with the audio engine in Phase 01.");
            ImGui::Separator();
            ImGui::Text("Audio: %s", engine::audioStatusText(kAudioStatus));
            ImGui::Separator();
            ImGui::Text("Audio load: n/a");
            ImGui::Separator();
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
    ImGui::Text("Audio: %s", engine::audioStatusText(kAudioStatus));
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

        if (ImGui::Begin(def.windowName, &open)) {
            switch (def.id) {
            case PanelId::Library: drawNotImplemented("The file library", 3); break;
            case PanelId::Timeline: drawNotImplemented("The arrangement timeline", 2); break;
            case PanelId::Session: drawNotImplemented("The live clip grid", 10); break;
            case PanelId::Inspector: drawNotImplemented("The inspector", 2); break;
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
        ImGui::TextWrapped("Free software under the GNU General Public License, version 3 or later. See LICENSE beside the program.");
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
        ImGui::Text("Device: %s", engine::audioStatusText(kAudioStatus));
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
    drawMainMenu(workspaces, state, actions);
    drawTransportBar(state);

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
