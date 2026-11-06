// SPDX-License-Identifier: GPL-3.0-or-later
// Drives the real window, ImGui context and D3D11 device through the workspace and
// recovery paths, then checks actual ImGui state. It cannot replace the physical
// tests in tests/manual (real monitors, real mouse dragging, real DPI changes).
#include <windows.h>

#include "app/Application.h"

#include "core/logging/Log.h"
#include "core/settings/KeyValueStore.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <filesystem>
#include <format>
#include <string>

namespace ldaw {
namespace {

constexpr std::uint64_t kFramesToSettle = 12;

const char* windowName(ui::PanelId id) { return ui::panelDefs()[static_cast<std::size_t>(id)].windowName; }

ImGuiWindow* findPanel(ui::PanelId id) { return ImGui::FindWindowByName(windowName(id)); }

bool isDocked(ui::PanelId id) {
    const ImGuiWindow* window = findPanel(id);
    return window && window->DockIsActive;
}

bool isFloatingInOwnWindow(ui::PanelId id) {
    const ImGuiWindow* window = findPanel(id);
    return window && !window->DockIsActive && window->Viewport && window->Viewport != ImGui::GetMainViewport();
}

int viewportCount() { return ImGui::GetPlatformIO().Viewports.Size; }

} // namespace

void Application::buildSelfTest() {
    using ui::PanelId;
    using ui::Workspace;
    const auto layouts = dataDir_ / L"layouts";

    selfTestSteps_ = {
        {"factory Production layout docks its panels", [] {},
         [this] {
             return workspaces_.current() == Workspace::Production && isDocked(PanelId::Timeline) && isDocked(PanelId::Library) &&
                    !workspaces_.panelOpen(PanelId::Session) && viewportCount() == 1;
         }},
        {"a panel detaches into a second OS window",
         [this] {
             workspaces_.panelOpen(PanelId::Diagnostics) = true;
             uiState_.detachDiagnosticsOnce = true;
         },
         [] { return isFloatingInOwnWindow(PanelId::Diagnostics) && viewportCount() == 2; }},
        {"switching to Live shows its panels", [this] { workspaces_.requestSwitch(Workspace::Live); },
         [this] {
             return workspaces_.current() == Workspace::Live && workspaces_.panelOpen(PanelId::Session) &&
                    !workspaces_.panelOpen(PanelId::Timeline) && isDocked(PanelId::Session);
         }},
        {"switching back restores the detached panel", [this] { workspaces_.requestSwitch(Workspace::Production); },
         [this] {
             return workspaces_.current() == Workspace::Production && workspaces_.panelOpen(PanelId::Timeline) &&
                    isFloatingInOwnWindow(PanelId::Diagnostics) && viewportCount() == 2;
         }},
        {"GPU reset keeps the UI and detached panel",
         [this] { simulateDeviceLoss_ = true; },
         [this] { return uiState_.deviceRecoveries == 1 && !deviceLost_ && isFloatingInOwnWindow(PanelId::Diagnostics); }},
        {"a panel stranded by a monitor change comes back",
         [this] {
             if (ImGuiWindow* window = findPanel(PanelId::Diagnostics)) ImGui::SetWindowPos(window, ImVec2(30000.0f, 30000.0f));
             workspaces_.notifyDisplayChanged(platform::WindowHost::monitorWorkAreas());
         },
         [] {
             const ImGuiWindow* window = findPanel(PanelId::Diagnostics);
             return window && isGrabbable({static_cast<int>(window->Pos.x), static_cast<int>(window->Pos.y),
                                           static_cast<int>(window->Size.x), static_cast<int>(window->Size.y)},
                                          platform::WindowHost::monitorWorkAreas());
         }},
        {"closing a detached window closes only that panel",
         [] {
             if (const ImGuiWindow* window = findPanel(PanelId::Diagnostics); window && window->Viewport) window->Viewport->PlatformRequestClose = true;
         },
         [this] { return !workspaces_.panelOpen(PanelId::Diagnostics) && viewportCount() == 1 && !exitRequested_; }},
        {"Reset Workspace restores the factory layout",
         [this] {
             workspaces_.panelOpen(PanelId::Library) = false;
             workspaces_.requestReset();
         },
         [this] {
             return workspaces_.panelOpen(PanelId::Library) && workspaces_.panelOpen(PanelId::Diagnostics) && isDocked(PanelId::Diagnostics) &&
                    viewportCount() == 1;
         }},
        {"a corrupt layout falls back to the factory one and tells the user",
         [this, layouts] {
             atomicWriteText(layouts / L"live.layout", "this is not a layout\n");
             workspaces_.requestSwitch(Workspace::Live);
         },
         [this, layouts] {
             std::error_code ec;
             const bool ok = workspaces_.current() == Workspace::Live && !workspaces_.notice().empty() && isDocked(PanelId::Session) &&
                             std::filesystem::exists(layouts / L"live.layout.corrupt", ec);
             workspaces_.clearNotice();
             return ok;
         }},
        {"the main window survives a resize",
         [this] { SetWindowPos(window_.hwnd(), nullptr, 0, 0, 900, 600, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE); },
         [] { return ImGui::GetMainViewport()->Size.x > 800.0f && ImGui::GetMainViewport()->Size.x < 1000.0f; }},
        {"minimize and restore keep drawing afterwards",
         [this] {
             ShowWindow(window_.hwnd(), SW_MINIMIZE);
             ShowWindow(window_.hwnd(), SW_RESTORE);
         },
         [this] { return !window_.isMinimized() && !deviceLost_; }},
    };
}

void Application::stepSelfTest(double time) {
    scheduler_.noteActivity(time); // Keep frames coming so each step finishes quickly.

    if (selfTestIndex_ >= selfTestSteps_.size()) {
        log::info(std::format("UI self-test finished: {} steps, {} failed", selfTestSteps_.size(), selfTestFailures_));
        selfTestDone_ = true;
        return;
    }
    if (framesTotal_ < 2) return; // The first frames build the factory layout.

    SelfTestStep& step = selfTestSteps_[selfTestIndex_];
    if (!selfTestActed_) {
        step.act();
        selfTestActed_ = true;
        selfTestCheckAtFrame_ = framesTotal_ + kFramesToSettle;
        return;
    }
    if (framesTotal_ < selfTestCheckAtFrame_) return;

    const bool passed = step.check();
    if (!passed) ++selfTestFailures_;
    log::info(std::format("UI self-test [{}] {}", passed ? "PASS" : "FAIL", step.name));
    ++selfTestIndex_;
    selfTestActed_ = false;
}

} // namespace ldaw
