// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/WorkspaceManager.h"

#include "core/logging/Log.h"
#include "core/settings/KeyValueStore.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <system_error>

namespace ldaw::ui {

const std::array<PanelDef, static_cast<std::size_t>(PanelId::Count)>& panelDefs() {
    static const std::array<PanelDef, static_cast<std::size_t>(PanelId::Count)> defs{{
        {PanelId::Library, "panel.library", "Library###panel.library"},
        {PanelId::Timeline, "panel.timeline", "Timeline###panel.timeline"},
        {PanelId::Session, "panel.session", "Session###panel.session"},
        {PanelId::Inspector, "panel.inspector", "Inspector###panel.inspector"},
        {PanelId::PianoRoll, "panel.pianoroll", "Piano Roll###panel.pianoroll"},
        {PanelId::Mixer, "panel.mixer", "Mixer###panel.mixer"},
        {PanelId::Diagnostics, "panel.diagnostics", "Diagnostics###panel.diagnostics"},
    }};
    return defs;
}

void WorkspaceManager::initialize(const std::filesystem::path& layoutDirectory, Workspace start, std::vector<IntRect> monitors) {
    directory_ = layoutDirectory;
    monitors_ = std::move(monitors);
    current_ = start;
    needsInitialLoad_ = true;
}

std::filesystem::path WorkspaceManager::pathFor(Workspace workspace) const {
    return directory_ / (workspace == Workspace::Live ? L"live.layout" : L"production.layout");
}

void WorkspaceManager::setFactoryPanels(Workspace workspace) {
    const bool live = workspace == Workspace::Live;
    panelOpen(PanelId::Library) = true;
    panelOpen(PanelId::Timeline) = !live;
    panelOpen(PanelId::Session) = live;
    panelOpen(PanelId::Inspector) = true;
    panelOpen(PanelId::PianoRoll) = !live;
    panelOpen(PanelId::Mixer) = true;
    panelOpen(PanelId::Diagnostics) = true;
}

void WorkspaceManager::load(Workspace workspace) {
    current_ = workspace;
    ImGui::ClearIniSettings();
    setFactoryPanels(workspace);

    bool readable = false;
    const auto path = pathFor(workspace);
    const std::string text = readTextFile(path, &readable);
    if (!readable) { // First run for this workspace.
        factoryBuildPending_ = true;
        return;
    }
    const auto layout = parseLayout(text);
    if (!layout) {
        // Keep the bad file for diagnosis, and never let it stop the app.
        auto aside = path;
        aside += L".corrupt";
        std::error_code ec;
        std::filesystem::rename(path, aside, ec);
        log::error("Unreadable layout set aside: " + aside.filename().generic_string());
        notice_ = "A saved workspace layout could not be read, so the default layout was restored.";
        factoryBuildPending_ = true;
        return;
    }
    for (const PanelDef& def : panelDefs()) {
        const auto it = layout->panels.find(def.stableId);
        if (it != layout->panels.end()) panelOpen(def.id) = it->second;
    }
    const std::string ini = rescueFloatingWindows(layout->imguiIni, monitors_);
    ImGui::LoadIniSettingsFromMemory(ini.c_str(), ini.size());
}

bool WorkspaceManager::save() {
    // Started minimized and closed before the first frame: nothing was loaded, so
    // saving now would overwrite the real layout with an empty one.
    if (needsInitialLoad_) return true;
    LayoutFile layout;
    for (const PanelDef& def : panelDefs()) layout.panels[def.stableId] = panelOpen(def.id);
    layout.imguiIni = ImGui::SaveIniSettingsToMemory();
    ImGui::GetIO().WantSaveIniSettings = false;
    const bool ok = atomicWriteText(pathFor(current_), serializeLayout(layout));
    if (!ok) log::error("Could not save workspace layout");
    return ok;
}

void WorkspaceManager::beforeNewFrame() {
    if (needsInitialLoad_) {
        needsInitialLoad_ = false;
        load(current_);
    }
    if (hasPendingSwitch_) {
        hasPendingSwitch_ = false;
        save();
        load(pendingSwitch_);
    }
    if (ImGui::GetIO().WantSaveIniSettings) save();
}

void WorkspaceManager::buildFactoryLayoutIfNeeded(unsigned dockspaceId, float width, float height) {
    if (!factoryBuildPending_) return;
    factoryBuildPending_ = false;

    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImVec2(width, height));

    ImGuiID center = dockspaceId;
    const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.18f, nullptr, &center);
    const ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.22f, nullptr, &center);
    const ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.34f, nullptr, &center);

    const auto name = [](PanelId id) { return panelDefs()[static_cast<std::size_t>(id)].windowName; };
    ImGui::DockBuilderDockWindow(name(PanelId::Library), left);
    ImGui::DockBuilderDockWindow(name(PanelId::Inspector), right);
    ImGui::DockBuilderDockWindow(name(PanelId::Timeline), center);
    ImGui::DockBuilderDockWindow(name(PanelId::Session), center);
    ImGui::DockBuilderDockWindow(name(PanelId::PianoRoll), bottom);
    ImGui::DockBuilderDockWindow(name(PanelId::Mixer), bottom);
    ImGui::DockBuilderDockWindow(name(PanelId::Diagnostics), bottom);
    ImGui::DockBuilderFinish(dockspaceId);
}

void WorkspaceManager::notifyDisplayChanged(std::vector<IntRect> monitors) {
    monitors_ = std::move(monitors);
    rescuePending_ = true;
}

void WorkspaceManager::rescueWindowsIfNeeded() {
    if (!rescuePending_) return;
    rescuePending_ = false;
    for (ImGuiWindow* window : ImGui::GetCurrentContext()->Windows) {
        if (window->ParentWindow || window->DockIsActive || window->Hidden) continue;
        if (window->Flags & (ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_Popup | ImGuiWindowFlags_NoMove)) continue;
        const IntRect current{static_cast<int>(window->Pos.x), static_cast<int>(window->Pos.y),
                              static_cast<int>(window->Size.x), static_cast<int>(window->Size.y)};
        const IntRect fixed = rescueRect(current, monitors_);
        if (!(fixed == current)) {
            ImGui::SetWindowPos(window, ImVec2(static_cast<float>(fixed.x), static_cast<float>(fixed.y)));
            log::info(std::string("Moved stranded panel back on screen: ") + window->Name);
        }
    }
}

} // namespace ldaw::ui
