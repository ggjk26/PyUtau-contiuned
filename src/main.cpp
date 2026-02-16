#include "oto_parser.hpp"
#include "synth_engine.hpp"
#include "ust_parser.hpp"

#include <exception>
#include <iostream>
#include <string>

using namespace miniutau;

namespace {

void print_usage() {
    std::cout << "Mini UTAU Core (no GUI)\n"
              << "用法:\n"
              << "  mini_utau --ust <input.ust> --out <output.wav> [--oto <voice/oto.ini>]\n";
}

} // namespace

int main(int argc, char** argv) {
    std::string ust_path;
    std::string oto_path;
    std::string out_path = "output.wav";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--ust" && i + 1 < argc) {
            ust_path = argv[++i];
        } else if (arg == "--oto" && i + 1 < argc) {
            oto_path = argv[++i];
        } else if (arg == "--out" && i + 1 < argc) {
            out_path = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            print_usage();
            return 0;
        }
    }

    if (ust_path.empty()) {
        print_usage();
        return 1;
    }

    try {
        UstParser ust_parser;
        UstProject project = ust_parser.parse_file(ust_path);

        std::vector<OtoEntry> oto_entries;
        if (!oto_path.empty()) {
            OtoParser oto_parser;
            oto_entries = oto_parser.parse_file(oto_path);
        }

        std::cout << "Tempo: " << project.tempo << "\n";
        std::cout << "Notes: " << project.notes.size() << "\n";
        std::cout << "Loaded oto entries: " << oto_entries.size() << "\n";

        SynthEngine synth;
        RenderOptions options;
        Pcm16Wave wave = synth.render(project, options);
        write_wave_file(wave, out_path.c_str());

        std::cout << "Rendered wav: " << out_path << "\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
