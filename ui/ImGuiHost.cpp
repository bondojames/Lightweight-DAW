// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>

#include "ui/ImGuiHost.h"

#include "core/logging/Log.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <filesystem>
#include <string>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace ldaw::ui {
namespace {

bool g_backendsUp = false;

void loadFonts() {
    // Segoe UI ships with Windows 11, so no font is redistributed (ADR-007). ImGui
    // rasterizes glyphs on demand, which also means no atlas rebuild on DPI change.
    wchar_t windowsDir[MAX_PATH] = {};
    GetWindowsDirectoryW(windowsDir, MAX_PATH);
    const std::filesystem::path font = std::filesystem::path(windowsDir) / L"Fonts" / L"segoeui.ttf";
    ImGuiIO& io = ImGui::GetIO();
    std::error_code ec;
    if (!std::filesystem::exists(font, ec) || !io.Fonts->AddFontFromFileTTF(font.string().c_str())) {
        log::error("Segoe UI not found; using the built-in ImGui font");
        io.Fonts->AddFontDefaultVector();
    }
    ImGui::GetStyle().FontSizeBase = 16.0f;
}

} // namespace

bool ImGuiHost::initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // WorkspaceManager owns layout persistence.
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
    io.ConfigDpiScaleFonts = true;
    io.ConfigDpiScaleViewports = true;

    hwnd_ = hwnd;
    initialized_ = true;
    if (!resumeBackends(device, context)) return false;
    loadFonts();
    return true;
}

void ImGuiHost::shutdown() {
    if (!initialized_) return;
    suspendBackends();
    ImGui::DestroyContext();
    initialized_ = false;
}

void ImGuiHost::suspendBackends() {
    if (!backendsUp_) return;
    g_backendsUp = false; // Messages arriving now must not reach the stopped backend.
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    backendsUp_ = false;
}

bool ImGuiHost::resumeBackends(ID3D11Device* device, ID3D11DeviceContext* context) {
    if (backendsUp_) return true;
    if (!ImGui_ImplWin32_Init(hwnd_)) return false;
    if (!ImGui_ImplDX11_Init(device, context)) {
        ImGui_ImplWin32_Shutdown();
        return false;
    }
    backendsUp_ = true;
    g_backendsUp = true;
    return true;
}

void ImGuiHost::beginFrame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    if (injector_) injector_();
    ImGui::NewFrame();
}

void ImGuiHost::endFrameAndBuildDrawData() { ImGui::Render(); }

void ImGuiHost::renderMainViewport() { ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); }

void ImGuiHost::renderSecondaryViewports() {
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
}

bool ImGuiHost::handleMessage(HWND hwnd, unsigned message, unsigned long long wParam, long long lParam) {
    if (!ImGui::GetCurrentContext() || !g_backendsUp) return false;
    return ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam) != 0;
}

bool ImGuiHost::dockingEnabled() { return (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable) != 0; }

bool ImGuiHost::viewportsEnabled() {
    const ImGuiIO& io = ImGui::GetIO();
    return (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0 && (io.BackendFlags & ImGuiBackendFlags_PlatformHasViewports) != 0 &&
           (io.BackendFlags & ImGuiBackendFlags_RendererHasViewports) != 0;
}

int ImGuiHost::countDrawCalls() {
    int total = 0;
    for (const ImGuiViewport* viewport : ImGui::GetPlatformIO().Viewports) {
        if (!viewport->DrawData) continue;
        for (const ImDrawList* list : viewport->DrawData->CmdLists) total += list->CmdBuffer.Size;
    }
    return total;
}

int ImGuiHost::countVertices() {
    int total = 0;
    for (const ImGuiViewport* viewport : ImGui::GetPlatformIO().Viewports) {
        if (viewport->DrawData) total += viewport->DrawData->TotalVtxCount;
    }
    return total;
}

} // namespace ldaw::ui
