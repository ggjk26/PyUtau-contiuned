#pragma once

#include "Project.h"
#include "Voicebank.h"

#include <cstdint>
#include <vector>

namespace pyutau::core {

struct RenderResult {
    std::vector<float> pcm;
    std::size_t workerThreads = 1;
    double elapsedMs = 0.0;
};

enum class RenderBackendType {
    OpenGL = 0,
    Vulkan = 1,
};

struct RenderRequest {
    const Track* track = nullptr;
    const Voicebank* voicebank = nullptr;
    int sampleRate = 44100;
    unsigned int maxThreads = 0;
    RenderBackendType backend = RenderBackendType::OpenGL;
    bool enableAutoPitchPrediction = true;
    bool enableSmoothTransition = true;
};

class Resampler {
public:
    [[nodiscard]] RenderResult renderTrack(const RenderRequest& request) const;
    [[nodiscard]] RenderResult renderTrack(const Track& track,
                                           const Voicebank& voicebank,
                                           int sampleRate = 44100,
                                           unsigned int maxThreads = 0) const;
};

} // namespace pyutau::core
