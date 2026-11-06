// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>

#include "app/Application.h"

#include "BuildInfo.h"
#include "core/logging/Log.h"
#include "engine/device/WasapiBackend.h"
#include "engine/render/OfflineRender.h"
#include "platform/win32/CrashHandler.h"
#include "platform/win32/NativeDialogs.h"
#include "platform/win32/ShellIntegration.h"
#include "ui/Theme.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <fstream>

namespace ldaw {
namespace {

constexpr float kClearColor[4] = {0.06f, 0.06f, 0.07f, 1.0f};
constexpr double kMetricsIntervalSeconds = 1.0;
constexpr double kDeviceRetrySeconds = 2.0;
constexpr std::size_t kMaxDroppedFilesShown = 16;

} // namespace

Application::Application(CommandLine args) : args_(std::move(args)) {
    start_ = std::chrono::steady_clock::now();
}

Application::~Application() = default;

double Application::now() const {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
}

void Application::loadPreferences() {
    preferences_.loadFile(dataDir_ / L"settings.ini");
    themeId_ = std::clamp(preferences_.getInt("ui.theme", 0), 0, 2);

    FrameScheduler::Config config;
    const int activeHz = preferences_.getInt("ui.activeHz", 60);
    config.activeHz = (activeHz == 30) ? 30.0 : 60.0;
    const int idleHz = preferences_.getInt("ui.idleHz", 10);
    config.idleHz = (idleHz == 5 || idleHz == 15) ? static_cast<double>(idleHz) : 10.0;
    scheduler_.setConfig(config);

    uiState_.themeId = themeId_;
    uiState_.activeHz = static_cast<int>(config.activeHz);
    uiState_.idleHz = static_cast<int>(config.idleHz);

    uiState_.audioDeviceId = preferences_.getString("audio.device", "");
    uiState_.audioRate = preferences_.getInt("audio.rate", 0);
    uiState_.audioFrames = std::clamp(preferences_.getInt("audio.frames", 256), 16, 8192);
}

void Application::savePreferences() {
    preferences_.setInt("ui.theme", themeId_);
    preferences_.setInt("ui.activeHz", static_cast<int>(scheduler_.config().activeHz));
    preferences_.setInt("ui.idleHz", static_cast<int>(scheduler_.config().idleHz));
    preferences_.setString("audio.device", uiState_.audioDeviceId);
    preferences_.setInt("audio.rate", uiState_.audioRate);
    preferences_.setInt("audio.frames", uiState_.audioFrames);
    if (window_.hwnd()) {
        const IntRect bounds = window_.restoredBounds();
        preferences_.setInt("window.x", bounds.x);
        preferences_.setInt("window.y", bounds.y);
        preferences_.setInt("window.w", bounds.w);
        preferences_.setInt("window.h", bounds.h);
        preferences_.setInt("window.maximized", window_.isMaximized() ? 1 : 0);
    }
    if (!preferences_.saveFile(dataDir_ / L"settings.ini")) log::error("Could not save settings.ini");
}

IntRect Application::initialWindowBounds() const {
    const auto monitors = platform::WindowHost::monitorWorkAreas();
    const float scale = static_cast<float>(GetDpiForSystem()) / 96.0f;
    IntRect fallback{100, 100, static_cast<int>(1280 * scale), static_cast<int>(800 * scale)};
    if (!monitors.empty()) {
        const IntRect& m = monitors.front();
        fallback.w = std::min(fallback.w, m.w);
        fallback.h = std::min(fallback.h, m.h);
        fallback.x = m.x + (m.w - fallback.w) / 2;
        fallback.y = m.y + (m.h - fallback.h) / 2;
    }
    const IntRect saved{preferences_.getInt("window.x", fallback.x), preferences_.getInt("window.y", fallback.y),
                        preferences_.getInt("window.w", fallback.w), preferences_.getInt("window.h", fallback.h)};
    return (saved.w > 0 && saved.h > 0) ? rescueRect(saved, monitors) : fallback;
}

bool Application::initialize(int showCommand) {
    dataDir_ = args_.dataDir ? *args_.dataDir : platform::resolveDataDirectory();
    log::open(dataDir_ / L"logs" / L"app.log");
    platform::installCrashHandler(dataDir_ / L"logs");
    log::info(std::format("LightweightDAW {} ({}) starting; data dir {}", LDAW_VERSION, LDAW_COMMIT,
                          platform::toUtf8(dataDir_.wstring())));

    if (args_.testCrash) {
        volatile int* null = nullptr;
        *null = 1;
    }

    loadPreferences();

    if (!window_.create(L"LightweightDAW", initialWindowBounds(), preferences_.getInt("window.maximized", 0) != 0)) {
        log::error("Could not create the main window");
        return false;
    }

    std::string error;
    if (!device_.create(args_.warp, error) || !swapChain_.create(device_.device(), window_.hwnd())) {
        if (error.empty()) error = "Could not create the Direct3D 11 swap chain.";
        log::error(error);
        MessageBoxW(window_.hwnd(), platform::toWide(error + "\n\nLightweightDAW needs Direct3D 11 to draw its interface.").c_str(),
                    L"LightweightDAW", MB_OK | MB_ICONERROR);
        return false;
    }
    log::info(std::format("Direct3D 11 on '{}', feature level {}{}", device_.adapterName(), device_.featureLevelName(),
                          device_.isWarp() ? " (WARP diagnostic mode)" : ""));

    if (!imgui_.initialize(window_.hwnd(), device_.device(), device_.context())) {
        log::error("Dear ImGui backend initialization failed");
        return false;
    }
    log::info(std::format("Dear ImGui {}; docking {}, viewports {}", ImGui::GetVersion(),
                          ui::ImGuiHost::dockingEnabled() ? "on" : "off", ui::ImGuiHost::viewportsEnabled() ? "on" : "off"));

    appliedDpiScale_ = window_.dpiScale();
    ui::applyTheme(themeId_, appliedDpiScale_);

    window_.setMessageHook([this](HWND hwnd, unsigned message, unsigned long long wParam, long long lParam) {
        if (ui::ImGuiHost::handleMessage(hwnd, message, wParam, lParam)) return true;
        if (message == WM_CLOSE) { // Shut down in order from the main loop, not inside the message.
            exitRequested_ = true;
            return true;
        }
        return false;
    });
    window_.setModalLoopTick([this] {
        if (insideModalTick_ || deviceLost_) return;
        insideModalTick_ = true;
        const double time = now();
        updateFrameState(time);
        scheduler_.noteActivity(time);
        renderFrame(time);
        insideModalTick_ = false;
    });

    audio_ = std::make_unique<engine::AudioService>(std::make_unique<engine::WasapiBackend>(), dataDir_ / L"cache");
    uiState_.history = &history_;
    uiState_.audioService = audio_.get();
    audio_->setProject(history_.current());
    applyAudioConfig();

    const std::string startWorkspace = preferences_.getString("ui.workspace", "production");
    workspaces_.initialize(dataDir_ / L"layouts",
                           startWorkspace == "live" ? ui::Workspace::Live : ui::Workspace::Production,
                           platform::WindowHost::monitorWorkAreas());

    uiState_.adapterName = device_.adapterName();
    uiState_.featureLevel = device_.featureLevelName();
    uiState_.warp = device_.isWarp();
    uiState_.dataDir = platform::toUtf8(dataDir_.wstring());

    window_.show(args_.profile == ProfileMode::Minimized ? SW_SHOWMINNOACTIVE : showCommand);
    // Windows applies the launcher's show mode to the first ShowWindow call and ignores
    // its argument, so the measurement mode must ask a second time.
    if (args_.profile == ProfileMode::Minimized) window_.show(SW_SHOWMINNOACTIVE);
    if (args_.profile == ProfileMode::Detached) {
        workspaces_.panelOpen(ui::PanelId::Diagnostics) = true;
        uiState_.detachDiagnosticsOnce = true;
    }
    if (args_.uiSelfTest) buildSelfTest();
    return true;
}

void Application::shutdown() {
    audio_.reset();
    if (imgui_.isInitialized()) {
        workspaces_.save();
        preferences_.setString("ui.workspace", workspaces_.current() == ui::Workspace::Live ? "live" : "production");
        savePreferences();
        imgui_.shutdown();
    }
    swapChain_.destroy();
    device_.destroy();
    log::info("Clean shutdown");
    log::close();
}

void Application::updateFrameState(double time) {
    unsigned width = 0, height = 0;
    if (!deviceLost_ && window_.takePendingResize(width, height)) {
        if (!swapChain_.resize(device_.device(), width, height)) {
            log::error("Swap chain resize failed; treating as device loss");
            deviceLost_ = true;
            nextDeviceRetryAt_ = time;
        }
    }
    if (window_.takeDisplayChanged()) {
        workspaces_.notifyDisplayChanged(platform::WindowHost::monitorWorkAreas());
        log::info("Display configuration changed");
        scheduler_.noteActivity(time);
    }
    const float scale = window_.dpiScale();
    if (scale != appliedDpiScale_) {
        appliedDpiScale_ = scale;
        ui::applyTheme(themeId_, scale);
        log::info(std::format("Main window DPI scale is now {:.2f}", scale));
    }
    const bool minimized = window_.isMinimized();
    if (minimized != wasMinimized_) {
        wasMinimized_ = minimized;
        log::info(std::format("Main window {} at {:.2f} s", minimized ? "minimized" : "restored", time));
    }
    scheduler_.setMinimized(minimized);
    scheduler_.setOccluded(occluded_);
    updateAudioState();
    scheduler_.setAnimating(uiState_.engine.playing);
    scheduler_.setHasSecondaryViewports(ImGui::GetPlatformIO().Viewports.Size > 1);
}

void Application::applyAudioConfig() {
    engine::AudioStreamConfig config;
    config.outputDeviceId = uiState_.audioDeviceId;
    config.sampleRate = uiState_.audioRate;
    config.requestedFrames = static_cast<std::uint32_t>(uiState_.audioFrames);
    audio_->open(config);
}

void Application::updateAudioState() {
    if (!audio_) return;
    uiState_.engine = audio_->engineState();
    uiState_.diskMissFrames = audio_->diskMissFrames();
    uiState_.streamedBlocks = audio_->streamedBlocks();
    finishImports();
    if (history_.revision() != publishedRevision_) {
        publishedRevision_ = history_.revision();
        audio_->setProject(history_.current());
    }
    const std::uint64_t generation = audio_->stateGeneration();
    if (generation == seenAudioGeneration_) return;
    seenAudioGeneration_ = generation;
    const engine::AudioStatus previous = uiState_.audio.status;
    uiState_.audio = audio_->state();
    if (uiState_.audio.status != previous || !uiState_.audio.message.empty()) {
        log::info(std::format("Audio {}: {} {}", engine::audioStatusText(uiState_.audio.status), uiState_.audio.stream.deviceName, uiState_.audio.message));
    }
}

void Application::requestImport(const std::filesystem::path& path, model::Id trackId, model::Time start) {
    const std::uint64_t token = nextImportToken_++;
    pendingImports_[token] = {trackId, start};
    audio_->media().requestImport(token, path);
    uiState_.importMessage.clear();
    log::info("Importing " + platform::toUtf8(path.wstring()));
}

void Application::finishImports() {
    for (const engine::ImportResult& result : audio_->media().takeResults()) {
        const auto pending = pendingImports_.find(result.token);
        if (pending == pendingImports_.end()) continue;
        const PendingImport where = pending->second;
        pendingImports_.erase(pending);
        const std::string name = platform::toUtf8(result.path.filename().wstring());
        if (!result.ok) {
            uiState_.importMessage = name + ": " + result.error;
            log::error("Import failed: " + uiState_.importMessage);
            continue;
        }
        log::info(std::format("Imported {}: {:.0f} Hz, {} channels, via {}", name, result.sourceSampleRate, result.channels, result.decoderName));
        model::Id clipId = model::kNoId;
        model::Id trackId = where.trackId;
        history_.apply("Import " + name, [&](model::Project& project) {
            const model::Time length = model::framesToTime(result.framesAtSourceRate, result.sourceSampleRate);
            const model::Id media = commands::addMedia(project, result.path, length, result.channels, result.sourceSampleRate);
            if (media == model::kNoId) return false;
            if (trackId == model::kNoId || !project.findTrack(trackId)) trackId = commands::addTrack(project, result.path.stem().string());
            clipId = commands::addClip(project, trackId, media, where.start);
            return clipId != model::kNoId;
        });
        uiState_.timeline.selectedClip = clipId;
        uiState_.timeline.selectedTrack = trackId;
    }
}

void Application::handleDrops() {
    bool first = true;
    for (const auto& path : window_.takeDroppedFiles()) {
        const std::string name = platform::toUtf8(path.wstring());
        log::info("Dropped file: " + name);
        uiState_.droppedFiles.insert(uiState_.droppedFiles.begin(), name);
        if (uiState_.droppedFiles.size() > kMaxDroppedFilesShown) uiState_.droppedFiles.pop_back();
        // The first file lands where it was dropped. Any others get tracks of their own, lined up with it.
        const ui::TimelineState& timeline = uiState_.timeline;
        requestImport(path, (first && timeline.dropInside) ? timeline.dropTrack : model::kNoId, timeline.dropInside ? timeline.dropTime : 0);
        first = false;
    }
}

void Application::renderFrame(double time) {
    using Clock = std::chrono::steady_clock;
    scheduler_.markRendered(time);
    const auto buildStart = Clock::now();

    workspaces_.beforeNewFrame();
    imgui_.beginFrame();
    uiState_.schedulerMode = static_cast<int>(scheduler_.mode(time));
    uiState_.currentWorkspace = workspaces_.current();
    const ui::UiActions actions = ui::drawInterface(workspaces_, uiState_);
    imgui_.endFrameAndBuildDrawData();

    const auto renderStart = Clock::now();
    swapChain_.bindAndClear(device_.context(), kClearColor);
    imgui_.renderMainViewport();
    imgui_.renderSecondaryViewports();

    const auto presentStart = Clock::now();
    if (!window_.isMinimized()) {
        const renderer::PresentResult result = swapChain_.present(true);
        occluded_ = (result == renderer::PresentResult::Occluded);
        if (result == renderer::PresentResult::DeviceLost) {
            log::error("Graphics device lost during Present");
            deviceLost_ = true;
            nextDeviceRetryAt_ = time;
        }
    }
    const auto end = Clock::now();

    const double buildMs = std::chrono::duration<double, std::milli>(renderStart - buildStart).count();
    const double renderMs = std::chrono::duration<double, std::milli>(presentStart - renderStart).count();
    const double presentMs = std::chrono::duration<double, std::milli>(end - presentStart).count();
    buildMsSum_ += buildMs;
    renderMsSum_ += renderMs;
    presentMsSum_ += presentMs;
    ++framesSinceSample_;
    ++framesTotal_;
    if (profileMeasuring_) {
        profileBuildMs_ += buildMs;
        profileRenderMs_ += renderMs;
        profilePresentMs_ += presentMs;
        profileMaxViewports_ = std::max(profileMaxViewports_, ImGui::GetPlatformIO().Viewports.Size);
        profileDrawCalls_ = std::max(profileDrawCalls_, ui::ImGuiHost::countDrawCalls());
    }
    uiState_.drawCalls = ui::ImGuiHost::countDrawCalls();
    uiState_.vertices = ui::ImGuiHost::countVertices();
    uiState_.viewportCount = ImGui::GetPlatformIO().Viewports.Size;

    applyActions(actions);
}

void Application::applyActions(const ui::UiActions& actions) {
    if (actions.exit) exitRequested_ = true;
    if (actions.dialog != ui::DialogRequest::None) pendingDialog_ = actions.dialog;
    if (actions.simulateDeviceLoss) simulateDeviceLoss_ = true;
    if (audio_) {
        for (const engine::EngineCommand& command : actions.commands) {
            if (!audio_->post(command)) log::error("Audio command queue is full; a command was dropped");
        }
        if (actions.audioConfigChanged) {
            applyAudioConfig();
            savePreferences();
        }
    }
    if (actions.preferencesChanged) {
        themeId_ = uiState_.themeId;
        ui::applyTheme(themeId_, appliedDpiScale_);
        FrameScheduler::Config config = scheduler_.config();
        config.activeHz = uiState_.activeHz;
        config.idleHz = uiState_.idleHz;
        scheduler_.setConfig(config);
        savePreferences();
    }
}

void Application::runPendingDialog() {
    const ui::DialogRequest request = pendingDialog_;
    pendingDialog_ = ui::DialogRequest::None;
    if (request == ui::DialogRequest::None) return;

    const auto result = request == ui::DialogRequest::Open ? platform::showOpenFileDialog(window_.hwnd())
                                                           : platform::showSaveFileDialog(window_.hwnd());
    if (request == ui::DialogRequest::ImportAudio) {
        if (result) {
            const double rate = uiState_.engine.sampleRate > 0.0 ? uiState_.engine.sampleRate : 48000.0;
            requestImport(*result, model::kNoId, model::framesToTime(uiState_.engine.positionFrames, rate));
        }
        scheduler_.noteActivity(now());
        return;
    }
    if (request == ui::DialogRequest::OfflineTestWav) {
        if (result) {
            auto path = *result;
            if (!path.has_extension()) path += L".wav";
            audio_->runOfflineTest(path);
        }
        scheduler_.noteActivity(now());
        return;
    }
    uiState_.lastDialogResult = result ? platform::toUtf8(result->wstring()) : std::string("(cancelled)");
    log::info(std::format("{} dialog returned: {}", request == ui::DialogRequest::Open ? "Open" : "Save", uiState_.lastDialogResult));
    scheduler_.noteActivity(now());
}

void Application::sampleMetrics() {
    const double time = now();
    uiState_.process = metrics_.sample();
    uiState_.pushCpuHistory(static_cast<float>(uiState_.process.processCpuOfOneCore));
    const double elapsed = time - lastSampleTime_;
    uiState_.fps = elapsed > 0.0 ? framesSinceSample_ / elapsed : 0.0;
    const double divisor = framesSinceSample_ > 0 ? framesSinceSample_ : 1;
    uiState_.buildMs = buildMsSum_ / divisor;
    uiState_.renderMs = renderMsSum_ / divisor;
    uiState_.presentMs = presentMsSum_ / divisor;
    uiState_.framesTotal = framesTotal_;
    uiState_.monitorCount = static_cast<int>(platform::WindowHost::monitorWorkAreas().size());
    uiState_.dpiScale = appliedDpiScale_;
    if (audio_) {
        engine::AudioTelemetry& telemetry = audio_->telemetry();
        uiState_.loadP50 = telemetry.loadPercentile(0.50);
        uiState_.loadP95 = telemetry.loadPercentile(0.95);
        uiState_.loadP99 = telemetry.loadPercentile(0.99);
        uiState_.loadPeak = telemetry.peakLoad();
        uiState_.callbacks = telemetry.callbacks();
        uiState_.xrunsEngine = telemetry.xruns(engine::XrunCause::Engine);
        uiState_.xrunsDevice = telemetry.xruns(engine::XrunCause::Device);
        uiState_.xrunsUnknown = telemetry.xruns(engine::XrunCause::Unknown);
        uiState_.outputPeak = audio_->takeOutputPeak();
        engine::XrunEvent event;
        while (telemetry.popXrun(event)) {
            log::error(std::format("Audio dropout at {:.3f} s: {}, load {:.0f}%", event.streamSeconds, engine::xrunCauseText(event.cause), event.loadPercent));
            uiState_.recentXruns.insert(uiState_.recentXruns.begin(), event);
            if (uiState_.recentXruns.size() > 8) uiState_.recentXruns.pop_back();
        }
    }
    buildMsSum_ = renderMsSum_ = presentMsSum_ = 0.0;
    framesSinceSample_ = 0;
    lastSampleTime_ = time;
    profileMaxWorkingSet_ = std::max(profileMaxWorkingSet_, uiState_.process.workingSetBytes);
}

void Application::recoverDevice(double time) {
    if (time < nextDeviceRetryAt_) return;
    nextDeviceRetryAt_ = time + kDeviceRetrySeconds;

    imgui_.suspendBackends();
    swapChain_.destroy();
    std::string error;
    if (!device_.create(args_.warp, error) || !swapChain_.create(device_.device(), window_.hwnd())) {
        log::error("Graphics device recovery failed; will retry. " + error);
        return;
    }
    if (!imgui_.resumeBackends(device_.device(), device_.context())) {
        log::error("Could not restart the ImGui backends; will retry");
        return;
    }
    deviceLost_ = false;
    occluded_ = false;
    uiState_.adapterName = device_.adapterName();
    uiState_.deviceRecoveries += 1;
    log::info("Graphics device recovered");
    scheduler_.noteActivity(time);
}

void Application::beginProfileWindow() {
    profileMetrics_.emplace(1); // Baseline is now; everything before is discarded.
    if (audio_) audio_->telemetry().reset();
    profileMeasuring_ = true;
    profileStart_ = now();
    profileEndAt_ = profileStart_ + args_.profileSeconds;
    profileFramesAtStart_ = framesTotal_;
    profileMaxWorkingSet_ = 0;
    log::info(std::format("Profile window started: {}", profileModeName(args_.profile)));
}

bool Application::finishProfile() {
    const double elapsed = now() - profileStart_;
    const platform::ProcessSample sample = profileMetrics_->sample();
    profileMaxWorkingSet_ = std::max(profileMaxWorkingSet_, sample.workingSetBytes);
    const std::uint64_t frames = framesTotal_ - profileFramesAtStart_;
    const double divisor = frames > 0 ? static_cast<double>(frames) : 1.0;

    std::ofstream out(args_.profileOut, std::ios::trunc);
    out << std::format(
        "{{\n  \"mode\": \"{}\",\n  \"settleSeconds\": {:.0f},\n  \"seconds\": {:.2f},\n  \"adapter\": \"{}\",\n  \"warp\": {},\n"
        "  \"monitors\": {},\n  \"logicalCores\": {},\n  \"frames\": {},\n  \"framesPerSecond\": {:.2f},\n"
        "  \"processCpuPctOfOneCore\": {:.3f},\n  \"processCpuPctOfSystem\": {:.3f},\n  \"uiThreadCpuPctOfOneCore\": {:.3f},\n"
        "  \"avgBuildMs\": {:.3f},\n  \"avgRenderMs\": {:.3f},\n  \"avgPresentMsIncludingVsyncWait\": {:.3f},\n"
        "  \"maxDrawCalls\": {},\n  \"maxViewports\": {},\n  \"workingSetMiB\": {:.1f},\n  \"maxWorkingSetMiB\": {:.1f},\n"
        "  \"privateMiB\": {:.1f},\n  \"mainWindowMinimized\": {},\n  \"audioStatus\": \"{}\",\n  \"audioDevice\": \"{}\",\n  \"audioSampleRate\": {:.0f},\n"
        "  \"audioPeriodFrames\": {},\n  \"audioP99LoadPct\": {:.1f},\n  \"audioPeakLoadPct\": {:.2f},\n  \"xrunsEngine\": {},\n  \"xrunsDevice\": {}\n}}\n",
        profileModeName(args_.profile), args_.settleSeconds, elapsed, device_.adapterName(), device_.isWarp() ? "true" : "false",
        platform::WindowHost::monitorWorkAreas().size(), sample.logicalCores, frames, static_cast<double>(frames) / elapsed,
        sample.processCpuOfOneCore, sample.processCpuOfSystem, sample.uiThreadCpuOfOneCore, profileBuildMs_ / divisor,
        profileRenderMs_ / divisor, profilePresentMs_ / divisor, profileDrawCalls_, profileMaxViewports_,
        static_cast<double>(sample.workingSetBytes) / 1048576.0, static_cast<double>(profileMaxWorkingSet_) / 1048576.0,
        static_cast<double>(sample.privateBytes) / 1048576.0, window_.isMinimized() ? "true" : "false",
        engine::audioStatusText(uiState_.audio.status), uiState_.audio.stream.deviceName, uiState_.audio.stream.sampleRate, uiState_.audio.stream.periodFrames,
        audio_->telemetry().loadPercentile(0.99), audio_->telemetry().peakLoad(), audio_->telemetry().xruns(engine::XrunCause::Engine), audio_->telemetry().xruns(engine::XrunCause::Device));
    // Someone restoring the window mid-run would silently invalidate the measurement.
    if (args_.profile == ProfileMode::Minimized && (!window_.isMinimized() || frames != 0)) {
        log::error("Minimized profile is invalid: the window was restored during the run");
        return false;
    }
    return static_cast<bool>(out);
}

int Application::run(int showCommand) {
    if (!initialize(showCommand)) {
        shutdown();
        return 1;
    }

    int exitCode = 0;
    bool running = true;
    while (running && !exitRequested_) {
        if (!window_.pumpMessages()) break;
        const double time = now();

        if (args_.smokeSeconds > 0.0 && time >= args_.smokeSeconds) {
            log::info(std::format("Smoke run finished after {} frames", framesTotal_));
            exitCode = framesTotal_ > 0 ? 0 : 2;
            running = false;
            continue;
        }
        if (args_.profile != ProfileMode::None) {
            if (!profileMeasuring_ && time >= args_.settleSeconds) beginProfileWindow();
            if (profileMeasuring_ && time >= profileEndAt_) {
                exitCode = finishProfile() ? 0 : 3;
                running = false;
                continue;
            }
            if (args_.profile == ProfileMode::Active) scheduler_.noteActivity(time);
            if (args_.profile == ProfileMode::Playing && !profilePlayStarted_ && uiState_.audio.status == engine::AudioStatus::Running) {
                profilePlayStarted_ = true; // Silent: full processing, zero output gain.
                audio_->post({engine::EngineCommandType::SetOutputGain, 0.0});
                audio_->post({engine::EngineCommandType::SetMetronome, 1.0});
                audio_->post({engine::EngineCommandType::Play, 0.0});
            }
        }

        if (args_.uiSelfTest) {
            stepSelfTest(time);
            if (selfTestDone_) {
                exitCode = selfTestFailures_ == 0 ? 0 : 4;
                running = false;
                continue;
            }
        }

        handleDrops();
        if (simulateDeviceLoss_) {
            simulateDeviceLoss_ = false;
            log::info("Simulating graphics device loss from Diagnostics");
            deviceLost_ = true;
            nextDeviceRetryAt_ = time;
        }
        if (deviceLost_) recoverDevice(time);

        updateFrameState(time);

        if (time >= nextMetricsAt_) {
            sampleMetrics();
            nextMetricsAt_ = time + kMetricsIntervalSeconds;
        }

        if (!deviceLost_ && scheduler_.shouldRender(time)) {
            if (scheduler_.mode(time) == FrameScheduler::Mode::Occluded) {
                scheduler_.markRendered(time);
                occluded_ = swapChain_.probe() == renderer::PresentResult::Occluded;
            } else {
                renderFrame(time);
            }
            if (pendingDialog_ != ui::DialogRequest::None) runPendingDialog();
        }

        const double afterFrame = now();
        double wait = std::max(0.0, nextMetricsAt_ - afterFrame);
        const double untilFrame = deviceLost_ ? kDeviceRetrySeconds : scheduler_.secondsUntilNextFrame(afterFrame);
        if (untilFrame >= 0.0) wait = std::min(wait, untilFrame);
        if (args_.smokeSeconds > 0.0) wait = std::min(wait, std::max(0.0, args_.smokeSeconds - afterFrame));
        if (profileMeasuring_) wait = std::min(wait, std::max(0.0, profileEndAt_ - afterFrame));

        if (window_.waitForMessages(wait)) scheduler_.noteActivity(now());
    }

    shutdown();
    return exitCode;
}

} // namespace ldaw
