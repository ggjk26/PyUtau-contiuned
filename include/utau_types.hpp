#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace miniutau {

struct OtoEntry {
    std::string wav_file;
    std::string alias;
    double offset_ms = 0.0;
    double consonant_ms = 0.0;
    double cutoff_ms = 0.0;
    double preutter_ms = 0.0;
    double overlap_ms = 0.0;
};

struct Note {
    std::string lyric = "R";
    int length_ticks = 480;
    int note_num = 60;
    int velocity = 100;
};

struct UstProject {
    int tempo = 120;
    std::vector<Note> notes;
};

struct RenderOptions {
    int sample_rate = 44100;
    double master_volume = 0.4;
    bool enable_simple_envelope = true;
};

struct Pcm16Wave {
    int sample_rate = 44100;
    std::vector<int16_t> samples;
};

} // namespace miniutau
