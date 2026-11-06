// SPDX-License-Identifier: GPL-3.0-or-later
// Headless test of the real audio path, with no window, ImGui or Direct3D linked.
#include <windows.h>
#include <objbase.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

#include "engine/AudioEngine.h"
#include "engine/device/WasapiBackend.h"
#include "engine/render/OfflineRender.h"
#include "engine/runtime/RtGuard.h"
#include "engine/telemetry/AudioTelemetry.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <format>
#include <fstream>
#include <new>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace ldaw::engine;
using Microsoft::WRL::ComPtr;

static std::atomic<std::uint64_t> g_realtimeAllocations{0};

void* operator new(std::size_t size) {
    if (g_inRealtimeSection) g_realtimeAllocations.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept {
    if (memory && g_inRealtimeSection) g_realtimeAllocations.fetch_add(1, std::memory_order_relaxed);
    std::free(memory);
}
void operator delete(void* memory, std::size_t) noexcept { operator delete(memory); }

namespace {

struct Options {
    std::string command;
    double seconds = 10.0;
    AudioStreamConfig stream;
    double gain = 0.0; // Silent by default; only listening tests ask for sound.
    double bpm = 120.0;
    int restarts = 10;
    std::wstring jsonPath;
};

void sleepSeconds(double seconds) { std::this_thread::sleep_for(std::chrono::duration<double>(seconds)); }

std::string runResultJson(const char* mode, const Options& options, const AudioStreamInfo& info, AudioTelemetry& telemetry, double seconds, const std::string& extra = {}) {
    return std::format(
        "{{\n  \"mode\": \"{}\",\n  \"seconds\": {:.1f},\n  \"device\": \"{}\",\n  \"sampleRate\": {:.0f},\n  \"requestedFrames\": {},\n"
        "  \"periodFrames\": {},\n  \"bufferFrames\": {},\n  \"format\": \"{}\",\n  \"channels\": {},\n  \"lowLatencyMode\": {},\n"
        "  \"systemResampling\": {},\n  \"realtimePriority\": {},\n  \"outputLatencyMs\": {:.2f},\n  \"gain\": {:.3f},\n"
        "  \"callbacks\": {},\n  \"frames\": {},\n  \"framesPerSecond\": {:.1f},\n  \"p50LoadPct\": {:.1f},\n  \"p95LoadPct\": {:.1f},\n  \"p99LoadPct\": {:.1f},\n"
        "  \"peakLoadPct\": {:.2f},\n  \"xrunsEngine\": {},\n  \"xrunsDevice\": {},\n  \"xrunsUnknown\": {},\n  \"realtimeAllocations\": {}{}\n}}\n",
        mode, seconds, info.deviceName, info.sampleRate, options.stream.requestedFrames, info.periodFrames, info.bufferFrames, info.deviceFormat, info.channels,
        info.lowLatencyMode, info.systemResampling, info.realtimePriority, info.outputLatencySeconds * 1000.0, options.gain, telemetry.callbacks(), telemetry.frames(),
        seconds > 0.0 ? static_cast<double>(telemetry.frames()) / seconds : 0.0, telemetry.loadPercentile(0.50), telemetry.loadPercentile(0.95), telemetry.loadPercentile(0.99),
        telemetry.peakLoad(), telemetry.xruns(XrunCause::Engine), telemetry.xruns(XrunCause::Device), telemetry.xruns(XrunCause::Unknown), g_realtimeAllocations.load(), extra);
}

void emit(const Options& options, const std::string& text) {
    std::fputs(text.c_str(), stdout);
    if (!options.jsonPath.empty()) std::ofstream(options.jsonPath, std::ios::trunc) << text;
}

void configure(AudioEngine& engine, const Options& options) {
    engine.post({EngineCommandType::SetOutputGain, options.gain});
    engine.post({EngineCommandType::SetTempo, options.bpm});
    engine.post({EngineCommandType::SetMetronome, 1.0});
    engine.post({EngineCommandType::Play, 0.0});
}

int commandList() {
    WasapiBackend backend;
    for (const AudioDeviceInfo& device : backend.enumerateOutputs()) {
        std::printf("%s%s\n    %s\n", device.isDefault ? "[default] " : "", device.name.c_str(), device.id.c_str());
    }
    return 0;
}

int commandRun(const Options& options) {
    WasapiBackend backend;
    AudioEngine engine;
    AudioTelemetry telemetry;
    if (!backend.start(options.stream, engine, telemetry)) {
        std::printf("Could not start: %s\n", backend.lastError().c_str());
        return 2;
    }
    configure(engine, options);
    sleepSeconds(options.seconds);
    const bool stillRunning = backend.status() == AudioStatus::Running;
    backend.stop();
    emit(options, runResultJson("run", options, backend.streamInfo(), telemetry, options.seconds));
    XrunEvent event;
    int shown = 0;
    while (telemetry.popXrun(event) && shown++ < 20) {
        std::printf("xrun at %.3f s: %s (load %.0f%%)\n", event.streamSeconds, xrunCauseText(event.cause), event.loadPercent);
    }
    return (stillRunning && g_realtimeAllocations.load() == 0) ? 0 : 3;
}

int commandRestarts(const Options& options) {
    WasapiBackend backend;
    AudioEngine engine;
    AudioTelemetry telemetry;
    std::uint64_t totalFrames = 0;
    for (int i = 0; i < options.restarts; ++i) {
        if (!backend.start(options.stream, engine, telemetry)) {
            std::printf("Restart %d failed: %s\n", i + 1, backend.lastError().c_str());
            return 2;
        }
        configure(engine, options);
        sleepSeconds(0.5);
        const std::uint64_t frames = telemetry.frames();
        if (frames == 0 || backend.status() != AudioStatus::Running) {
            std::printf("Restart %d produced no audio callbacks.\n", i + 1);
            return 3;
        }
        totalFrames += frames;
        engine.post({EngineCommandType::Stop, 0.0});
        engine.post({EngineCommandType::Rewind, 0.0});
        backend.stop();
    }
    emit(options, std::format("{{\n  \"mode\": \"restarts\",\n  \"cycles\": {},\n  \"allCyclesProducedCallbacks\": true,\n  \"totalFrames\": {},\n  \"realtimeAllocations\": {}\n}}\n",
                              options.restarts, totalFrames, g_realtimeAllocations.load()));
    return g_realtimeAllocations.load() == 0 ? 0 : 3;
}

// Records what the device is actually being sent, through Windows' loopback path, then
// measures the click timing in that recording. Other sound playing at the time can disturb it.
int commandLoopback(const Options& options) {
    WasapiBackend backend;
    AudioEngine engine;
    AudioTelemetry telemetry;

    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice> device;
    ComPtr<IAudioClient> capture;
    ComPtr<IAudioCaptureClient> captureClient;
    WAVEFORMATEX* format = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator)))) return 2;
    if (options.stream.outputDeviceId.empty()) {
        if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device))) return 2;
    } else {
        const std::wstring id(options.stream.outputDeviceId.begin(), options.stream.outputDeviceId.end());
        if (FAILED(enumerator->GetDevice(id.c_str(), &device))) return 2;
    }
    if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &capture))) return 2;
    if (FAILED(capture->GetMixFormat(&format))) return 2;
    if (FAILED(capture->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 10000000, 0, format, nullptr))) return 2;
    if (FAILED(capture->GetService(IID_PPV_ARGS(&captureClient)))) return 2;
    if (format->wBitsPerSample != 32) {
        std::printf("Loopback measurement expects a 32-bit float mix format.\n");
        return 2;
    }
    const std::uint32_t channels = format->nChannels;
    const double rate = format->nSamplesPerSec;
    CoTaskMemFree(format);

    std::vector<float> recorded;
    recorded.reserve(static_cast<std::size_t>(rate * (options.seconds + 2.0)));
    std::uint64_t discontinuities = 0;
    capture->Start();

    if (!backend.start(options.stream, engine, telemetry)) {
        std::printf("Could not start: %s\n", backend.lastError().c_str());
        return 2;
    }
    configure(engine, options);

    const auto end = std::chrono::steady_clock::now() + std::chrono::duration<double>(options.seconds);
    bool sawStart = false;
    while (std::chrono::steady_clock::now() < end) {
        UINT32 packet = 0;
        while (SUCCEEDED(captureClient->GetNextPacketSize(&packet)) && packet > 0) {
            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            if (FAILED(captureClient->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) break;
            // Windows flags a gap on the first packet; only later ones mean lost audio.
            if ((flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) && sawStart) ++discontinuities;
            sawStart = true;
            const auto* samples = reinterpret_cast<const float*>(data);
            for (UINT32 i = 0; i < frames; ++i) {
                recorded.push_back((flags & AUDCLNT_BUFFERFLAGS_SILENT) ? 0.0f : samples[i * channels]);
            }
            captureClient->ReleaseBuffer(frames);
        }
        Sleep(5);
    }
    backend.stop();
    capture->Stop();

    const auto onsets = findClickOnsets(recorded, static_cast<float>(0.02 * options.gain), static_cast<std::int64_t>(rate * 0.05));
    // Allow a few frames: detection depends on the waveform crossing a threshold.
    const ClickReport report = analyzeClickSpacing(onsets, rate, options.bpm, 4.0);
    const std::string extra = std::format(
        ",\n  \"loopbackClicks\": {},\n  \"expectedSpacingFrames\": {:.2f},\n  \"measuredSpacingFrames\": {:.3f},\n  \"maxErrorFrames\": {:.2f},\n"
        "  \"captureDiscontinuities\": {},\n  \"timingPassed\": {}",
        report.clicks, report.expectedSpacingFrames, report.meanSpacingFrames, report.maxErrorFrames, discontinuities, report.passed);
    emit(options, runResultJson("loopback", options, backend.streamInfo(), telemetry, options.seconds, extra));
    return report.passed ? 0 : 4;
}

