// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "app/CommandLine.h"
#include "core/settings/KeyValueStore.h"
#include "platform/win32/ProcessMetrics.h"
#include "platform/win32/WindowHost.h"
#include "renderer/d3d11/D3D11Device.h"
#include "renderer/d3d11/SwapChainManager.h"
#include "ui/FrameScheduler.h"
#include "ui/ImGuiHost.h"
#include "ui/WorkspaceManager.h"
#include "ui/panels/Panels.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <vector>

namespace ldaw {

class Application {
public:
    explicit Application(CommandLine args);
    ~Application();
    int run(int showCommand);

private:
    bool initialize(int showCommand);
    void shutdown();
    void loadPreferences();
    void savePreferences();
    IntRect initialWindowBounds() const;

    double now() const;
    void updateFrameState(double time);
    void renderFrame(double time);
    void applyActions(const ui::UiActions& actions);
    void runPendingDialog();
    void sampleMetrics();
    void recoverDevice(double time);
    void handleDrops();

    void beginProfileWindow();
    bool finishProfile();

    CommandLine args_;
    std::filesystem::path dataDir_;
    KeyValueStore preferences_;

    platform::WindowHost window_;
    renderer::D3D11Device device_;
    renderer::SwapChainManager swapChain_;
    ui::ImGuiHost imgui_;
    ui::WorkspaceManager workspaces_;
    FrameScheduler scheduler_;
    platform::ProcessMetrics metrics_;
    ui::UiState uiState_;

    std::chrono::steady_clock::time_point start_;
    double nextMetricsAt_ = 0.0;
    double nextDeviceRetryAt_ = 0.0;
    bool deviceLost_ = false;
    bool exitRequested_ = false;
    bool insideModalTick_ = false;
    bool simulateDeviceLoss_ = false;
    bool occluded_ = false;
    bool wasMinimized_ = false;
    float appliedDpiScale_ = 0.0f;
    int themeId_ = 0;
    ui::DialogRequest pendingDialog_ = ui::DialogRequest::None;

    // Per-second frame statistics.
    double buildMsSum_ = 0.0, renderMsSum_ = 0.0, presentMsSum_ = 0.0;
    int framesSinceSample_ = 0;
    double lastSampleTime_ = 0.0;
    std::uint64_t framesTotal_ = 0;

    // Measurement window for --profile.
    std::optional<platform::ProcessMetrics> profileMetrics_; // Separate baseline; sampled only at the end.
    bool profileMeasuring_ = false;
    double profileStart_ = 0.0;
    double profileEndAt_ = 0.0;
    std::uint64_t profileFramesAtStart_ = 0;
    double profileBuildMs_ = 0.0, profileRenderMs_ = 0.0, profilePresentMs_ = 0.0;
    std::uint64_t profileMaxWorkingSet_ = 0;
    int profileMaxViewports_ = 0;
    int profileDrawCalls_ = 0;

    // --ui-self-test: scripted checks of docking, workspaces and recovery (UiSelfTest.cpp).
    void buildSelfTest();
    void stepSelfTest(double time);
    struct SelfTestStep {
        const char* name;
        std::function<void()> act;
        std::function<bool()> check;
    };
    std::vector<SelfTestStep> selfTestSteps_;
    std::size_t selfTestIndex_ = 0;
    std::uint64_t selfTestCheckAtFrame_ = 0;
    bool selfTestActed_ = false;
    int selfTestFailures_ = 0;
    bool selfTestDone_ = false;
};

} // namespace ldaw
