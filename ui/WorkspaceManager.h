// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/layout/LayoutFile.h"

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace ldaw::ui {

enum class Workspace { Production, Live };

enum class PanelId { Library, Timeline, Session, Inspector, PianoRoll, Mixer, Diagnostics, Count };

struct PanelDef {
    PanelId id;
    const char* stableId; // Persisted. Never change it, even if the title changes.
    const char* windowName; // The part after ### keeps ImGui's ID stable if the label changes.
};

const std::array<PanelDef, static_cast<std::size_t>(PanelId::Count)>& panelDefs();

// Named Production and Live layouts: display state only, never musical content, and
// switching them never touches the audio engine (spec section 5.1).
class WorkspaceManager {
public:
    void initialize(const std::filesystem::path& layoutDirectory, Workspace start, std::vector<IntRect> monitors);

    Workspace current() const { return current_; }
    bool& panelOpen(PanelId id) { return panelOpen_[static_cast<std::size_t>(id)]; }

    void requestSwitch(Workspace target) { pendingSwitch_ = target; hasPendingSwitch_ = target != current_; }
    void requestReset() { factoryBuildPending_ = true; setFactoryPanels(current_); }

    void notifyDisplayChanged(std::vector<IntRect> monitors);

    // Must run between frames: ImGui refuses to load layouts inside one.
    void beforeNewFrame();
    // Runs inside the frame, before the dockspace is submitted.
    void buildFactoryLayoutIfNeeded(unsigned dockspaceId, float width, float height);
    // Runs inside the frame: brings back panels stranded by a monitor change.
    void rescueWindowsIfNeeded();

    bool save();

    // Set when a saved layout was unreadable and was replaced by the factory one.
    const std::string& notice() const { return notice_; }
    void clearNotice() { notice_.clear(); }

private:
    std::filesystem::path pathFor(Workspace workspace) const;
    void load(Workspace workspace);
    void setFactoryPanels(Workspace workspace);

    std::filesystem::path directory_;
    std::vector<IntRect> monitors_;
    Workspace current_ = Workspace::Production;
    Workspace pendingSwitch_ = Workspace::Production;
    bool hasPendingSwitch_ = false;
    bool needsInitialLoad_ = false;
    bool factoryBuildPending_ = false;
    bool rescuePending_ = false;
    std::array<bool, static_cast<std::size_t>(PanelId::Count)> panelOpen_{};
    std::string notice_;
};

} // namespace ldaw::ui
