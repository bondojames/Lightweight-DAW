// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/arrangement/TimelinePanel.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ldaw::ui {
namespace {

using commands::ProjectHistory;
using model::Clip;
using model::Id;
using model::kNoId;
using model::Project;
using model::Time;
using model::Track;

float amplitudeToDb(float gain) { return gain <= 0.00001f ? -100.0f : 20.0f * std::log10(gain); }
float dbToAmplitude(float db) { return db <= -60.0f ? 0.0f : std::pow(10.0f, db / 20.0f); }

Time snapped(const TimelineState& state, Time time, double bpm) {
    // Alt turns snap off for this gesture.
    return (state.snap && !ImGui::GetIO().KeyAlt) ? snapToBeat(time, bpm) : time;
}

void drawWaveform(ImDrawList* draw, const engine::WaveformPeaks& peaks, const Clip& clip, const TimelineView& view, double rate, ImVec2 clipMin, ImVec2 clipMax, float canvasLeft, float canvasRight, ImU32 color) {
    const double framesPerPixel = rate / view.pixelsPerSecond();
    const engine::WaveformPeaks::Level* level = peaks.levelFor(framesPerPixel);
    if (!level) return;

    // Only the columns on screen are ever drawn, so an hour-long clip costs the same as a short one.
    const int firstColumn = static_cast<int>(std::max(clipMin.x, canvasLeft));
    const int lastColumn = static_cast<int>(std::min(clipMax.x, canvasRight));
    if (lastColumn <= firstColumn) return;

    const float laneHeight = (clipMax.y - clipMin.y) / static_cast<float>(peaks.channels);
    const double sourceStart = model::timeToSeconds(clip.sourceOffset) * rate;
    const auto binsPerPixel = std::max<std::int64_t>(1, static_cast<std::int64_t>(framesPerPixel / level->framesPerBin));
    const float scale = clip.gain / 32768.0f;

    for (std::uint32_t channel = 0; channel < peaks.channels; ++channel) {
        const std::vector<engine::PeakBin>& bins = level->bins[channel];
        const float middle = clipMin.y + laneHeight * (static_cast<float>(channel) + 0.5f);
        const float halfHeight = laneHeight * 0.48f;
        for (int x = firstColumn; x < lastColumn; ++x) {
            const double sourceFrame = sourceStart + (static_cast<double>(x) - clipMin.x) * framesPerPixel;
            const auto first = static_cast<std::int64_t>(sourceFrame / level->framesPerBin);
            if (first < 0 || first >= static_cast<std::int64_t>(bins.size())) continue;
            int low = 32767, high = -32768;
            const std::int64_t last = std::min<std::int64_t>(first + binsPerPixel, static_cast<std::int64_t>(bins.size()));
            for (std::int64_t i = first; i < last; ++i) {
                low = std::min<int>(low, bins[static_cast<std::size_t>(i)].min);
                high = std::max<int>(high, bins[static_cast<std::size_t>(i)].max);
            }
            const float top = middle - std::clamp(static_cast<float>(high) * scale, -1.0f, 1.0f) * halfHeight;
            const float bottom = middle - std::clamp(static_cast<float>(low) * scale, -1.0f, 1.0f) * halfHeight;
            draw->AddRectFilled(ImVec2(static_cast<float>(x), top), ImVec2(static_cast<float>(x) + 1.0f, std::max(bottom, top + 1.0f)), color);
        }
    }
}

void drawRuler(ImDrawList* draw, ImVec2 min, ImVec2 max, const TimelineView& view, double bpm) {
    draw->AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_FrameBg));
    const double barSeconds = 60.0 / bpm * 4.0;
    const int labelEvery = barsPerLabel(view.pixelsPerSecond(), bpm, 4, ImGui::GetFontSize() * 4.0f);
    const auto firstBar = static_cast<std::int64_t>(view.scrollSeconds() / barSeconds);
    const double width = max.x - min.x;
    for (std::int64_t bar = firstBar - firstBar % labelEvery;; bar += labelEvery) {
        const double x = (static_cast<double>(bar) * barSeconds - view.scrollSeconds()) * view.pixelsPerSecond();
        if (x > width) break;
        if (x < 0.0) continue;
        const auto px = min.x + static_cast<float>(x);
        draw->AddLine(ImVec2(px, min.y + (max.y - min.y) * 0.5f), ImVec2(px, max.y), ImGui::GetColorU32(ImGuiCol_Text));
        char label[32];
        std::snprintf(label, sizeof(label), "%lld", static_cast<long long>(bar + 1));
        draw->AddText(ImVec2(px + 3.0f, min.y), ImGui::GetColorU32(ImGuiCol_Text), label);
    }
}

