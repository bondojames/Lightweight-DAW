// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/model/Project.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ldaw::commands {

using model::Id;
using model::Project;
using model::Time;

// Every edit validates its input and returns false, leaving the project unchanged, if it
// can't apply. None of them ever touches a media file.
Id addTrack(Project& project, const std::string& name);
bool removeTrack(Project& project, Id trackId);
Id addMedia(Project& project, const std::filesystem::path& path, Time length, std::uint32_t channels, double sourceSampleRate);
Id addClip(Project& project, Id trackId, Id mediaId, Time start);
bool deleteClip(Project& project, Id clipId);
bool moveClip(Project& project, Id clipId, Id targetTrackId, Time newStart);
// Moves the clip's left edge. The audio under the rest of the clip stays where it is.
bool trimClipStart(Project& project, Id clipId, Time newStart);
bool trimClipEnd(Project& project, Id clipId, Time newEnd);
// Returns the new right-hand clip's id, or kNoId if the point isn't inside the clip.
Id splitClip(Project& project, Id clipId, Time at);
Id duplicateClip(Project& project, Id clipId);
bool setClipGain(Project& project, Id clipId, float gain);
bool setTrackGain(Project& project, Id trackId, float gain);
bool setTrackPan(Project& project, Id trackId, float pan);
bool setTrackMute(Project& project, Id trackId, bool mute);
bool setTrackSolo(Project& project, Id trackId, bool solo);
bool setLoop(Project& project, bool enabled, Time start, Time end);

// Immutable project versions with undo and redo. Each version can be read from any
// thread with no lock, because it never changes after being published.
class ProjectHistory {
public:
    using Edit = std::function<bool(Project&)>;

    ProjectHistory();

    std::shared_ptr<const Project> current() const { return current_; }
    std::uint64_t revision() const { return revision_; }

    // Applies the edit to a copy. Returns false, and records nothing, if the edit refused.
    bool apply(const std::string& name, const Edit& edit);
    // Like apply, but continued edits under one group merge into a single undo step, so
    // dragging a slider is one undo. Call endGroup when the drag ends.
    bool applyGrouped(const std::string& name, const Edit& edit);
    void endGroup() { groupOpen_ = false; }

    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    const std::string& undoName() const;
    const std::string& redoName() const;
    bool undo();
    bool redo();

private:
    struct Entry {
        std::string name;
        std::shared_ptr<const Project> project;
    };
    static constexpr std::size_t kMaxUndo = 256;

    std::shared_ptr<const Project> current_;
    std::vector<Entry> undo_;
    std::vector<Entry> redo_;
    std::uint64_t revision_ = 0;
    bool groupOpen_ = false;
};

} // namespace ldaw::commands
