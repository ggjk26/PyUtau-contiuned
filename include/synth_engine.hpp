#pragma once

#include "utau_types.hpp"

#include <vector>

namespace miniutau {

class SynthEngine {
public:
    Pcm16Wave render(const UstProject& project,
                     const std::vector<OtoEntry>& oto_entries,
                     const RenderOptions& options) const;
};

void write_wave_file(const Pcm16Wave& wave, const char* output_path);

double midi_note_to_hz(int note_num);

} // namespace miniutau
