#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace utau::engine {

struct OtoEntry {
    std::string wavFile;
    std::string alias;
    double offsetMs = 0.0;
    double consonantMs = 0.0;
    double cutoffMs = 0.0;
    double preutterMs = 0.0;
    double overlapMs = 0.0;
};

struct Note {
    int index = 0;
    int lengthTicks = 480;
    int midiTone = 60;
    int velocity = 100;
    std::string lyric = "a";
};

struct SongProject {
    int tempo = 120;
    int ticksPerBeat = 480;
    std::vector<Note> notes;
};

struct RenderConfig {
    int sampleRate = 44100;
    int channels = 1;
    double masterGain = 0.2;
};

struct AudioBuffer {
    int sampleRate = 44100;
    std::vector<float> samples;
};

inline double midi_to_frequency(int midi) {
    return 440.0 * std::pow(2.0, (static_cast<double>(midi) - 69.0) / 12.0);
}

}  // namespace utau::engine
