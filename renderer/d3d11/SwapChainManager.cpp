// SPDX-License-Identifier: GPL-3.0-or-later
#include "renderer/d3d11/SwapChainManager.h"

using Microsoft::WRL::ComPtr;

namespace ldaw::renderer {

bool SwapChainManager::create(ID3D11Device* device, HWND hwnd) {
    destroy();
    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory2> factory;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgiDevice)))) return false;
    if (FAILED(dxgiDevice->GetAdapter(&adapter))) return false;
    if (FAILED(adapter->GetParent(IID_PPV_ARGS(&factory)))) return false;

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    if (FAILED(factory->CreateSwapChainForHwnd(device, hwnd, &desc, nullptr, nullptr, &swapChain_))) return false;
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
    return createTarget(device);
}

void SwapChainManager::destroy() {
    target_.Reset();
    swapChain_.Reset();
}

bool SwapChainManager::createTarget(ID3D11Device* device) {
    ComPtr<ID3D11Texture2D> backBuffer;
    if (FAILED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) return false;
    return SUCCEEDED(device->CreateRenderTargetView(backBuffer.Get(), nullptr, &target_));
}

bool SwapChainManager::resize(ID3D11Device* device, unsigned width, unsigned height) {
    target_.Reset();
    // Every reference to the back buffer must be gone first, including the context's.
    ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(&context);
    context->OMSetRenderTargets(0, nullptr, nullptr);
    context->Flush();
    if (FAILED(swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0))) return false;
    return createTarget(device);
}

void SwapChainManager::bindAndClear(ID3D11DeviceContext* context, const float clearColor[4]) {
    ID3D11RenderTargetView* targets[] = {target_.Get()};
    context->OMSetRenderTargets(1, targets, nullptr);
    context->ClearRenderTargetView(target_.Get(), clearColor);
}

PresentResult SwapChainManager::classify(HRESULT hr) {
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) return PresentResult::DeviceLost;
    if (hr == DXGI_STATUS_OCCLUDED) return PresentResult::Occluded;
    return PresentResult::Ok;
}

PresentResult SwapChainManager::present(bool vsync) { return classify(swapChain_->Present(vsync ? 1 : 0, 0)); }

PresentResult SwapChainManager::probe() { return classify(swapChain_->Present(0, DXGI_PRESENT_TEST)); }

} // namespace ldaw::renderer
