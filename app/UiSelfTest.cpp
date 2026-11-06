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

// --- Simulated input. One event per frame, as real input arrives, always pointing at the
// main window, so the real mouse can't interfere wherever it happens to be resting.
void Application::injectTestInput() {
    ImGuiIO& io = ImGui::GetIO();
    if (!testInput_.empty()) {
        testLastInput_ = testInput_.front();
        testInput_.pop_front();
        const TestInput& input = testLastInput_;
        io.AddMouseViewportEvent(ImGui::GetMainViewport()->ID);
        io.AddMousePosEvent(input.x, input.y);
        if (input.keyMod != 0) io.AddKeyEvent(static_cast<ImGuiKey>(input.keyMod), input.keyDown);
        if (input.button >= 0) io.AddMouseButtonEvent(input.button, input.down);
        if (input.wheel != 0.0f) io.AddMouseWheelEvent(0.0f, input.wheel);
    } else if (testLastInput_.x >= 0.0f) {
        io.AddMouseViewportEvent(ImGui::GetMainViewport()->ID);
        io.AddMousePosEvent(testLastInput_.x, testLastInput_.y);
    }
}

void Application::sendMove(float x, float y) {
    TestInput input;
    input.x = x;
    input.y = y;
    testInput_.push_back(input);
}

void Application::sendClick(float x, float y) {
    sendMove(x, y);
    sendMove(x, y);
    TestInput down;
    down.x = x;
    down.y = y;
    down.button = 0;
    down.down = true;
    testInput_.push_back(down);
    sendMove(x, y);
    TestInput up = down;
    up.down = false;
    testInput_.push_back(up);
    sendMove(x, y);
}

void Application::sendDrag(float x0, float y0, float x1, float y1, bool shift) {
    sendMove(x0, y0);
    sendMove(x0, y0);
    if (shift) {
        TestInput key;
        key.x = x0;
        key.y = y0;
        key.keyMod = static_cast<int>(ImGuiMod_Shift);
        key.keyDown = true;
        testInput_.push_back(key);
    }
    TestInput down;
    down.x = x0;
    down.y = y0;
    down.button = 0;
    down.down = true;
    testInput_.push_back(down);
    for (int i = 1; i <= 8; ++i) {
        const float t = static_cast<float>(i) / 8.0f;
        sendMove(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t);
    }
    TestInput up = down;
    up.x = x1;
    up.y = y1;
    up.down = false;
    testInput_.push_back(up);
    if (shift) {
        TestInput key;
        key.x = x1;
        key.y = y1;
        key.keyMod = static_cast<int>(ImGuiMod_Shift);
        key.keyDown = false;
        testInput_.push_back(key);
    }
    sendMove(x1, y1);
}

void Application::sendWheel(float x, float y, float amount, bool ctrl) {
    sendMove(x, y);
    sendMove(x, y);
    TestInput key;
    key.x = x;
    key.y = y;
    if (ctrl) {
        key.keyMod = static_cast<int>(ImGuiMod_Ctrl);
        key.keyDown = true;
        testInput_.push_back(key);
    }
    TestInput wheel;
    wheel.x = x;
    wheel.y = y;
    wheel.wheel = amount;
    testInput_.push_back(wheel);
    sendMove(x, y);
    if (ctrl) {
        key.keyDown = false;
        testInput_.push_back(key);
    }
    sendMove(x, y);
}

const model::Clip* Application::testClip() const {
    const auto project = history_.current();
    for (const model::Track& track : project->tracks) {
        if (!track.clips.empty()) {
            testClipCopy_ = track.clips.front();
            return &testClipCopy_;
        }
    }
    return nullptr;
}

float Application::testClipX(float fraction) const {
    const model::Clip* clip = testClip();
    if (!clip) return 0.0f;
    const model::Time at = clip->start + static_cast<model::Time>(static_cast<double>(clip->length) * fraction);
    return uiState_.timeline.geometry.canvasMinX + static_cast<float>(uiState_.timeline.view.timeToPixel(at));
}

float Application::testClipEndX() const { return testClipX(1.0f); }

float Application::testLaneY(int lane) const {
    const auto& g = uiState_.timeline.geometry;
    return g.canvasMinY - uiState_.timeline.scrollY + g.trackHeight * (static_cast<float>(lane) + 0.5f);
}

