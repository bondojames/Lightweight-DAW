// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace ldaw::engine {

struct DecodedFormat {
    double sampleRate = 0.0;
    std::uint32_t channels = 0;     // In the source file.
    std::int64_t frames = -1;       // -1 if the decoder can't tell in advance.
    std::string decoderName;
};

// Receives interleaved float samples, at most 2 channels. Returning false cancels.
using DecodeSink = std::function<bool(const float* interleaved, std::uint32_t frames, std::uint32_t channels)>;

// Streams a file through the sink, in chunks, so size has no effect on memory use.
// Audio files are untrusted input: anything malformed fails with a message.
// Sources with more than 2 channels are reduced to their first 2.
bool decodeAudioFile(const std::filesystem::path& path, DecodedFormat& formatOut, const DecodeSink& sink, std::string& errorOut);

// False on Windows editions without Media Foundation, such as N editions; then
// FLAC, MP3 and M4A can't be read, but WAV and AIFF still can.
bool mediaFoundationAvailable();

// Test helper: encodes float PCM with Windows' own encoders, for fixtures. The
// extension picks the format (.flac, .m4a, .mp3). False if the encoder is missing.
bool encodeWithMediaFoundation(const std::filesystem::path& path, const float* interleaved, std::uint32_t frames, std::uint32_t channels, std::uint32_t sampleRate);

} // namespace ldaw::engine
