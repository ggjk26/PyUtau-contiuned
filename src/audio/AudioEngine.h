#pragma once

#include <filesystem>
#include <vector>

namespace pyutau::audio {

class AudioEngine {
public:
    bool exportWav(const std::filesystem::path& filePath,
                   const std::vector<float>& monoPcm,
                   int sampleRate = 44100,
                   int bitsPerSample = 16,
                   int channels = 1) const;
};

} // namespace pyutau::audio
