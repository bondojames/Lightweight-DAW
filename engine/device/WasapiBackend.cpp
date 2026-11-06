// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <objbase.h>
#include <initguid.h>
#include <audioclient.h>
#include <avrt.h>
#include <propidl.h>
#include <propkeydef.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
#include <wrl/client.h>

#include "engine/device/WasapiBackend.h"

#include "engine/device/SampleConvert.h"
#include "engine/runtime/RtGuard.h"
#include "engine/telemetry/AudioTelemetry.h"

#include <atomic>
#include <format>
#include <future>
#include <mutex>
#include <thread>

using Microsoft::WRL::ComPtr;

namespace ldaw::engine {
namespace {

std::string toUtf8(const wchar_t* wide) {
    if (!wide || !*wide) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), size, nullptr, nullptr);
    return out;
}

std::wstring toWide(const std::string& utf8) {
    if (utf8.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring out(static_cast<std::size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, out.data(), size);
    return out;
}

std::string hrText(HRESULT hr) { return std::format("0x{:08X}", static_cast<unsigned long>(hr)); }

std::string describeOpenFailure(HRESULT hr) {
    switch (hr) {
    case AUDCLNT_E_DEVICE_INVALIDATED: return "The audio device was removed.";
    case AUDCLNT_E_DEVICE_IN_USE: return "Another app has exclusive control of this audio device.";
    case AUDCLNT_E_UNSUPPORTED_FORMAT: return "This audio device does not support the requested format.";
    case AUDCLNT_E_ENGINE_PERIODICITY_LOCKED: return "Another app has locked this device's buffer size.";
    case E_NOTFOUND: return "No audio output device was found.";
    default: return "Could not open the audio device (" + hrText(hr) + ").";
    }
}

struct CoTaskFormat {
    WAVEFORMATEX* format = nullptr;
    ~CoTaskFormat() { CoTaskMemFree(format); }
};

DeviceSampleFormat parseFormat(const WAVEFORMATEX& format) {
    GUID subFormat{};
    WORD validBits = format.wBitsPerSample;
    if (format.wFormatTag == WAVE_FORMAT_EXTENSIBLE && format.cbSize >= 22) {
        const auto& extended = reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(format);
        subFormat = extended.SubFormat;
        validBits = extended.Samples.wValidBitsPerSample;
    } else if (format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        subFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    } else if (format.wFormatTag == WAVE_FORMAT_PCM) {
        subFormat = KSDATAFORMAT_SUBTYPE_PCM;
    } else {
        return DeviceSampleFormat::Unsupported;
    }
    if (subFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
        if (format.wBitsPerSample == 32) return DeviceSampleFormat::Float32;
        if (format.wBitsPerSample == 64) return DeviceSampleFormat::Float64;
        return DeviceSampleFormat::Unsupported;
    }
    if (subFormat == KSDATAFORMAT_SUBTYPE_PCM) {
        if (format.wBitsPerSample == 16) return DeviceSampleFormat::Int16;
        if (format.wBitsPerSample == 24) return DeviceSampleFormat::Int24;
        if (format.wBitsPerSample == 32) return validBits == 24 ? DeviceSampleFormat::Int24In32 : DeviceSampleFormat::Int32;
    }
    return DeviceSampleFormat::Unsupported;
}

double secondsNow() {
    static const double frequency = [] {
        LARGE_INTEGER value;
        QueryPerformanceFrequency(&value);
        return static_cast<double>(value.QuadPart);
    }();
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return static_cast<double>(now.QuadPart) / frequency;
}

// Only sets a flag: Windows forbids waiting or making COM device calls in these callbacks.
class DeviceNotifications final : public IMMNotificationClient {
public:
    std::atomic<bool> changed{false};

    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; } // Owned by the backend.
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IMMNotificationClient)) {
            *out = static_cast<IMMNotificationClient*>(this);
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override { return mark(); }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { return mark(); }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override { return mark(); }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow, ERole, LPCWSTR) override { return mark(); }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

private:
    HRESULT mark() {
        changed.store(true, std::memory_order_relaxed);
        return S_OK;
    }
};

std::string deviceName(IMMDevice* device) {
    ComPtr<IPropertyStore> properties;
    if (FAILED(device->OpenPropertyStore(STGM_READ, &properties))) return "Unknown device";
    PROPVARIANT value;
    PropVariantInit(&value);
    std::string name = "Unknown device";
    if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value)) && value.vt == VT_LPWSTR) name = toUtf8(value.pwszVal);
    PropVariantClear(&value);
    return name;
}

std::string deviceId(IMMDevice* device) {
    LPWSTR id = nullptr;
    std::string out;
    if (SUCCEEDED(device->GetId(&id))) out = toUtf8(id);
    CoTaskMemFree(id);
    return out;
}

} // namespace

