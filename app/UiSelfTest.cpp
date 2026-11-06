// SPDX-License-Identifier: GPL-3.0-or-later
// Drives the real window, ImGui context and D3D11 device through the workspace and
// recovery paths, then checks actual ImGui state. It cannot replace the physical
// tests in tests/manual (real monitors, real mouse dragging, real DPI changes).
#include <windows.h>

#include "app/Application.h"

#include "core/logging/Log.h"
#include "core/commands/Edits.h"
#include "core/settings/KeyValueStore.h"
#include "engine/render/OfflineRender.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <cmath>
#include <filesystem>
#include <memory>
#include <format>
#include <string>

namespace ldaw {
namespace {

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

// Panel windows only. A tooltip under the user's resting mouse is also an OS window,
// and must not change the result.
int viewportCount() {
    int count = 0;
    for (ImGuiViewport* viewport : ImGui::GetPlatformIO().Viewports) {
        const ImGuiWindow* window = static_cast<ImGuiViewportP*>(viewport)->Window;
        if (!window || !(window->Flags & (ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_Popup))) ++count;
    }
    return count;
}

} // namespace

void Application::buildSelfTest() {
    using ui::PanelId;
    using ui::Workspace;
    const auto layouts = dataDir_ / L"layouts";

    // Silent: audio really runs through the device, at zero output gain. Machines with no
    // audio device, such as CI runners, must instead show a clear message.
    const auto audioIsRunning = [this] { return uiState_.audio.status == engine::AudioStatus::Running; };
    const auto clockIsMoving = [this, audioIsRunning, last = std::make_shared<std::int64_t>(-1)] {
        if (!audioIsRunning()) return !uiState_.audio.message.empty();
        const std::int64_t position = audio_->engineState().positionFrames;
        const bool moved = position > *last;
        *last = position;
        return moved && audio_->engineState().playing;
    };

    selfTestSteps_ = {
        {"audio reports its state: running, or a clear message", [] {},
         [this, audioIsRunning] { return uiState_.audio.status != engine::AudioStatus::NotInitialized && (audioIsRunning() || !uiState_.audio.message.empty()); },
         240},
        {"Play starts the sample clock",
         [this] {
             audio_->post({engine::EngineCommandType::SetOutputGain, 0.0});
             audio_->post({engine::EngineCommandType::SetMetronome, 1.0});
             audio_->post({engine::EngineCommandType::Play, 0.0});
         },
         clockIsMoving},
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
        {"the audio clock kept running through the GPU reset and workspace changes", [] {}, clockIsMoving},
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
        {"the audio clock survived all of the UI work above", [] {}, clockIsMoving},
        {"Stop halts the clock", [this] { audio_->post({engine::EngineCommandType::Stop, 0.0}); },
         [this, audioIsRunning] { return !audioIsRunning() || !audio_->engineState().playing; }},
        {"importing a file makes a track and a clip",
         [this] {
             const auto path = dataDir_ / L"selftest_tést.wav";
             engine::StereoBuffer audio;
             audio.left.resize(96000);
             for (std::size_t i = 0; i < audio.left.size(); ++i) audio.left[i] = 0.25f * static_cast<float>(std::sin(static_cast<double>(i) * 0.05));
             audio.right = audio.left;
             engine::writeWavFloat32(path, audio, 44100);
             requestImport(path, model::kNoId, 0);
         },
         [this] {
             const auto project = history_.current();
             return project->tracks.size() == 1 && project->tracks[0].clips.size() == 1 && uiState_.timeline.selectedClip != model::kNoId && uiState_.importMessage.empty();
         },
         240},
        {"a clip plays with no disk misses, while the timeline draws it",
         [this] {
             audio_->post({engine::EngineCommandType::Rewind, 0.0});
             audio_->post({engine::EngineCommandType::Play, 0.0});
         },
         [this, audioIsRunning] {
             if (!audioIsRunning()) return true; // No device to play through.
             const engine::EngineState state = audio_->engineState();
             return state.playing && state.positionFrames > 0 && audio_->diskMissFrames() == 0 && audio_->streamedBlocks() > 0;
         },
         60},
        {"split, then undo, restores the clip",
         [this] {
             audio_->post({engine::EngineCommandType::Stop, 0.0});
             const model::Id clip = uiState_.timeline.selectedClip;
             history_.apply("Split clip", [&](model::Project& p) { return commands::splitClip(p, clip, model::kTimePerSecond) != model::kNoId; });
         },
         [this] {
             if (history_.current()->tracks[0].clips.size() != 2) return false;
             history_.undo();
             return history_.current()->tracks[0].clips.size() == 1;
         }},
        {"a bad file reports an error and changes nothing",
         [this] { requestImport(dataDir_ / L"no_such_file.wav", model::kNoId, 0); },
         [this] { return !uiState_.importMessage.empty() && history_.current()->tracks.size() == 1; },
         120},
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
        selfTestCheckAtFrame_ = framesTotal_ + step.settleFrames;
        return;
    }
    if (framesTotal_ < selfTestCheckAtFrame_) return;

    const bool passed = step.check();
    if (!passed) ++selfTestFailures_;
    if (!passed) {
        // Enough state to diagnose a failure from the log alone.
        const engine::EngineState audio = audio_->engineState();
        log::info(std::format("  audio playing={} position={} rate={:.0f} diskMisses={} blocks={}", audio.playing, audio.positionFrames, audio.sampleRate, audio_->diskMissFrames(), audio_->streamedBlocks()));
        for (const ImGuiViewport* viewport : ImGui::GetPlatformIO().Viewports) {
            log::info(std::format("  viewport pos {:.0f},{:.0f} size {:.0f}x{:.0f} flags 0x{:X}", viewport->Pos.x, viewport->Pos.y, viewport->Size.x, viewport->Size.y, static_cast<int>(viewport->Flags)));
        }
        for (const ui::PanelDef& def : ui::panelDefs()) {
            if (const ImGuiWindow* window = ImGui::FindWindowByName(def.windowName)) {
                log::info(std::format("  {} open={} docked={} pos {:.0f},{:.0f}", def.stableId, workspaces_.panelOpen(def.id), window->DockIsActive, window->Pos.x, window->Pos.y));
            }
        }
    }
    log::info(std::format("UI self-test [{}] {}", passed ? "PASS" : "FAIL", step.name));
    ++selfTestIndex_;
    selfTestActed_ = false;
}

} // namespace ldaw
