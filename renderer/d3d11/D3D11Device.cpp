// SPDX-License-Identifier: GPL-3.0-or-later
#include "renderer/d3d11/D3D11Device.h"

#include <dxgi.h>

#include <cstdio>

using Microsoft::WRL::ComPtr;

namespace ldaw::renderer {

bool D3D11Device::create(bool useWarp, std::string& errorOut) {
    destroy();
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    const HRESULT hr = D3D11CreateDevice(nullptr, useWarp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                         levels, static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION, &device_,
                                         &featureLevel_, &context_);
    if (FAILED(hr)) {
        char message[128];
        std::snprintf(message, sizeof(message), "Direct3D 11 device creation failed (HRESULT 0x%08lX).", static_cast<unsigned long>(hr));
        errorOut = message;
        return false;
    }
    warp_ = useWarp;

    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC desc{};
    if (SUCCEEDED(device_.As(&dxgiDevice)) && SUCCEEDED(dxgiDevice->GetAdapter(&adapter)) && SUCCEEDED(adapter->GetDesc(&desc))) {
        char name[256] = {};
        WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, sizeof(name), nullptr, nullptr);
        adapterName_ = name;
    }
    return true;
}

void D3D11Device::destroy() {
    if (context_) {
        context_->ClearState();
        context_->Flush();
    }
    context_.Reset();
    device_.Reset();
}

const char* D3D11Device::featureLevelName() const {
    switch (featureLevel_) {
    case D3D_FEATURE_LEVEL_11_0: return "11_0";
    case D3D_FEATURE_LEVEL_10_1: return "10_1";
    case D3D_FEATURE_LEVEL_10_0: return "10_0";
    default: return "unknown";
    }
}

} // namespace ldaw::renderer
