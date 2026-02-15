#include "utau/engine/wav_writer.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <stdexcept>

namespace utau::engine {

void write_wav(const AudioBuffer& buffer, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("failed to write wav: " + path);
    }

    const std::uint16_t channels = 1;
    const std::uint16_t bitsPerSample = 16;
    const std::uint32_t byteRate = buffer.sampleRate * channels * bitsPerSample / 8;
    const std::uint16_t blockAlign = channels * bitsPerSample / 8;
    const std::uint32_t dataSize = static_cast<std::uint32_t>(buffer.samples.size() * sizeof(std::int16_t));
    const std::uint32_t riffSize = 36 + dataSize;

    out.write("RIFF", 4);
    out.write(reinterpret_cast<const char*>(&riffSize), 4);
    out.write("WAVE", 4);

    out.write("fmt ", 4);
    const std::uint32_t fmtSize = 16;
    const std::uint16_t audioFormat = 1;
    out.write(reinterpret_cast<const char*>(&fmtSize), 4);
    out.write(reinterpret_cast<const char*>(&audioFormat), 2);
    out.write(reinterpret_cast<const char*>(&channels), 2);
    out.write(reinterpret_cast<const char*>(&buffer.sampleRate), 4);
    out.write(reinterpret_cast<const char*>(&byteRate), 4);
    out.write(reinterpret_cast<const char*>(&blockAlign), 2);
    out.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    out.write("data", 4);
    out.write(reinterpret_cast<const char*>(&dataSize), 4);

    for (const float sample : buffer.samples) {
        const float clamped = std::clamp(sample, -1.0f, 1.0f);
        const auto pcm = static_cast<std::int16_t>(clamped * 32767.0f);
        out.write(reinterpret_cast<const char*>(&pcm), sizeof(pcm));
    }
}

}  // namespace utau::engine
