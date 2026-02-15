#include "utau/engine/synthesis.hpp"

#include <algorithm>
#include <cmath>

namespace utau::engine {

SynthEngine::SynthEngine(RenderConfig config) : config_(config) {}

AudioBuffer SynthEngine::render(const SongProject& song, const Voicebank& voicebank) const {
    const double secPerTick = 60.0 / static_cast<double>(song.tempo * song.ticksPerBeat);

    std::size_t totalSamples = 0;
    for (const auto& note : song.notes) {
        const auto samples = static_cast<std::size_t>(std::max(1.0, note.lengthTicks * secPerTick * config_.sampleRate));
        totalSamples += samples;
    }

    AudioBuffer buffer;
    buffer.sampleRate = config_.sampleRate;
    buffer.samples.assign(totalSamples, 0.0f);

    std::size_t cursor = 0;
    for (const auto& note : song.notes) {
        const auto sampleCount = static_cast<std::size_t>(std::max(1.0, note.lengthTicks * secPerTick * config_.sampleRate));
        const auto entry = voicebank.lookup(note.lyric);

        const double frequency = midi_to_frequency(note.midiTone);
        const double attack = entry ? std::clamp(entry->preutterMs / 1000.0, 0.005, 0.08) : 0.01;
        const double release = entry ? std::clamp(entry->overlapMs / 1000.0, 0.005, 0.08) : 0.03;

        for (std::size_t i = 0; i < sampleCount; ++i) {
            const double t = static_cast<double>(i) / config_.sampleRate;
            constexpr double kPi = 3.14159265358979323846;
            const double phase = 2.0 * kPi * frequency * t;

            double env = 1.0;
            const double noteDuration = static_cast<double>(sampleCount) / config_.sampleRate;
            if (t < attack) {
                env = t / attack;
            } else if (t > noteDuration - release) {
                env = std::max(0.0, (noteDuration - t) / release);
            }

            const float v = static_cast<float>(std::sin(phase) * env * config_.masterGain);
            buffer.samples[cursor + i] += v;
        }

        cursor += sampleCount;
    }

    return buffer;
}

}  // namespace utau::engine
