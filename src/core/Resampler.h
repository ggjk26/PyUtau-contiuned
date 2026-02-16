#pragma once

#include "Project.h"
#include "Voicebank.h"

#include <cstdint>
#include <vector>

namespace pyutau::core {

class Resampler {
public:
    [[nodiscard]] std::vector<float> renderTrack(const Track& track,
                                                 const Voicebank& voicebank,
                                                 int sampleRate = 44100) const;
};

} // namespace pyutau::core
