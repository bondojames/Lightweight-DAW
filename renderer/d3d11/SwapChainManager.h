// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

namespace ldaw::renderer {

enum class PresentResult { Ok, Occluded, DeviceLost };

// Swap chain for the primary window. Detached panels get theirs from the official
// ImGui DX11 backend (spec section 4.1A).
class SwapChainManager {
public:
    bool create(ID3D11Device* device, HWND hwnd);
    void destroy();
    bool resize(ID3D11Device* device, unsigned width, unsigned height);

    void bindAndClear(ID3D11DeviceContext* context, const float clearColor[4]);
    PresentResult present(bool vsync);
    // Asks DXGI whether the window is visible, without showing anything.
    PresentResult probe();

private:
    bool createTarget(ID3D11Device* device);
    static PresentResult classify(HRESULT hr);

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target_;
};

} // namespace ldaw::renderer
