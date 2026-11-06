// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <objbase.h>
#include <initguid.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wmcodecdsp.h>
#include <propvarutil.h>
#include <wrl/client.h>

#define ENABLE_SNDFILE_WINDOWS_PROTOTYPES 1
#include <sndfile.h>

#include "engine/media/AudioDecoder.h"

#include <algorithm>
#include <cwctype>
#include <format>
#include <limits>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace ldaw::engine {
namespace {

constexpr std::uint32_t kChunkFrames = 16384;

// --- Media Foundation is loaded at run time, so the app still starts where it's missing.
struct MfApi {
    using StartupFn = HRESULT(WINAPI*)(ULONG, DWORD);
    using ShutdownFn = HRESULT(WINAPI*)();
    using CreateMediaTypeFn = HRESULT(WINAPI*)(IMFMediaType**);
    using CreateMemoryBufferFn = HRESULT(WINAPI*)(DWORD, IMFMediaBuffer**);
    using CreateSampleFn = HRESULT(WINAPI*)(IMFSample**);
    using CreateReaderFn = HRESULT(WINAPI*)(LPCWSTR, IMFAttributes*, IMFSourceReader**);
    using CreateWriterFn = HRESULT(WINAPI*)(LPCWSTR, IMFByteStream*, IMFAttributes*, IMFSinkWriter**);

    StartupFn startup = nullptr;
    ShutdownFn shutdown = nullptr;
    CreateMediaTypeFn createMediaType = nullptr;
    CreateMemoryBufferFn createMemoryBuffer = nullptr;
    CreateSampleFn createSample = nullptr;
    CreateReaderFn createReader = nullptr;
    CreateWriterFn createWriter = nullptr;
    bool ok = false;
};

const MfApi& mfApi() {
    static const MfApi api = [] {
        MfApi out;
        const HMODULE plat = LoadLibraryExW(L"mfplat.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        const HMODULE readWrite = LoadLibraryExW(L"mfreadwrite.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!plat || !readWrite) return out;
        out.startup = reinterpret_cast<MfApi::StartupFn>(GetProcAddress(plat, "MFStartup"));
        out.shutdown = reinterpret_cast<MfApi::ShutdownFn>(GetProcAddress(plat, "MFShutdown"));
        out.createMediaType = reinterpret_cast<MfApi::CreateMediaTypeFn>(GetProcAddress(plat, "MFCreateMediaType"));
        out.createMemoryBuffer = reinterpret_cast<MfApi::CreateMemoryBufferFn>(GetProcAddress(plat, "MFCreateMemoryBuffer"));
        out.createSample = reinterpret_cast<MfApi::CreateSampleFn>(GetProcAddress(plat, "MFCreateSample"));
        out.createReader = reinterpret_cast<MfApi::CreateReaderFn>(GetProcAddress(readWrite, "MFCreateSourceReaderFromURL"));
        out.createWriter = reinterpret_cast<MfApi::CreateWriterFn>(GetProcAddress(readWrite, "MFCreateSinkWriterFromURL"));
        out.ok = out.startup && out.shutdown && out.createMediaType && out.createMemoryBuffer && out.createSample && out.createReader && out.createWriter;
        return out;
    }();
    return api;
}

class MfSession {
public:
    MfSession() : started_(mfApi().ok && SUCCEEDED(mfApi().startup(MF_VERSION, MFSTARTUP_LITE))) {}
    ~MfSession() {
        if (started_) mfApi().shutdown();
    }
    bool started() const { return started_; }

private:
    bool started_;
};

std::wstring lowerExtension(const std::filesystem::path& path) {
    std::wstring extension = path.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return extension;
}

// Takes interleaved frames of any channel count and hands the sink at most 2 channels.
bool deliver(const DecodeSink& sink, const float* data, std::uint32_t frames, std::uint32_t channels, std::vector<float>& scratch) {
    if (channels <= 2) return sink(data, frames, channels);
    scratch.resize(static_cast<std::size_t>(frames) * 2);
    for (std::uint32_t i = 0; i < frames; ++i) {
        scratch[i * 2] = data[i * channels];
        scratch[i * 2 + 1] = data[i * channels + 1];
    }
    return sink(scratch.data(), frames, 2);
}

bool decodeWithSndfile(const std::filesystem::path& path, DecodedFormat& format, const DecodeSink& sink, std::string& error) {
    SF_INFO info{};
    SNDFILE* file = sf_wchar_open(path.c_str(), SFM_READ, &info);
    if (!file) {
        error = sf_strerror(nullptr);
        return false;
    }
    struct Closer {
        SNDFILE* file;
        ~Closer() { sf_close(file); }
    } closer{file};

    // Limits guard against a crafted header asking for absurd allocations.
    if (info.samplerate <= 0 || info.samplerate > 768000 || info.channels <= 0 || info.channels > 64 || info.frames < 0) {
        error = "The file's header has values outside the supported range.";
        return false;
    }
    format.sampleRate = info.samplerate;
    format.channels = static_cast<std::uint32_t>(info.channels);
    format.frames = info.frames;
    format.decoderName = "libsndfile";

    std::vector<float> buffer(static_cast<std::size_t>(kChunkFrames) * static_cast<std::size_t>(info.channels));
    std::vector<float> scratch;
    for (;;) {
        const sf_count_t got = sf_readf_float(file, buffer.data(), kChunkFrames);
        if (got <= 0) break;
        if (!deliver(sink, buffer.data(), static_cast<std::uint32_t>(got), format.channels, scratch)) {
            error = "Cancelled.";
            return false;
        }
    }
    const int status = sf_error(file);
    if (status != SF_ERR_NO_ERROR) {
        error = sf_error_number(status);
        return false;
    }
    return true;
}

bool decodeWithMediaFoundation(const std::filesystem::path& path, DecodedFormat& format, const DecodeSink& sink, std::string& error) {
    const MfSession session;
    if (!session.started()) {
        error = "This edition of Windows has no Media Foundation, which is needed for this format.";
        return false;
    }
    const MfApi& mf = mfApi();

    ComPtr<IMFSourceReader> reader;
    HRESULT hr = mf.createReader(path.c_str(), nullptr, &reader);
    if (FAILED(hr)) {
        error = std::format("Windows could not open this file for decoding (0x{:08X}). The codec may not be installed.", static_cast<unsigned long>(hr));
        return false;
    }

    constexpr DWORD kStream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM);
    reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
    reader->SetStreamSelection(kStream, TRUE);

