#include "Resampler.h"

#include <cmath>

namespace pyutau::core {

std::vector<float> Resampler::renderTrack(const Track& track,
                                          const Voicebank& voicebank,
                                          int sampleRate) const {
    std::vector<float> pcm;

    constexpr double pi = 3.14159265358979323846;
    const auto ticksPerQuarter = 480.0;
    const auto defaultBpm = 120.0;

    for (const auto& note : track.notes) {
        (void)voicebank.lookup(note.lyric);
        const auto seconds = (static_cast<double>(note.durationTick) / ticksPerQuarter) * (60.0 / defaultBpm);
        const auto frameCount = static_cast<int>(seconds * sampleRate);
        const auto frequency = 440.0 * std::pow(2.0, (note.pitch - 69) / 12.0);

        for (int i = 0; i < frameCount; ++i) {
            const auto t = static_cast<double>(i) / sampleRate;
            const auto sample = static_cast<float>(0.15 * std::sin(2.0 * pi * frequency * t));
            pcm.push_back(sample);
        }
    }

    return pcm;
}

} // namespace pyutau::core