Options parse(int argc, wchar_t** argv) {
    Options options;
    const auto narrow = [](std::wstring_view text) {
        const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string out(static_cast<std::size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size, nullptr, nullptr);
        return out;
    };
    if (argc > 1) options.command = narrow(argv[1]);
    for (int i = 2; i + 1 < argc; i += 2) {
        const std::wstring_view key = argv[i];
        const wchar_t* value = argv[i + 1];
        if (key == L"--seconds") options.seconds = std::wcstod(value, nullptr);
        else if (key == L"--frames") options.stream.requestedFrames = static_cast<std::uint32_t>(std::wcstoul(value, nullptr, 10));
        else if (key == L"--rate") options.stream.sampleRate = std::wcstod(value, nullptr);
        else if (key == L"--device") options.stream.outputDeviceId = narrow(value);
        else if (key == L"--gain") options.gain = std::wcstod(value, nullptr);
        else if (key == L"--bpm") options.bpm = std::wcstod(value, nullptr);
        else if (key == L"--restarts") options.restarts = _wtoi(value);
        else if (key == L"--json") options.jsonPath = value;
        else if (key == L"--stall-ms") options.stream.testStallMs = std::wcstod(value, nullptr);
        else if (key == L"--stall-every") options.stream.testStallEverySeconds = std::wcstod(value, nullptr);
    }
    return options;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const Options options = parse(argc, argv);
    int result = 1;
    if (options.command == "list") result = commandList();
    else if (options.command == "run") result = commandRun(options);
    else if (options.command == "restarts") result = commandRestarts(options);
    else if (options.command == "loopback") result = commandLoopback(options);
    else std::printf("Commands: list | run | restarts | loopback\nOptions: --seconds S --frames N --rate R --device ID --gain G --bpm B --restarts N --json FILE\n");
    CoUninitialize();
    return result;
}
