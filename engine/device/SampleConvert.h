// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace ldaw::engine {

enum class DeviceSampleFormat { Float32, Float64, Int16, Int24, Int24In32, Int32, Unsupported };

constexpr std::uint32_t bytesPerSample(DeviceSampleFormat format) {
    switch (format) {
    case DeviceSampleFormat::Float32: return 4;
    case DeviceSampleFormat::Float64: return 8;
    case DeviceSampleFormat::Int16: return 2;
    case DeviceSampleFormat::Int24: return 3;
    case DeviceSampleFormat::Int24In32: return 4;
    case DeviceSampleFormat::Int32: return 4;
    case DeviceSampleFormat::Unsupported: return 0;
    }
    return 0;
}

constexpr const char* sampleFormatText(DeviceSampleFormat format) {
    switch (format) {
    case DeviceSampleFormat::Float32: return "32-bit float";
    case DeviceSampleFormat::Float64: return "64-bit float";
    case DeviceSampleFormat::Int16: return "16-bit";
    case DeviceSampleFormat::Int24: return "24-bit";
    case DeviceSampleFormat::Int24In32: return "24-bit in 32";
    case DeviceSampleFormat::Int32: return "32-bit";
    case DeviceSampleFormat::Unsupported: return "unsupported";
    }
    return "?";
}

namespace detail {

inline std::int64_t toInteger(double sample, double scale, double maximum) {
    return std::llround(std::clamp(sample * scale, -scale, maximum));
}

inline void writeSample(std::uint8_t* out, double sample, DeviceSampleFormat format) {
    switch (format) {
    case DeviceSampleFormat::Float32: {
        const auto value = static_cast<float>(sample);
        std::memcpy(out, &value, 4);
        break;
    }
    case DeviceSampleFormat::Float64:
        std::memcpy(out, &sample, 8);
        break;
    case DeviceSampleFormat::Int16: {
        const auto value = static_cast<std::int16_t>(toInteger(sample, 32768.0, 32767.0));
        std::memcpy(out, &value, 2);
        break;
    }
    case DeviceSampleFormat::Int24: {
        const auto value = static_cast<std::int32_t>(toInteger(sample, 8388608.0, 8388607.0));
        out[0] = static_cast<std::uint8_t>(value);
        out[1] = static_cast<std::uint8_t>(value >> 8);
        out[2] = static_cast<std::uint8_t>(value >> 16);
        break;
    }
    case DeviceSampleFormat::Int24In32: {
        const auto value = static_cast<std::int32_t>(toInteger(sample, 8388608.0, 8388607.0) * 256);
        std::memcpy(out, &value, 4);
        break;
    }
    case DeviceSampleFormat::Int32: {
        const auto value = static_cast<std::int32_t>(toInteger(sample, 2147483648.0, 2147483647.0));
        std::memcpy(out, &value, 4);
        break;
    }
    case DeviceSampleFormat::Unsupported:
        break;
    }
}

} // namespace detail

// Writes engine stereo into a device buffer of any layout. Mono devices get the
// average of both sides, and channels past the second get silence. Allocation-free.
inline void writeStereoToDevice(const float* left, const float* right, std::uint32_t frames, std::uint8_t* out,
                                std::uint32_t channels, DeviceSampleFormat format) {
    const std::uint32_t sampleBytes = bytesPerSample(format);
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        for (std::uint32_t channel = 0; channel < channels; ++channel) {
            double sample = 0.0;
            if (channels == 1) sample = (static_cast<double>(left[frame]) + right[frame]) * 0.5;
            else if (channel == 0) sample = left[frame];
            else if (channel == 1) sample = right[frame];
            detail::writeSample(out, sample, format);
            out += sampleBytes;
        }
    }
}

// Picks the shared-mode period closest to the request, on the driver's allowed grid
// (minimum plus multiples of the fundamental, up to the maximum).
inline std::uint32_t chooseSharedPeriod(std::uint32_t requested, std::uint32_t fundamental, std::uint32_t minimum, std::uint32_t maximum) {
    if (fundamental == 0 || minimum == 0 || maximum < minimum) return maximum;
    const std::uint32_t clamped = std::clamp(requested, minimum, maximum);
    const std::uint32_t steps = (clamped - minimum + fundamental / 2) / fundamental;
    return std::min(minimum + steps * fundamental, maximum);
}

} // namespace ldaw::engine
