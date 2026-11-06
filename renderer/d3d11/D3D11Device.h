// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <d3d11.h>
#include <wrl/client.h>

#include <string>

namespace ldaw::renderer {

// Owns the D3D11 device. UI rendering only: audio never depends on it (spec section 15).
class D3D11Device {
public:
    // Hardware by default. WARP is used only when asked for (--warp), so software
    // rendering can never silently replace the GPU in a benchmark.
    bool create(bool useWarp, std::string& errorOut);
    void destroy();

    ID3D11Device* device() const { return device_.Get(); }
    ID3D11DeviceContext* context() const { return context_.Get(); }
    const std::string& adapterName() const { return adapterName_; }
    const char* featureLevelName() const;
    bool isWarp() const { return warp_; }

private:
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    D3D_FEATURE_LEVEL featureLevel_ = D3D_FEATURE_LEVEL_10_0;
    std::string adapterName_;
    bool warp_ = false;
};

} // namespace ldaw::renderer
