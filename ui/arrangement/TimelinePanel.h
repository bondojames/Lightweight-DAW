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

    // Where things were last drawn, in screen pixels. Used to place dropped files, since
    // Windows sends no mouse moves during a file drag, and by the UI self-test.
    struct Geometry {
        bool valid = false;
        float canvasMinX = 0.0f, canvasMinY = 0.0f, canvasMaxX = 0.0f, canvasMaxY = 0.0f;
        float rulerMinY = 0.0f;
        float trackHeight = 1.0f;
        float gainSliderX = 0.0f, gainSliderY = 0.0f; // Centre of the first track's gain slider.
    };
    Geometry geometry;
};

// Where a file dropped at this screen point should land.
struct DropTarget {
    bool insideTimeline = false;
    model::Id trackId = model::kNoId; // kNoId means a new track.
    model::Time time = 0;
};
DropTarget timelineDropTarget(const TimelineState& state, const model::Project& project, double bpm, int screenX, int screenY);

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