void Application::buildSelfTest() {
    using ui::PanelId;
    using ui::Workspace;
    imgui_.setInputInjector([this] { injectTestInput(); });
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

        // --- Real gestures: simulated mouse input through the actual widgets.
        {"the Production workspace shows the timeline",
         [this] {
             SetWindowPos(window_.hwnd(), nullptr, 0, 0, 1280, 800, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
             workspaces_.requestSwitch(Workspace::Production);
         },
         [this] {
             // The track headers start at the panel's left edge, so the canvas starts just past them.
             const ImGuiWindow* window = findPanel(PanelId::Timeline);
             const auto& g = uiState_.timeline.geometry;
             const float headerWidth = 170.0f * ImGui::GetFontSize() / 16.0f;
             return workspaces_.panelOpen(PanelId::Timeline) && g.valid && window && std::abs(g.canvasMinX - window->Pos.x - headerWidth) < 24.0f;
         }, 30},
        {"clicking an empty part of a lane deselects the clip",
         [this] { sendClick(testClipEndX() + 60.0f, testLaneY(0)); },
         [this] { return uiState_.timeline.selectedClip == model::kNoId; }, 24},
        {"clicking a clip selects it",
         [this] { sendClick(testClipX(0.5f), testLaneY(0)); },
         [this] { return uiState_.timeline.selectedClip != model::kNoId; }, 24},
        {"dragging a clip moves it",
         [this] { sendDrag(testClipX(0.5f), testLaneY(0), testClipX(0.5f) + 100.0f, testLaneY(0)); },
         [this] { return testClip() && testClip()->start > 0; }, 30},
        {"a clip can be dragged to another track",
         [this] {
             history_.apply("Add track", [](model::Project& p) { commands::addTrack(p, "Second"); return true; });
             sendDrag(testClipX(0.5f), testLaneY(0), testClipX(0.5f), testLaneY(1));
         },
         [this] {
             const auto project = history_.current();
             return project->tracks.size() == 2 && project->tracks[0].clips.empty() && project->tracks[1].clips.size() == 1;
         }, 30},
        {"dragging a clip's right edge trims it",
         [this] {
             testLength_ = testClip() ? testClip()->length : 0;
             sendDrag(testClipEndX() - 2.0f, testLaneY(1), testClipEndX() - 40.0f, testLaneY(1));
         },
         [this] { return testClip() && testClip()->length < testLength_; }, 30},
        {"dragging a clip's left edge trims it and keeps the audio in place",
         [this] {
             testLength_ = testClip() ? testClip()->length : 0;
             sendDrag(testClipX(0.0f) + 2.0f, testLaneY(1), testClipX(0.0f) + 20.0f, testLaneY(1));
         },
         [this] { return testClip() && testClip()->length < testLength_ && testClip()->sourceOffset > 0; }, 30},
        {"clicking the ruler moves the playhead",
         [this] {
             const auto& g = uiState_.timeline.geometry;
             const engine::EngineState before = audio_->engineState();
             // Well away from where it is now, so moving proves the click worked.
             const float x = g.canvasMinX + 120.0f;
             testSeekFrames_ = model::timeToFrames(uiState_.timeline.view.pixelToTime(120.0), before.sampleRate > 0.0 ? before.sampleRate : 48000.0);
             sendClick(x, g.rulerMinY + 4.0f);
         },
         [this, audioIsRunning] { return !audioIsRunning() || std::abs(audio_->engineState().positionFrames - testSeekFrames_) < 4800; }, 30},
        {"Ctrl+wheel zooms",
         [this] {
             testZoom_ = uiState_.timeline.view.pixelsPerSecond();
             sendWheel(uiState_.timeline.geometry.canvasMinX + 50.0f, testLaneY(0), 1.0f, true);
         },
         [this] { return uiState_.timeline.view.pixelsPerSecond() > testZoom_; }, 24},
        {"the wheel scrolls",
         [this] {
             testScroll_ = uiState_.timeline.view.scrollSeconds();
             sendWheel(uiState_.timeline.geometry.canvasMinX + 50.0f, testLaneY(0), -1.0f, false);
         },
         [this] { return uiState_.timeline.view.scrollSeconds() > testScroll_; }, 24},
        {"a track's gain slider responds",
         [this] {
             const auto& g = uiState_.timeline.geometry;
             sendClick(g.gainSliderX, g.gainSliderY);
         },
         [this] { return history_.current()->tracks[0].gain != 1.0f || history_.current()->tracks[1].gain != 1.0f; }, 24},
        {"dragging a gain slider follows the mouse and is one undo step",
         [this] {
             const auto& g = uiState_.timeline.geometry;
             // The recorded slider belongs to the last track drawn.
             testGain_ = history_.current()->tracks.back().gain;
             testRevision_ = history_.revision();
             sendDrag(g.gainSliderX - 40.0f, g.gainSliderY, g.gainSliderX + 40.0f, g.gainSliderY);
         },
         [this] {
             const float dragged = history_.current()->tracks.back().gain;
             const bool moved = dragged > testGain_ && history_.revision() > testRevision_ + 1; // Several live changes...
             log::info(std::format("  slider drag: gain {:.3f} to {:.3f}, {} revisions", testGain_, dragged, history_.revision() - testRevision_));
             history_.undo();
             return moved && history_.current()->tracks.back().gain == testGain_; // ...undone in one step.
         }, 30},
        {"Shift-dragging the ruler sets a loop",
         [this] {
             const auto& g = uiState_.timeline.geometry;
             sendDrag(g.canvasMinX + 40.0f, g.rulerMinY + 4.0f, g.canvasMinX + 160.0f, g.rulerMinY + 4.0f, true);
         },
         [this] { return history_.current()->loopEnabled && history_.current()->loopEnd > history_.current()->loopStart; }, 30},
        {"a dropped file lands on the lane under the drop point",
         [] {},
         [this] {
             const auto& g = uiState_.timeline.geometry;
             const auto project = history_.current();
             const auto onLane = ui::timelineDropTarget(uiState_.timeline, *project, 120.0, static_cast<int>(g.canvasMinX + 80.0f), static_cast<int>(testLaneY(1)));
             const auto onSpace = ui::timelineDropTarget(uiState_.timeline, *project, 120.0, static_cast<int>(g.canvasMinX + 80.0f), static_cast<int>(g.canvasMinY + g.trackHeight * 2.5f));
             const auto outside = ui::timelineDropTarget(uiState_.timeline, *project, 120.0, static_cast<int>(g.canvasMinX - 50.0f), static_cast<int>(testLaneY(1)));
             return onLane.insideTimeline && onLane.trackId == project->tracks[1].id && onLane.time > 0 && !outside.insideTimeline && (!onSpace.insideTimeline || onSpace.trackId == model::kNoId);
         }},
        {"the timeline still draws and responds in a smaller window",
         [this] {
             SetWindowPos(window_.hwnd(), nullptr, 0, 0, 900, 600, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
             testZoom_ = 0.0;
         },
         [this] {
             const auto& g = uiState_.timeline.geometry;
             const ImGuiViewport* main = ImGui::GetMainViewport();
             return g.valid && g.canvasMaxX <= main->Pos.x + main->Size.x + 1.0f && g.canvasMinX > main->Pos.x;
         }, 30},
        {"a click still works in that smaller window",
         [this] {
             testZoom_ = uiState_.timeline.view.pixelsPerSecond();
             const auto& g = uiState_.timeline.geometry;
             sendWheel(g.canvasMinX + 20.0f, g.canvasMinY + 20.0f, 1.0f, true);
         },
         [this] { return uiState_.timeline.view.pixelsPerSecond() > testZoom_; }, 24},
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
        const ImGuiContext& g = *ImGui::GetCurrentContext();
        log::info(std::format("  mouse {:.0f},{:.0f} hoveredWindow={} navWindow={} activeId={:X} hoveredId={:X} popups={} moving={} queued={}", g.IO.MousePos.x, g.IO.MousePos.y,
                              g.HoveredWindow ? g.HoveredWindow->Name : "none", g.NavWindow ? g.NavWindow->Name : "none", g.ActiveId, g.HoveredId, g.OpenPopupStack.Size,
                              g.MovingWindow ? g.MovingWindow->Name : "none", testInput_.size()));
        const auto& geo = uiState_.timeline.geometry;
        log::info(std::format("  canvas {:.0f},{:.0f} to {:.0f},{:.0f} ruler {:.0f} trackH {:.0f} slider {:.0f},{:.0f} pps {:.1f} scroll {:.2f}", geo.canvasMinX, geo.canvasMinY, geo.canvasMaxX, geo.canvasMaxY, geo.rulerMinY, geo.trackHeight, geo.gainSliderX, geo.gainSliderY, uiState_.timeline.view.pixelsPerSecond(), uiState_.timeline.view.scrollSeconds()));
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