    ComPtr<IMFMediaType> wanted;
    if (FAILED(hr = mf.createMediaType(&wanted))) {
        error = "Could not create the audio format description.";
        return false;
    }
    wanted->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    wanted->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
    if (FAILED(hr = reader->SetCurrentMediaType(kStream, nullptr, wanted.Get()))) {
        error = "No Windows decoder can convert this file to PCM.";
        return false;
    }
    ComPtr<IMFMediaType> actual;
    if (FAILED(hr = reader->GetCurrentMediaType(kStream, &actual))) {
        error = "Could not read the decoded audio format.";
        return false;
    }
    UINT32 channels = 0, rate = 0, bits = 0;
    actual->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
    actual->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
    actual->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits);
    if (channels == 0 || channels > 64 || rate == 0 || rate > 768000 || bits != 32) {
        error = "The decoded audio format is outside the supported range.";
        return false;
    }
    format.sampleRate = rate;
    format.channels = channels;
    format.frames = -1;
    format.decoderName = "Windows Media Foundation";

    std::vector<float> scratch;
    for (;;) {
        DWORD flags = 0;
        ComPtr<IMFSample> sample;
        hr = reader->ReadSample(kStream, 0, nullptr, &flags, nullptr, &sample);
        if (FAILED(hr)) {
            error = std::format("Decoding failed partway through the file (0x{:08X}).", static_cast<unsigned long>(hr));
            return false;
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
        if (!sample) continue;
        ComPtr<IMFMediaBuffer> buffer;
        if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) continue;
        BYTE* data = nullptr;
        DWORD bytes = 0;
        if (FAILED(buffer->Lock(&data, nullptr, &bytes))) continue;
        const std::uint32_t frames = bytes / (channels * sizeof(float));
        const bool keepGoing = frames == 0 || deliver(sink, reinterpret_cast<const float*>(data), frames, channels, scratch);
        buffer->Unlock();
        if (!keepGoing) {
            error = "Cancelled.";
            return false;
        }
    }
    return true;
}

} // namespace