class WasapiBackend::Impl {
public:
    Impl() {
        CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator_));
        if (enumerator_) enumerator_->RegisterEndpointNotificationCallback(&notifications_);
    }

    ~Impl() {
        stop();
        if (enumerator_) enumerator_->UnregisterEndpointNotificationCallback(&notifications_);
    }

    std::vector<AudioDeviceInfo> enumerateOutputs() {
        std::vector<AudioDeviceInfo> out;
        if (!enumerator_) return out;
        std::string defaultId;
        ComPtr<IMMDevice> defaultDevice;
        if (SUCCEEDED(enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice))) defaultId = deviceId(defaultDevice.Get());
        ComPtr<IMMDeviceCollection> devices;
        if (FAILED(enumerator_->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices))) return out;
        UINT count = 0;
        devices->GetCount(&count);
        for (UINT i = 0; i < count; ++i) {
            ComPtr<IMMDevice> device;
            if (FAILED(devices->Item(i, &device))) continue;
            AudioDeviceInfo info;
            info.id = deviceId(device.Get());
            info.name = deviceName(device.Get());
            info.isDefault = info.id == defaultId;
            out.push_back(std::move(info));
        }
        return out;
    }

    bool start(const AudioStreamConfig& config, IAudioProcessor& processor, AudioTelemetry& telemetry) {
        stop();
        stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        status_.store(AudioStatus::Stopped);
        setError({});
        std::promise<bool> started;
        auto result = started.get_future();
        thread_ = std::thread([this, config, &processor, &telemetry, &started] { threadMain(config, processor, telemetry, started); });
        return result.get();
    }

    void stop() {
        if (stopEvent_) SetEvent(stopEvent_);
        if (thread_.joinable()) thread_.join();
        if (stopEvent_) {
            CloseHandle(stopEvent_);
            stopEvent_ = nullptr;
        }
        const AudioStatus current = status_.load();
        if (current == AudioStatus::Running) status_.store(AudioStatus::Stopped);
    }

    AudioStatus status() const { return status_.load(); }

    AudioStreamInfo streamInfo() const {
        const std::scoped_lock lock(infoMutex_);
        return info_;
    }

    std::string lastError() const {
        const std::scoped_lock lock(infoMutex_);
        return error_;
    }

    bool takeDevicesChanged() { return notifications_.changed.exchange(false, std::memory_order_relaxed); }

