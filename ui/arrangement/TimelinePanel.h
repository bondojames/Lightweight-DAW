// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/commands/Edits.h"
#include "engine/AudioEngine.h"
#include "engine/media/MediaManager.h"
#include "ui/arrangement/TimelineView.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace ldaw::ui {

// Harmless view state: where the user is looking and what's selected. None of it is
// the song, so losing it loses no work.
struct TimelineState {
    TimelineView view;
    float scrollY = 0.0f;
    model::Id selectedClip = model::kNoId;
    model::Id selectedTrack = model::kNoId;
    bool snap = true;
    bool followPlayhead = true;

    enum class Drag { None, Move, TrimStart, TrimEnd, Scrub, Loop };
    Drag drag = Drag::None;
    model::Id dragClip = model::kNoId;
    model::Time dragGrabOffset = 0;
    model::Time dragStartTime = 0;
    model::Time dragPreviewStart = 0;
    model::Time dragPreviewEnd = 0;
    model::Id dragPreviewTrack = model::kNoId;

    // Filled in while drawing, for a file dropped on the window this frame.
    model::Id dropTrack = model::kNoId;
    model::Time dropTime = 0;
    bool dropInside = false;
};

struct TimelineContext {
    commands::ProjectHistory& history;
    engine::MediaManager& media;
    engine::EngineState engine;
    bool audioRunning = false;
    std::function<void(const engine::EngineCommand&)> post;
};

void drawTimelinePanel(TimelineState& state, const TimelineContext& context);
void drawInspectorPanel(TimelineState& state, const TimelineContext& context);

} // namespace ldaw::ui
