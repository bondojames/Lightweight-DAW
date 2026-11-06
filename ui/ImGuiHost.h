// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <d3d11.h>

#include <functional>

namespace ldaw::ui {

// The single Dear ImGui context and its official Win32 and DX11 backends.
class ImGuiHost {
public:
    bool initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context);
    void shutdown();
    bool isInitialized() const { return initialized_; }

    // For GPU loss: both backends restart while the context and all UI state stay
    // alive. Both must restart, because the DX11 backend shutdown also destroys the
    // Win32 backend data on every viewport. Detached panels get new native windows
    // on the next frame.
    void suspendBackends();
    bool resumeBackends(ID3D11Device* device, ID3D11DeviceContext* context);

    // Test only: runs after the platform backend has fed in the real mouse, so input
    // sent from here is what the frame actually sees.
    void setInputInjector(std::function<void()> injector) { injector_ = std::move(injector); }

    void beginFrame();
    void endFrameAndBuildDrawData();
    void renderMainViewport();
    void renderSecondaryViewports();

    static bool handleMessage(HWND hwnd, unsigned message, unsigned long long wParam, long long lParam);
    static bool dockingEnabled();
    static bool viewportsEnabled();
    static int countDrawCalls();
    static int countVertices();

private:
    std::function<void()> injector_;
    bool initialized_ = false;
    HWND hwnd_ = nullptr;
    bool backendsUp_ = false;
};

} // namespace ldaw::ui
