#include "utau/engine/synthesis.hpp"
#include "utau/engine/ust.hpp"
#include "utau/engine/voicebank.hpp"
#include "utau/engine/wav_writer.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: utau_cli <song.ust> <oto.ini> <output.wav>\n";
        return 1;
    }

    try {
        const std::string ustPath = argv[1];
        const std::string otoPath = argv[2];
        const std::string outPath = argv[3];

        auto song = utau::engine::UstParser::parse_file(ustPath);
        utau::engine::Voicebank voicebank;
        voicebank.load_oto(otoPath);

        utau::engine::SynthEngine synth;
        auto buffer = synth.render(song, voicebank);
        utau::engine::write_wav(buffer, outPath);

        std::cout << "Rendered " << song.notes.size() << " note(s) to " << outPath << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 2;
    }
}