bool mediaFoundationAvailable() { return mfApi().ok; }

bool decodeAudioFile(const std::filesystem::path& path, DecodedFormat& formatOut, const DecodeSink& sink, std::string& errorOut) {
    const std::wstring extension = lowerExtension(path);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
        errorOut = "The file was not found.";
        return false;
    }
    const bool mfFormat = extension == L".flac" || extension == L".mp3" || extension == L".m4a" || extension == L".aac" || extension == L".mp4";
    if (mfFormat) return decodeWithMediaFoundation(path, formatOut, sink, errorOut);
    if (extension == L".wav" || extension == L".wave" || extension == L".aif" || extension == L".aiff" || extension == L".aifc") {
        return decodeWithSndfile(path, formatOut, sink, errorOut);
    }
    errorOut = "This file type isn't supported yet. Use WAV, AIFF, FLAC, MP3 or M4A.";
    return false;
}

bool encodeWithMediaFoundation(const std::filesystem::path& path, const float* interleaved, std::uint32_t frames, std::uint32_t channels, std::uint32_t sampleRate) {
    const MfSession session;
    if (!session.started()) return false;
    const MfApi& mf = mfApi();

    GUID container = MFAudioFormat_FLAC;
    const std::wstring extension = lowerExtension(path);
    if (extension == L".m4a") container = MFAudioFormat_AAC;
    else if (extension == L".mp3") container = MFAudioFormat_MP3;

    ComPtr<IMFSinkWriter> writer;
    if (FAILED(mf.createWriter(path.c_str(), nullptr, nullptr, &writer))) return false;

    ComPtr<IMFMediaType> output;
    if (FAILED(mf.createMediaType(&output))) return false;
    output->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    output->SetGUID(MF_MT_SUBTYPE, container);
    output->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, channels);
    output->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate);
    if (container == MFAudioFormat_FLAC) {
        output->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    } else {
        output->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
        output->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 24000);
    }
    DWORD stream = 0;
    if (FAILED(writer->AddStream(output.Get(), &stream))) return false;

    ComPtr<IMFMediaType> input;
    if (FAILED(mf.createMediaType(&input))) return false;
    const UINT32 blockAlign = channels * 2;
    input->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    input->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    input->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, channels);
    input->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate);
    input->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    input->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, blockAlign);
    input->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, blockAlign * sampleRate);
    if (FAILED(writer->SetInputMediaType(stream, input.Get(), nullptr))) return false;
    if (FAILED(writer->BeginWriting())) return false;

    std::uint32_t done = 0;
    while (done < frames) {
        const std::uint32_t chunk = std::min(frames - done, sampleRate / 10);
        ComPtr<IMFMediaBuffer> buffer;
        ComPtr<IMFSample> sample;
        if (FAILED(mf.createMemoryBuffer(chunk * blockAlign, &buffer)) || FAILED(mf.createSample(&sample))) return false;
        BYTE* data = nullptr;
        if (FAILED(buffer->Lock(&data, nullptr, nullptr))) return false;
        auto* pcm = reinterpret_cast<std::int16_t*>(data);
        for (std::uint32_t i = 0; i < chunk * channels; ++i) {
            pcm[i] = static_cast<std::int16_t>(std::clamp(interleaved[(done * channels) + i], -1.0f, 1.0f) * 32767.0f);
        }
        buffer->Unlock();
        buffer->SetCurrentLength(chunk * blockAlign);
        sample->AddBuffer(buffer.Get());
        sample->SetSampleTime(static_cast<LONGLONG>(done) * 10000000LL / sampleRate);
        sample->SetSampleDuration(static_cast<LONGLONG>(chunk) * 10000000LL / sampleRate);
        if (FAILED(writer->WriteSample(stream, sample.Get()))) return false;
        done += chunk;
    }
    return SUCCEEDED(writer->Finalize());
}

} // namespace ldaw::engine
