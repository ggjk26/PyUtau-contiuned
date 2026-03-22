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
                            int sampleRate) const {
    std::ofstream out(filePath, std::ios::binary);
    if (!out.is_open()) {
        return false;
    }

    const std::uint16_t channels = 1;
    const std::uint16_t bitsPerSample = 16;
    const std::uint32_t byteRate = sampleRate * channels * bitsPerSample / 8;
    const std::uint16_t blockAlign = channels * bitsPerSample / 8;
    const std::uint32_t dataSize = static_cast<std::uint32_t>(monoPcm.size() * sizeof(std::int16_t));

    out.write("RIFF", 4);
    writeU32(out, 36 + dataSize);
    out.write("WAVE", 4);

    out.write("fmt ", 4);
    writeU32(out, 16);
    writeU16(out, 1);
    writeU16(out, channels);
    writeU32(out, sampleRate);
    writeU32(out, byteRate);
    writeU16(out, blockAlign);
    writeU16(out, bitsPerSample);

    out.write("data", 4);
    writeU32(out, dataSize);

    for (float sample : monoPcm) {
        const auto clamped = std::clamp(sample, -1.0f, 1.0f);
        const auto pcm16 = static_cast<std::int16_t>(clamped * 32767.0f);
        out.write(reinterpret_cast<const char*>(&pcm16), sizeof(pcm16));
    }

    return true;
}

} // namespace pyutau::audio
