#include "AudioEngine.h"

#include <algorithm>
#include <cstdint>
#include <fstream>

namespace pyutau::audio {

namespace {

void writeU32(std::ofstream& out, std::uint32_t value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void writeU16(std::ofstream& out, std::uint16_t value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

} // namespace

bool AudioEngine::exportWav(const std::filesystem::path& filePath,
                            const std::vector<float>& monoPcm,
                            int sampleRate,
                            int bitsPerSample,
                            int channels) const {
    std::ofstream out(filePath, std::ios::binary);
    if (!out.is_open()) {
        return false;
    }

    const std::uint16_t safeChannels = static_cast<std::uint16_t>(std::clamp(channels, 1, 2));
    const std::uint16_t safeBits = static_cast<std::uint16_t>((bitsPerSample == 24 || bitsPerSample == 32) ? bitsPerSample : 16);
    const std::uint32_t bytesPerSample = safeBits / 8;
    const std::uint32_t byteRate = static_cast<std::uint32_t>(sampleRate) * safeChannels * bytesPerSample;
    const std::uint16_t blockAlign = safeChannels * static_cast<std::uint16_t>(bytesPerSample);
    const std::uint32_t dataSize = static_cast<std::uint32_t>(monoPcm.size() * safeChannels * bytesPerSample);

    out.write("RIFF", 4);
    writeU32(out, 36 + dataSize);
    out.write("WAVE", 4);

    out.write("fmt ", 4);
    writeU32(out, 16);
    writeU16(out, 1);
    writeU16(out, safeChannels);
    writeU32(out, static_cast<std::uint32_t>(sampleRate));
    writeU32(out, byteRate);
    writeU16(out, blockAlign);
    writeU16(out, safeBits);

    out.write("data", 4);
    writeU32(out, dataSize);

    auto writeSample = [&](float sample) {
        const auto clamped = std::clamp(sample, -1.0f, 1.0f);
        if (safeBits == 16) {
            const auto pcm = static_cast<std::int16_t>(clamped * 32767.0f);
            out.write(reinterpret_cast<const char*>(&pcm), sizeof(pcm));
        } else if (safeBits == 24) {
            const auto pcm = static_cast<std::int32_t>(clamped * 8388607.0f);
            const unsigned char bytes[3] = {
                static_cast<unsigned char>(pcm & 0xFF),
                static_cast<unsigned char>((pcm >> 8) & 0xFF),
                static_cast<unsigned char>((pcm >> 16) & 0xFF),
            };
            out.write(reinterpret_cast<const char*>(bytes), 3);
        } else {
            const auto pcm = static_cast<std::int32_t>(clamped * 2147483647.0f);
            out.write(reinterpret_cast<const char*>(&pcm), sizeof(pcm));
        }
    };

    for (float sample : monoPcm) {
        for (std::uint16_t ch = 0; ch < safeChannels; ++ch) {
            writeSample(sample);
        }
    }

    return true;
}

} // namespace pyutau::audio
