// SPDX-License-Identifier: GPL-3.0-or-later
#include "engine/device/SampleConvert.h"
#include "tests/unit/test_framework.h"

#include <array>
#include <cstring>

using namespace ldaw::engine;

namespace {

const float kLeft[] = {0.5f, -1.0f};
const float kRight[] = {-0.5f, 1.0f};

template <typename T>
T readAt(const std::uint8_t* data, std::size_t index) {
    T value{};
    std::memcpy(&value, data + index * sizeof(T), sizeof(T));
    return value;
}

} // namespace

TEST_CASE(period_choice_lands_on_the_driver_grid) {
    CHECK(chooseSharedPeriod(256, 48, 96, 480) == 240); // Nearer 240 than 288.
    CHECK(chooseSharedPeriod(264, 48, 96, 480) == 288); // Exactly between rounds up.
    CHECK(chooseSharedPeriod(128, 48, 96, 480) == 144);
    CHECK(chooseSharedPeriod(16, 48, 96, 480) == 96);
    CHECK(chooseSharedPeriod(100000, 48, 96, 480) == 480);
}

TEST_CASE(period_choice_handles_a_driver_with_one_size) {
    CHECK(chooseSharedPeriod(256, 480, 480, 480) == 480);
    CHECK(chooseSharedPeriod(256, 0, 480, 480) == 480);
    CHECK(chooseSharedPeriod(256, 0, 0, 0) == 0);
}

TEST_CASE(convert_float32_stereo) {
    std::array<std::uint8_t, 16> out{};
    writeStereoToDevice(kLeft, kRight, 2, out.data(), 2, DeviceSampleFormat::Float32);
    CHECK(readAt<float>(out.data(), 0) == 0.5f);
    CHECK(readAt<float>(out.data(), 1) == -0.5f);
    CHECK(readAt<float>(out.data(), 2) == -1.0f);
    CHECK(readAt<float>(out.data(), 3) == 1.0f);
}

TEST_CASE(convert_float64) {
    std::array<std::uint8_t, 32> out{};
    writeStereoToDevice(kLeft, kRight, 2, out.data(), 2, DeviceSampleFormat::Float64);
    CHECK(readAt<double>(out.data(), 0) == 0.5);
    CHECK(readAt<double>(out.data(), 3) == 1.0);
}

TEST_CASE(convert_int16_clamps_full_scale) {
    std::array<std::uint8_t, 8> out{};
    writeStereoToDevice(kLeft, kRight, 2, out.data(), 2, DeviceSampleFormat::Int16);
    CHECK(readAt<std::int16_t>(out.data(), 0) == 16384);
    CHECK(readAt<std::int16_t>(out.data(), 2) == -32768);
    CHECK(readAt<std::int16_t>(out.data(), 3) == 32767);
}

TEST_CASE(convert_int16_clamps_overload) {
    const float hot[] = {4.0f};
    const float cold[] = {-4.0f};
    std::array<std::uint8_t, 4> out{};
    writeStereoToDevice(hot, cold, 1, out.data(), 2, DeviceSampleFormat::Int16);
    CHECK(readAt<std::int16_t>(out.data(), 0) == 32767);
    CHECK(readAt<std::int16_t>(out.data(), 1) == -32768);
}

TEST_CASE(convert_packed_int24) {
    std::array<std::uint8_t, 12> out{};
    writeStereoToDevice(kLeft, kRight, 2, out.data(), 2, DeviceSampleFormat::Int24);
    CHECK(out[0] == 0x00 && out[1] == 0x00 && out[2] == 0x40); // +0.5 = 0x400000
    CHECK(out[9] == 0xFF && out[10] == 0xFF && out[11] == 0x7F); // Full positive
}

TEST_CASE(convert_int24_in_32_is_left_justified) {
    std::array<std::uint8_t, 16> out{};
    writeStereoToDevice(kLeft, kRight, 2, out.data(), 2, DeviceSampleFormat::Int24In32);
    CHECK(readAt<std::int32_t>(out.data(), 0) == 0x40000000);
}

TEST_CASE(convert_int32) {
    std::array<std::uint8_t, 16> out{};
    writeStereoToDevice(kLeft, kRight, 2, out.data(), 2, DeviceSampleFormat::Int32);
    CHECK(readAt<std::int32_t>(out.data(), 0) == 1073741824);
    CHECK(readAt<std::int32_t>(out.data(), 3) == 2147483647);
}

TEST_CASE(convert_mono_device_averages_both_sides) {
    const float left[] = {1.0f};
    const float right[] = {0.5f};
    std::array<std::uint8_t, 4> out{};
    writeStereoToDevice(left, right, 1, out.data(), 1, DeviceSampleFormat::Float32);
    CHECK(readAt<float>(out.data(), 0) == 0.75f);
}

TEST_CASE(convert_surround_device_silences_extra_channels) {
    std::array<float, 12> raw{};
    raw.fill(9.0f);
    writeStereoToDevice(kLeft, kRight, 2, reinterpret_cast<std::uint8_t*>(raw.data()), 6, DeviceSampleFormat::Float32);
    CHECK(raw[0] == 0.5f);
    CHECK(raw[1] == -0.5f);
    CHECK(raw[2] == 0.0f && raw[5] == 0.0f);
    CHECK(raw[6] == -1.0f);
}