private:
    void setError(const std::string& message) {
        const std::scoped_lock lock(infoMutex_);
        error_ = message;
    }

    void threadMain(AudioStreamConfig config, IAudioProcessor& processor, AudioTelemetry& telemetry, std::promise<bool>& started) {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        DWORD taskIndex = 0;
        const HANDLE task = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
        run(config, processor, telemetry, started, task != nullptr);
        if (task) AvRevertMmThreadCharacteristics(task);
        CoUninitialize();
    }

    void fail(std::promise<bool>& started, const std::string& message) {
        setError(message);
        status_.store(AudioStatus::Failed);
        started.set_value(false);
    }

    void run(const AudioStreamConfig& config, IAudioProcessor& processor, AudioTelemetry& telemetry, std::promise<bool>& started, bool realtime) {
        ComPtr<IMMDeviceEnumerator> enumerator;
        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
        if (FAILED(hr)) return fail(started, describeOpenFailure(hr));

        ComPtr<IMMDevice> device;
        hr = config.outputDeviceId.empty() ? enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device)
                                           : enumerator->GetDevice(toWide(config.outputDeviceId).c_str(), &device);
        if (FAILED(hr)) return fail(started, describeOpenFailure(hr));

        ComPtr<IAudioClient> client;
        hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client);
        if (FAILED(hr)) return fail(started, describeOpenFailure(hr));

        CoTaskFormat mix;
        hr = client->GetMixFormat(&mix.format);
        if (FAILED(hr)) return fail(started, describeOpenFailure(hr));

        WAVEFORMATEXTENSIBLE converted{};
        const WAVEFORMATEX* streamFormat = mix.format;
        const bool resample = config.sampleRate > 0.0 && static_cast<DWORD>(config.sampleRate) != mix.format->nSamplesPerSec;
        AudioStreamInfo info;
        hr = E_FAIL;
        if (resample) {
            // Windows converts for us. It only offers the default 10 ms period in this mode.
            converted.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
            converted.Format.nChannels = 2;
            converted.Format.nSamplesPerSec = static_cast<DWORD>(config.sampleRate);
            converted.Format.wBitsPerSample = 32;
            converted.Format.nBlockAlign = 8;
            converted.Format.nAvgBytesPerSec = converted.Format.nSamplesPerSec * 8;
            converted.Format.cbSize = 22;
            converted.Samples.wValidBitsPerSample = 32;
            converted.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
            converted.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
            streamFormat = &converted.Format;
            hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                    AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                                    0, 0, streamFormat, nullptr);
            info.systemResampling = true;
        } else {
            ComPtr<IAudioClient3> client3;
            if (SUCCEEDED(client.As(&client3))) {
                UINT32 defaultPeriod = 0, fundamental = 0, minimum = 0, maximum = 0;
                if (SUCCEEDED(client3->GetSharedModeEnginePeriod(mix.format, &defaultPeriod, &fundamental, &minimum, &maximum))) {
                    const UINT32 period = chooseSharedPeriod(config.requestedFrames, fundamental, minimum, maximum);
                    hr = client3->InitializeSharedAudioStream(AUDCLNT_STREAMFLAGS_EVENTCALLBACK, period, mix.format, nullptr);
                    if (SUCCEEDED(hr)) {
                        info.periodFrames = period;
                        info.lowLatencyMode = period != defaultPeriod;
                    } else {
                        // Most often another app has locked the period, so use what's running.
                        WAVEFORMATEX* current = nullptr;
                        UINT32 currentPeriod = 0;
                        if (SUCCEEDED(client3->GetCurrentSharedModeEnginePeriod(&current, &currentPeriod))) {
                            CoTaskMemFree(current);
                            client.Reset();
                            client3.Reset();
                            hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client);
                            if (SUCCEEDED(hr) && SUCCEEDED(client.As(&client3))) {
                                hr = client3->InitializeSharedAudioStream(AUDCLNT_STREAMFLAGS_EVENTCALLBACK, currentPeriod, mix.format, nullptr);
                                if (SUCCEEDED(hr)) info.periodFrames = currentPeriod;
                            }
                        }
                    }
                }
            }
            if (FAILED(hr)) {
                // The plain route, which every Windows driver supports.
                client.Reset();
                hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client);
                if (SUCCEEDED(hr)) hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, mix.format, nullptr);
            }
        }
        if (FAILED(hr)) return fail(started, describeOpenFailure(hr));

        const DeviceSampleFormat format = parseFormat(*streamFormat);
        if (format == DeviceSampleFormat::Unsupported) return fail(started, "This audio device uses a sample format this app cannot write.");
        const std::uint32_t channels = streamFormat->nChannels;
        const double sampleRate = streamFormat->nSamplesPerSec;

        const HANDLE audioEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        struct EventCloser {
            HANDLE handle;
            ~EventCloser() { CloseHandle(handle); }
        } audioEventCloser{audioEvent};
        hr = client->SetEventHandle(audioEvent);
        if (FAILED(hr)) return fail(started, describeOpenFailure(hr));

        UINT32 bufferFrames = 0;
        ComPtr<IAudioRenderClient> render;
        if (FAILED(hr = client->GetBufferSize(&bufferFrames)) || FAILED(hr = client->GetService(IID_PPV_ARGS(&render)))) {
            return fail(started, describeOpenFailure(hr));
        }
        if (info.periodFrames == 0) {
            REFERENCE_TIME period = 0;
            client->GetDevicePeriod(&period, nullptr);
            info.periodFrames = static_cast<std::uint32_t>(static_cast<double>(period) * sampleRate / 1.0e7 + 0.5);
        }
        REFERENCE_TIME latency = 0;
        client->GetStreamLatency(&latency);

        info.deviceName = deviceName(device.Get());
        info.deviceId = deviceId(device.Get());
        info.backendName = "WASAPI shared";
        info.deviceFormat = sampleFormatText(format);
        info.sampleRate = sampleRate;
        info.bufferFrames = bufferFrames;
        info.channels = channels;
        info.outputLatencySeconds = static_cast<double>(latency) / 1.0e7;
        info.realtimePriority = realtime;
        {
            const std::scoped_lock lock(infoMutex_);
            info_ = info;
        }

        // Everything the callback needs is allocated here, before the stream starts.
        std::vector<float> scratchLeft(bufferFrames), scratchRight(bufferFrames);
        processor.prepare(sampleRate, bufferFrames);
        telemetry.reset();

        BYTE* data = nullptr;
        if (SUCCEEDED(render->GetBuffer(bufferFrames, &data))) render->ReleaseBuffer(bufferFrames, AUDCLNT_BUFFERFLAGS_SILENT);

        hr = client->Start();
        if (FAILED(hr)) return fail(started, describeOpenFailure(hr));
        status_.store(AudioStatus::Running);
        started.set_value(true);

        loop(*client.Get(), *render.Get(), audioEvent, config, processor, telemetry, scratchLeft, scratchRight, channels, format, sampleRate, info.periodFrames, bufferFrames);

        client->Stop();
    }

    void loop(IAudioClient& client, IAudioRenderClient& render, HANDLE audioEvent, const AudioStreamConfig& config, IAudioProcessor& processor,
              AudioTelemetry& telemetry, std::vector<float>& scratchLeft, std::vector<float>& scratchRight, std::uint32_t channels,
              DeviceSampleFormat format, double sampleRate, std::uint32_t periodFrames, std::uint32_t bufferFrames) {
        const HANDLE handles[] = {stopEvent_, audioEvent};
        const double periodSeconds = static_cast<double>(periodFrames) / sampleRate;
        const double streamStart = secondsNow();
        double previousWake = streamStart;
        double nextStall = config.testStallEverySeconds > 0.0 ? streamStart + config.testStallEverySeconds : 0.0;
        float* outputs[2] = {scratchLeft.data(), scratchRight.data()};
        XrunDetector detector;
        bool firstWake = true;

        for (;;) {
            const DWORD woke = WaitForMultipleObjects(2, handles, FALSE, 2000);
            if (woke == WAIT_OBJECT_0) return; // Asked to stop.
            const double wakeTime = secondsNow();
            if (woke == WAIT_TIMEOUT) {
                // The device has stopped asking for audio altogether.
                telemetry.recordXrun(XrunCause::Device, wakeTime - streamStart, 0.0f);
                continue;
            }
            if (woke != WAIT_OBJECT_0 + 1) {
                status_.store(AudioStatus::Failed);
                return;
            }

            UINT32 padding = 0;
            HRESULT hr = client.GetCurrentPadding(&padding);
            if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
                status_.store(AudioStatus::DeviceLost);
                return;
            }
            if (FAILED(hr)) continue;

            if (!firstWake && detector.checkWake(wakeTime - previousWake, periodSeconds, padding)) {
                telemetry.recordXrun(XrunCause::Device, wakeTime - streamStart, telemetry.lastLoad());
            }
            firstWake = false;
            previousWake = wakeTime;

            const UINT32 available = bufferFrames - padding;
            if (available == 0) continue;

            BYTE* data = nullptr;
            hr = render.GetBuffer(available, &data);
            if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
                status_.store(AudioStatus::DeviceLost);
                return;
            }
            if (FAILED(hr)) continue;

            {
                const RealtimeSection realtime;
                ProcessContext context;
                context.outputs = outputs;
                context.outputChannels = 2;
                context.frames = available;
                context.sampleRate = sampleRate;
                processor.process(context);
                writeStereoToDevice(scratchLeft.data(), scratchRight.data(), available, data, channels, format);
            }

            if (nextStall > 0.0 && wakeTime >= nextStall) {
                nextStall = wakeTime + config.testStallEverySeconds;
                const double stallUntil = secondsNow() + config.testStallMs / 1000.0;
                while (secondsNow() < stallUntil) {}
            }

            hr = render.ReleaseBuffer(available, 0);
            if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
                status_.store(AudioStatus::DeviceLost);
                return;
            }

            const double computeSeconds = secondsNow() - wakeTime;
            const double deadlineSeconds = static_cast<double>(available) / sampleRate;
            telemetry.recordCallback(computeSeconds, deadlineSeconds, available);
            if (detector.checkCompute(computeSeconds, deadlineSeconds)) {
                telemetry.recordXrun(XrunCause::Engine, wakeTime - streamStart, telemetry.lastLoad());
            }
        }
    }

    ComPtr<IMMDeviceEnumerator> enumerator_;
    DeviceNotifications notifications_;
    std::thread thread_;
    HANDLE stopEvent_ = nullptr;
    std::atomic<AudioStatus> status_{AudioStatus::NotInitialized};
    mutable std::mutex infoMutex_; // Never taken inside the audio loop.
    AudioStreamInfo info_;
    std::string error_;
};

WasapiBackend::WasapiBackend() : impl_(std::make_unique<Impl>()) {}
WasapiBackend::~WasapiBackend() = default;
std::vector<AudioDeviceInfo> WasapiBackend::enumerateOutputs() { return impl_->enumerateOutputs(); }
bool WasapiBackend::start(const AudioStreamConfig& config, IAudioProcessor& processor, AudioTelemetry& telemetry) { return impl_->start(config, processor, telemetry); }
void WasapiBackend::stop() { impl_->stop(); }
AudioStatus WasapiBackend::status() const { return impl_->status(); }
AudioStreamInfo WasapiBackend::streamInfo() const { return impl_->streamInfo(); }
std::string WasapiBackend::lastError() const { return impl_->lastError(); }
bool WasapiBackend::takeDevicesChanged() { return impl_->takeDevicesChanged(); }

} // namespace ldaw::engine