void drawBarLines(ImDrawList* draw, ImVec2 min, ImVec2 max, const TimelineView& view, double bpm) {
    const double beatSeconds = 60.0 / bpm;
    const double beatPixels = beatSeconds * view.pixelsPerSecond();
    const double barPixels = beatPixels * 4.0;
    const double step = beatPixels >= 12.0 ? beatSeconds : (barPixels >= 12.0 ? beatSeconds * 4.0 : 0.0);
    if (step <= 0.0) return;
    const double width = max.x - min.x;
    const auto first = static_cast<std::int64_t>(view.scrollSeconds() / step);
    const ImU32 color = ImGui::GetColorU32(ImGuiCol_Separator, 0.35f);
    for (std::int64_t i = first;; ++i) {
        const double x = (static_cast<double>(i) * step - view.scrollSeconds()) * view.pixelsPerSecond();
        if (x > width) break;
        if (x >= 0.0) draw->AddLine(ImVec2(min.x + static_cast<float>(x), min.y), ImVec2(min.x + static_cast<float>(x), max.y), color);
    }
}

void drawTrackHeader(TimelineState& state, ProjectHistory& history, const Track& track, ImVec2 min, float width, float height) {
    ImGui::PushID(static_cast<int>(track.id));
    ImGui::SetCursorScreenPos(ImVec2(min.x + 4.0f, min.y + 2.0f));
    ImGui::BeginGroup();
    const bool selected = state.selectedTrack == track.id;
    if (ImGui::Selectable(track.name.c_str(), selected, 0, ImVec2(width - 8.0f, 0.0f))) {
        state.selectedTrack = track.id;
        state.selectedClip = kNoId;
    }
    bool mute = track.mute;
    if (ImGui::Checkbox("M", &mute)) history.apply("Mute track", [&](Project& p) { return commands::setTrackMute(p, track.id, mute); });
    ImGui::SetItemTooltip("Mute");
    ImGui::SameLine();
    bool solo = track.solo;
    if (ImGui::Checkbox("S", &solo)) history.apply("Solo track", [&](Project& p) { return commands::setTrackSolo(p, track.id, solo); });
    ImGui::SetItemTooltip("Solo");
    if (height > ImGui::GetFrameHeightWithSpacing() * 3.0f) {
        ImGui::SetNextItemWidth(width - 8.0f);
        float db = amplitudeToDb(track.gain);
        if (ImGui::SliderFloat("##gain", &db, -60.0f, 12.0f, db <= -60.0f ? "-inf dB" : "%.1f dB")) {
            const float gain = dbToAmplitude(db);
            history.applyGrouped("Track gain", [&](Project& p) { return commands::setTrackGain(p, track.id, gain); });
        }
        if (ImGui::IsItemDeactivated()) history.endGroup();
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            history.apply("Track gain", [&](Project& p) { return commands::setTrackGain(p, track.id, 1.0f); });
        }
        ImGui::SetItemTooltip("Track gain. Right-click for 0 dB.");
        ImGui::SetNextItemWidth(width - 8.0f);
        float pan = track.pan;
        if (ImGui::SliderFloat("##pan", &pan, -1.0f, 1.0f, pan == 0.0f ? "Centre" : (pan < 0.0f ? "L %.2f" : "R %.2f"))) {
            history.applyGrouped("Track pan", [&](Project& p) { return commands::setTrackPan(p, track.id, pan); });
        }
        if (ImGui::IsItemDeactivated()) history.endGroup();
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            history.apply("Track pan", [&](Project& p) { return commands::setTrackPan(p, track.id, 0.0f); });
        }
        ImGui::SetItemTooltip("Pan. Right-click to centre.");
    }
    ImGui::EndGroup();
    ImGui::PopID();
}

} // namespace

