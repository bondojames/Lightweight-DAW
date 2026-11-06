// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/Theme.h"

#include <imgui.h>

namespace ldaw::ui {
namespace {

void applyHighContrast(ImGuiStyle& style) {
    ImGui::StyleColorsDark(&style);
    const ImVec4 black(0.0f, 0.0f, 0.0f, 1.0f);
    const ImVec4 white(1.0f, 1.0f, 1.0f, 1.0f);
    const ImVec4 yellow(1.0f, 1.0f, 0.0f, 1.0f);
    const ImVec4 cyan(0.0f, 1.0f, 1.0f, 1.0f);
    const ImVec4 dark(0.15f, 0.15f, 0.15f, 1.0f);
    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = white;
    c[ImGuiCol_TextDisabled] = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
    c[ImGuiCol_WindowBg] = c[ImGuiCol_ChildBg] = c[ImGuiCol_PopupBg] = c[ImGuiCol_MenuBarBg] = black;
    c[ImGuiCol_DockingEmptyBg] = black;
    c[ImGuiCol_Border] = white;
    c[ImGuiCol_FrameBg] = dark;
    c[ImGuiCol_FrameBgHovered] = c[ImGuiCol_ButtonHovered] = c[ImGuiCol_HeaderHovered] = ImVec4(0.0f, 0.35f, 0.55f, 1.0f);
    c[ImGuiCol_FrameBgActive] = c[ImGuiCol_ButtonActive] = c[ImGuiCol_HeaderActive] = ImVec4(0.0f, 0.5f, 0.75f, 1.0f);
    c[ImGuiCol_Button] = c[ImGuiCol_Header] = dark;
    c[ImGuiCol_TitleBg] = c[ImGuiCol_TitleBgCollapsed] = dark;
    c[ImGuiCol_TitleBgActive] = ImVec4(0.0f, 0.3f, 0.5f, 1.0f);
    c[ImGuiCol_CheckMark] = c[ImGuiCol_SliderGrab] = yellow;
    c[ImGuiCol_DockingPreview] = yellow;
    c[ImGuiCol_NavCursor] = cyan;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 1.0f;
}

} // namespace

void applyTheme(int themeId, float dpiScale) {
    ImGuiStyle style;
    switch (themeId) {
    case 1: ImGui::StyleColorsLight(&style); break;
    case 2: applyHighContrast(style); break;
    default: ImGui::StyleColorsDark(&style); break;
    }
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.FontSizeBase = ImGui::GetStyle().FontSizeBase;
    style.ScaleAllSizes(dpiScale);
    style.FontScaleDpi = dpiScale;
    // Detached panels are native windows, so they must be square and fully opaque.
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    ImGui::GetStyle() = style;
}

} // namespace ldaw::ui