void drawTimelinePanel(TimelineState& state, const TimelineContext& context) {
    ProjectHistory& history = context.history;
    const auto project = history.current();
    const double bpm = context.engine.bpm > 0.0 ? context.engine.bpm : 120.0;
    const double rate = context.media.sampleRate();
    const float scale = ImGui::GetFontSize() / 16.0f;

    // --- Toolbar
    if (ImGui::SmallButton("Add track")) {
        history.apply("Add track", [&](Project& p) {
            state.selectedTrack = commands::addTrack(p, "Audio " + std::to_string(p.tracks.size() + 1));
            return true;
        });
    }
    ImGui::SameLine();
    ImGui::Checkbox("Snap", &state.snap);
    ImGui::SetItemTooltip("Snap edits to beats. Hold Alt to ignore it for one gesture.");
    ImGui::SameLine();
    bool loop = project->loopEnabled;
    ImGui::BeginDisabled(project->loopEnd <= project->loopStart);
    if (ImGui::Checkbox("Loop", &loop)) {
        history.apply("Toggle loop", [&](Project& p) { return commands::setLoop(p, loop, p.loopStart, p.loopEnd); });
    }
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("Shift-drag in the ruler to set the loop region.");
    ImGui::SameLine();
    ImGui::Checkbox("Follow", &state.followPlayhead);
    ImGui::SetItemTooltip("Scroll to keep the playhead in view while playing.");
    ImGui::SameLine();
    const std::size_t jobs = context.media.pendingJobs();
    if (jobs > 0) {
        const std::string name = context.media.currentJobName();
        ImGui::Text("Preparing %s (%.0f%%)%s", name.c_str(), context.media.currentJobProgress() * 100.0f, jobs > 1 ? ", more waiting" : "");
    }

    // --- Regions
    const float headerWidth = 170.0f * scale;
    const float rulerHeight = ImGui::GetFontSize() * 1.5f;
    const float trackHeight = 96.0f * scale;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x <= headerWidth + 10.0f || avail.y <= rulerHeight + 10.0f) return;

    const ImVec2 canvasMin(origin.x + headerWidth, origin.y + rulerHeight);
    const ImVec2 canvasMax(origin.x + avail.x, origin.y + avail.y);
    const float canvasWidth = canvasMax.x - canvasMin.x;
    const float canvasHeight = canvasMax.y - canvasMin.y;
    const float totalHeight = trackHeight * static_cast<float>(project->tracks.size());
    state.scrollY = std::clamp(state.scrollY, 0.0f, std::max(0.0f, totalHeight - canvasHeight));

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImGuiIO& io = ImGui::GetIO();

    // --- Keep the playhead in view while playing
    const double playSeconds = context.engine.sampleRate > 0.0 ? static_cast<double>(context.engine.positionFrames) / context.engine.sampleRate : 0.0;
    if (state.followPlayhead && context.engine.playing && state.drag == TimelineState::Drag::None) {
        const double visible = canvasWidth / state.view.pixelsPerSecond();
        if (playSeconds < state.view.scrollSeconds() || playSeconds > state.view.scrollSeconds() + visible * 0.95) state.view.setScroll(playSeconds - visible * 0.05);
    }

    // --- Mouse helpers
    const ImVec2 mouse = io.MousePos;
    const bool inCanvas = mouse.x >= canvasMin.x && mouse.x < canvasMax.x && mouse.y >= canvasMin.y && mouse.y < canvasMax.y;
    const bool inRuler = mouse.x >= canvasMin.x && mouse.x < canvasMax.x && mouse.y >= origin.y && mouse.y < canvasMin.y;
    const Time mouseTime = state.view.pixelToTime(mouse.x - canvasMin.x);
    const auto trackAtMouse = [&]() -> Id {
        const int index = static_cast<int>((mouse.y - canvasMin.y + state.scrollY) / trackHeight);
        if (mouse.y < canvasMin.y || index < 0 || index >= static_cast<int>(project->tracks.size())) return kNoId;
        return project->tracks[static_cast<std::size_t>(index)].id;
    };

    state.dropInside = (inCanvas || inRuler) && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    state.dropTrack = inCanvas ? trackAtMouse() : kNoId;
    state.dropTime = snapped(state, mouseTime, bpm);

    // --- Background and clipping
    draw->PushClipRect(canvasMin, canvasMax, true);
    draw->AddRectFilled(canvasMin, canvasMax, ImGui::GetColorU32(ImGuiCol_ChildBg, 1.0f));
    drawBarLines(draw, canvasMin, canvasMax, state.view, bpm);

    // --- Track lanes and clips (offscreen tracks are skipped entirely)
    const float handle = 6.0f * scale;
    Id hoveredClip = kNoId;
    TimelineState::Drag hoverMode = TimelineState::Drag::None;

    for (std::size_t i = 0; i < project->tracks.size(); ++i) {
        const Track& track = project->tracks[i];
        const float laneTop = canvasMin.y + trackHeight * static_cast<float>(i) - state.scrollY;
        const float laneBottom = laneTop + trackHeight;
        if (laneBottom < canvasMin.y || laneTop > canvasMax.y) continue;
        draw->AddLine(ImVec2(canvasMin.x, laneBottom - 1.0f), ImVec2(canvasMax.x, laneBottom - 1.0f), ImGui::GetColorU32(ImGuiCol_Border));

        for (const Clip& clip : track.clips) {
            // A clip being dragged is drawn at its preview position further down.
            const bool dragged = state.dragClip == clip.id && state.drag != TimelineState::Drag::None;
            const float x0 = canvasMin.x + static_cast<float>(state.view.timeToPixel(clip.start));
            const float x1 = canvasMin.x + static_cast<float>(state.view.timeToPixel(clip.start + clip.length));
            if (x1 < canvasMin.x || x0 > canvasMax.x) continue;

            const ImVec2 min(x0, laneTop + 1.0f);
            const ImVec2 max(std::max(x1, x0 + 2.0f), laneBottom - 2.0f);
            const bool selected = state.selectedClip == clip.id;
            const ImU32 fill = ImGui::GetColorU32(ImGuiCol_Header, dragged ? 0.35f : 1.0f);
            draw->AddRectFilled(min, max, fill, 2.0f);
            const model::MediaItem* item = project->findMedia(clip.mediaId);
            if (item) {
                if (const auto peaks = context.media.peaksFor(item->path)) {
                    drawWaveform(draw, *peaks, clip, state.view, rate, ImVec2(min.x, min.y + ImGui::GetFontSize()), max, canvasMin.x, canvasMax.x, ImGui::GetColorU32(ImGuiCol_Text, dragged ? 0.35f : 0.85f));
                }
                draw->PushClipRect(ImVec2(std::max(min.x, canvasMin.x), min.y), ImVec2(std::min(max.x, canvasMax.x), max.y), true);
                const std::u8string name = item->path.filename().u8string();
                draw->AddText(ImVec2(std::max(min.x, canvasMin.x) + 3.0f, min.y), ImGui::GetColorU32(ImGuiCol_Text), reinterpret_cast<const char*>(name.c_str()));
                draw->PopClipRect();
            }
            // Selection shows by a thick outline, not by colour alone.
            draw->AddRect(min, max, ImGui::GetColorU32(selected ? ImGuiCol_Text : ImGuiCol_Border), 2.0f, selected ? 3.0f : 1.0f);

            if (inCanvas && mouse.x >= min.x && mouse.x < max.x && mouse.y >= min.y && mouse.y < max.y) {
                hoveredClip = clip.id;
                if (mouse.x < min.x + handle) hoverMode = TimelineState::Drag::TrimStart;
                else if (mouse.x > max.x - handle) hoverMode = TimelineState::Drag::TrimEnd;
                else hoverMode = TimelineState::Drag::Move;
            }
        }
    }

    // --- Drag preview
    if (state.dragClip != kNoId && state.drag != TimelineState::Drag::None) {
        int laneIndex = -1;
        for (std::size_t i = 0; i < project->tracks.size(); ++i) {
            if (project->tracks[i].id == state.dragPreviewTrack) laneIndex = static_cast<int>(i);
        }
        if (laneIndex >= 0) {
            const float laneTop = canvasMin.y + trackHeight * static_cast<float>(laneIndex) - state.scrollY;
            const float x0 = canvasMin.x + static_cast<float>(state.view.timeToPixel(state.dragPreviewStart));
            const float x1 = canvasMin.x + static_cast<float>(state.view.timeToPixel(state.dragPreviewEnd));
            draw->AddRectFilled(ImVec2(x0, laneTop + 1.0f), ImVec2(x1, laneTop + trackHeight - 2.0f), ImGui::GetColorU32(ImGuiCol_HeaderActive, 0.55f), 2.0f);
            draw->AddRect(ImVec2(x0, laneTop + 1.0f), ImVec2(x1, laneTop + trackHeight - 2.0f), ImGui::GetColorU32(ImGuiCol_Text), 2.0f, 2.0f);
        }
    }

    // --- Loop region and playhead, drawn over the lanes
    if (project->loopEnd > project->loopStart) {
        const float x0 = canvasMin.x + static_cast<float>(state.view.timeToPixel(project->loopStart));
        const float x1 = canvasMin.x + static_cast<float>(state.view.timeToPixel(project->loopEnd));
        draw->AddRectFilled(ImVec2(x0, canvasMin.y), ImVec2(x1, canvasMax.y), ImGui::GetColorU32(ImGuiCol_PlotHistogram, project->loopEnabled ? 0.10f : 0.04f));
    }
    const auto playX = static_cast<float>((playSeconds - state.view.scrollSeconds()) * state.view.pixelsPerSecond());
    if (playX >= 0.0f && playX <= canvasWidth) {
        draw->AddLine(ImVec2(canvasMin.x + playX, canvasMin.y), ImVec2(canvasMin.x + playX, canvasMax.y), ImGui::GetColorU32(ImGuiCol_PlotLinesHovered), 2.0f);
    }
    if (project->tracks.empty()) {
        const char* hint = "Drop an audio file here, or use File > Import audio.";
        const ImVec2 size = ImGui::CalcTextSize(hint);
        draw->AddText(ImVec2(canvasMin.x + (canvasWidth - size.x) * 0.5f, canvasMin.y + (canvasHeight - size.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), hint);
    }
    draw->PopClipRect();

    // --- Ruler
    const ImVec2 rulerMin(canvasMin.x, origin.y);
    const ImVec2 rulerMax(canvasMax.x, canvasMin.y);
    draw->PushClipRect(rulerMin, rulerMax, true);
    drawRuler(draw, rulerMin, rulerMax, state.view, bpm);
    if (project->loopEnd > project->loopStart) {
        const float x0 = canvasMin.x + static_cast<float>(state.view.timeToPixel(project->loopStart));
        const float x1 = canvasMin.x + static_cast<float>(state.view.timeToPixel(project->loopEnd));
        draw->AddRectFilled(ImVec2(x0, rulerMax.y - 4.0f * scale), ImVec2(x1, rulerMax.y), ImGui::GetColorU32(ImGuiCol_PlotHistogram));
    }
    if (playX >= 0.0f && playX <= canvasWidth) {
        const float x = canvasMin.x + playX;
        draw->AddTriangleFilled(ImVec2(x - 5.0f * scale, rulerMin.y), ImVec2(x + 5.0f * scale, rulerMin.y), ImVec2(x, rulerMax.y), ImGui::GetColorU32(ImGuiCol_PlotLinesHovered));
    }
    draw->PopClipRect();

    // --- Track headers (real widgets, so they handle their own input)
    draw->PushClipRect(ImVec2(origin.x, canvasMin.y), ImVec2(canvasMin.x, canvasMax.y), true);
    ImGui::PushClipRect(ImVec2(origin.x, canvasMin.y), ImVec2(canvasMin.x, canvasMax.y), true);
    for (std::size_t i = 0; i < project->tracks.size(); ++i) {
        const float top = canvasMin.y + trackHeight * static_cast<float>(i) - state.scrollY;
        if (top + trackHeight < canvasMin.y || top > canvasMax.y) continue;
        drawTrackHeader(state, history, project->tracks[i], ImVec2(origin.x, top), headerWidth, trackHeight);
        draw->AddLine(ImVec2(origin.x, top + trackHeight - 1.0f), ImVec2(canvasMin.x, top + trackHeight - 1.0f), ImGui::GetColorU32(ImGuiCol_Border));
    }
    ImGui::PopClipRect();
    draw->PopClipRect();

    // --- Canvas input
    ImGui::SetCursorScreenPos(rulerMin);
    ImGui::InvisibleButton("##timeline", ImVec2(canvasWidth, canvasMax.y - rulerMin.y), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool hovered = ImGui::IsItemHovered();

    if (hovered) {
        if (hoverMode == TimelineState::Drag::TrimStart || hoverMode == TimelineState::Drag::TrimEnd) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (io.MouseWheel != 0.0f) {
            if (io.KeyCtrl) {
                state.view.zoomAround(mouse.x - canvasMin.x, std::pow(1.25, static_cast<double>(io.MouseWheel)));
            } else if (io.KeyShift) {
                state.scrollY -= io.MouseWheel * trackHeight * 0.5f;
            } else {
                state.view.setScroll(state.view.scrollSeconds() - static_cast<double>(io.MouseWheel) * 80.0 * static_cast<double>(scale) / state.view.pixelsPerSecond());
            }
        }
        if (io.MouseWheelH != 0.0f) state.view.setScroll(state.view.scrollSeconds() + static_cast<double>(io.MouseWheelH) * 80.0 * static_cast<double>(scale) / state.view.pixelsPerSecond());
    }

    // Start a gesture.
    if (ImGui::IsItemActivated() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (inRuler) {
            if (io.KeyShift) {
                state.drag = TimelineState::Drag::Loop;
                state.dragStartTime = snapped(state, mouseTime, bpm);
                state.dragPreviewStart = state.dragPreviewEnd = state.dragStartTime;
            } else {
                state.drag = TimelineState::Drag::Scrub;
            }
        } else if (hoveredClip != kNoId) {
            Id trackId = kNoId;
            const Clip* clip = project->findClip(hoveredClip, &trackId);
            state.selectedClip = hoveredClip;
            state.selectedTrack = trackId;
            state.drag = hoverMode;
            state.dragClip = hoveredClip;
            state.dragGrabOffset = mouseTime - clip->start;
            state.dragPreviewStart = clip->start;
            state.dragPreviewEnd = clip->start + clip->length;
            state.dragPreviewTrack = trackId;
        } else {
            state.selectedClip = kNoId;
            if (const Id track = trackAtMouse(); track != kNoId) state.selectedTrack = track;
        }
    }

    // Continue a gesture. Nothing changes the song until the mouse is released.
    if (state.drag != TimelineState::Drag::None && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const Clip* clip = state.dragClip != kNoId ? project->findClip(state.dragClip) : nullptr;
        switch (state.drag) {
        case TimelineState::Drag::Scrub:
            if (rate > 0.0) context.post({engine::EngineCommandType::Seek, static_cast<double>(model::timeToFrames(mouseTime, rate))});
            break;
        case TimelineState::Drag::Loop: {
            const Time now = snapped(state, mouseTime, bpm);
            state.dragPreviewStart = std::min(state.dragStartTime, now);
            state.dragPreviewEnd = std::max(state.dragStartTime, now);
            break;
        }
        case TimelineState::Drag::Move:
            if (clip) {
                const Time start = std::max<Time>(0, snapped(state, mouseTime - state.dragGrabOffset, bpm));
                state.dragPreviewStart = start;
                state.dragPreviewEnd = start + clip->length;
                if (const Id track = trackAtMouse(); track != kNoId) state.dragPreviewTrack = track;
            }
            break;
        case TimelineState::Drag::TrimStart:
            if (clip) {
                Project trial = *project;
                commands::trimClipStart(trial, clip->id, snapped(state, mouseTime, bpm));
                const Clip* result = trial.findClip(clip->id);
                state.dragPreviewStart = result->start;
                state.dragPreviewEnd = result->start + result->length;
            }
            break;
        case TimelineState::Drag::TrimEnd:
            if (clip) {
                Project trial = *project;
                commands::trimClipEnd(trial, clip->id, snapped(state, mouseTime, bpm));
                const Clip* result = trial.findClip(clip->id);
                state.dragPreviewStart = result->start;
                state.dragPreviewEnd = result->start + result->length;
            }
            break;
        case TimelineState::Drag::None:
            break;
        }
    }

    // Finish a gesture: one undo step.
    if (state.drag != TimelineState::Drag::None && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const Id clipId = state.dragClip;
        const Time start = state.dragPreviewStart;
        const Time end = state.dragPreviewEnd;
        const Id targetTrack = state.dragPreviewTrack;
        switch (state.drag) {
        case TimelineState::Drag::Move: {
            const Clip* clip = project->findClip(clipId);
            Id currentTrack = kNoId;
            project->findClip(clipId, &currentTrack);
            if (clip && (clip->start != start || currentTrack != targetTrack)) {
                history.apply("Move clip", [&](Project& p) { return commands::moveClip(p, clipId, targetTrack, start); });
            }
            break;
        }
        case TimelineState::Drag::TrimStart: {
            const Clip* clip = project->findClip(clipId);
            if (clip && clip->start != start) history.apply("Trim clip", [&](Project& p) { return commands::trimClipStart(p, clipId, start); });
            break;
        }
        case TimelineState::Drag::TrimEnd: {
            const Clip* clip = project->findClip(clipId);
            if (clip && clip->start + clip->length != end) history.apply("Trim clip", [&](Project& p) { return commands::trimClipEnd(p, clipId, end); });
            break;
        }
        case TimelineState::Drag::Loop:
            if (end > start) history.apply("Set loop", [&](Project& p) { return commands::setLoop(p, true, start, end); });
            break;
        case TimelineState::Drag::Scrub:
        case TimelineState::Drag::None:
            break;
        }
        state.drag = TimelineState::Drag::None;
        state.dragClip = kNoId;
    }

    // Keyboard, only when this panel has focus and nobody is typing.
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !io.WantTextInput) {
        const Time playTime = model::secondsToTime(playSeconds);
        if (state.selectedClip != kNoId) {
            const Id clipId = state.selectedClip;
            if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
                history.apply("Delete clip", [&](Project& p) { return commands::deleteClip(p, clipId); });
                state.selectedClip = kNoId;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_S, false) && !io.KeyCtrl) {
                Id right = kNoId;
                history.apply("Split clip", [&](Project& p) {
                    right = commands::splitClip(p, clipId, playTime);
                    return right != kNoId;
                });
            }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
                Id copy = kNoId;
                history.apply("Duplicate clip", [&](Project& p) {
                    copy = commands::duplicateClip(p, clipId);
                    return copy != kNoId;
                });
                if (copy != kNoId) state.selectedClip = copy;
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_L, false) && project->loopEnd > project->loopStart) {
            const bool on = !project->loopEnabled;
            history.apply("Toggle loop", [&](Project& p) { return commands::setLoop(p, on, p.loopStart, p.loopEnd); });
        }
    }

    // A clip the user selected may have been removed by undo.
    if (state.selectedClip != kNoId && !history.current()->findClip(state.selectedClip)) state.selectedClip = kNoId;
    if (state.selectedTrack != kNoId && !history.current()->findTrack(state.selectedTrack)) state.selectedTrack = kNoId;
}

void drawInspectorPanel(TimelineState& state, const TimelineContext& context) {
    ProjectHistory& history = context.history;
    const auto project = history.current();

    if (const Clip* clip = state.selectedClip != kNoId ? project->findClip(state.selectedClip) : nullptr) {
        const Id clipId = clip->id;
        ImGui::SeparatorText("Clip");
        if (const model::MediaItem* item = project->findMedia(clip->mediaId)) {
            const std::u8string name = item->path.filename().u8string();
            ImGui::TextWrapped("%s", reinterpret_cast<const char*>(name.c_str()));
            ImGui::Text("%.0f Hz, %s", item->sourceSampleRate, item->channels == 1 ? "mono" : "stereo");
        }
        ImGui::Text("Starts at %.3f s", model::timeToSeconds(clip->start));
        ImGui::Text("Length %.3f s", model::timeToSeconds(clip->length));
        ImGui::Text("From %.3f s in the file", model::timeToSeconds(clip->sourceOffset));
        float db = amplitudeToDb(clip->gain);
        if (ImGui::SliderFloat("Clip gain", &db, -60.0f, 12.0f, db <= -60.0f ? "-inf dB" : "%.1f dB")) {
            const float gain = dbToAmplitude(db);
            history.applyGrouped("Clip gain", [&](Project& p) { return commands::setClipGain(p, clipId, gain); });
        }
        if (ImGui::IsItemDeactivated()) history.endGroup();
        ImGui::Spacing();
        ImGui::TextDisabled("S splits at the playhead. Ctrl+D duplicates. Delete removes the clip, not the file.");
        return;
    }
    if (const Track* track = state.selectedTrack != kNoId ? project->findTrack(state.selectedTrack) : nullptr) {
        const Id trackId = track->id;
        ImGui::SeparatorText("Track");
        ImGui::TextUnformatted(track->name.c_str());
        ImGui::Text("%zu clips", track->clips.size());
        if (ImGui::Button("Remove track")) {
            history.apply("Remove track", [&](Project& p) { return commands::removeTrack(p, trackId); });
            state.selectedTrack = kNoId;
        }
        ImGui::TextDisabled("Audio files are never deleted.");
        return;
    }
    ImGui::TextDisabled("Select a clip or track.");
}

} // namespace ldaw::ui
