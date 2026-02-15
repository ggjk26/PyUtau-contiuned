#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace {

struct NoteEvent {
    int length_ticks = 0;   // UTAU tick, 480 ticks = quarter note
    int note_num = 60;      // MIDI note number
    std::string lyric = "a";
};

struct Project {
    double tempo_bpm = 120.0;
    std::vector<NoteEvent> notes;
};

struct CliArgs {
    std::filesystem::path ust;
    std::filesystem::path out;
    int sample_rate = 44100;
    bool help = false;
};

std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

bool starts_with(const std::string& s, const std::string& prefix) {
    return s.rfind(prefix, 0) == 0;
}

std::optional<std::pair<std::string, std::string>> parse_key_value(const std::string& line) {
    const auto pos = line.find('=');
    if (pos == std::string::npos || pos == 0) {
        return std::nullopt;
    }
    return std::make_pair(trim(line.substr(0, pos)), trim(line.substr(pos + 1)));
}

std::optional<int> parse_int(const std::string& value) {
    try {
        size_t idx = 0;
        const int out = std::stoi(value, &idx);
        if (idx != value.size()) {
            return std::nullopt;
        }
        return out;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<double> parse_double(const std::string& value) {
    try {
        size_t idx = 0;
        const double out = std::stod(value, &idx);
        if (idx != value.size()) {
            return std::nullopt;
        }
        return out;
    } catch (...) {
        return std::nullopt;
    }
}

bool is_rest_lyric(const std::string& lyric) {
    const std::string t = trim(lyric);
    return t == "R" || t == "r" || t == "rest" || t == "REST";
}

std::optional<Project> parse_ust(const std::filesystem::path& ust_path, std::string& error) {
    std::ifstream input(ust_path);
    if (!input.is_open()) {
        error = "Cannot open UST file: " + ust_path.string();
        return std::nullopt;
    }

    Project project;
    std::string line;
    std::string current_section;
    std::map<std::string, std::string> section_map;

    auto flush_note_section = [&]() {
        if (!starts_with(current_section, "[#") || current_section == "[#SETTING]" || current_section == "[#TRACKEND]") {
            return;
        }
        if (!section_map.contains("Length") || !section_map.contains("NoteNum")) {
            return;
        }

        NoteEvent note;
        const auto length = parse_int(section_map["Length"]);
        const auto note_num = parse_int(section_map["NoteNum"]);
        if (!length.has_value() || !note_num.has_value()) {
            return;
        }

        note.length_ticks = *length;
        note.note_num = std::clamp(*note_num, 0, 127);
        if (section_map.contains("Lyric")) {
            note.lyric = section_map["Lyric"];
        }
        if (note.length_ticks > 0) {
            project.notes.push_back(note);
        }
    };

    while (std::getline(input, line)) {
        const auto t = trim(line);
        if (t.empty()) {
            continue;
        }

        if (starts_with(t, "[#") && t.back() == ']') {
            if (!current_section.empty()) {
                flush_note_section();
            }
            current_section = t;
            section_map.clear();
            continue;
        }

        const auto kv = parse_key_value(t);
        if (!kv.has_value()) {
            continue;
        }

        if (current_section == "[#SETTING]" && kv->first == "Tempo") {
            const auto tempo = parse_double(kv->second);
            if (tempo.has_value() && *tempo > 0.0 && *tempo < 1000.0) {
                project.tempo_bpm = *tempo;
            }
        } else {
            section_map[kv->first] = kv->second;
        }
    }

    if (!current_section.empty()) {
        flush_note_section();
    }

    if (project.notes.empty()) {
        error = "UST has no valid notes.";
        return std::nullopt;
    }

    return project;
}

double midi_to_freq(const int midi_note) {
    return 440.0 * std::pow(2.0, (static_cast<double>(midi_note) - 69.0) / 12.0);
}

std::vector<float> render_project(const Project& project, const int sample_rate) {
    constexpr double kTicksPerQuarter = 480.0;
    const double sec_per_tick = (60.0 / project.tempo_bpm) / kTicksPerQuarter;

    std::size_t total_frames = 0;
    for (const auto& note : project.notes) {
        const double duration_s = static_cast<double>(note.length_ticks) * sec_per_tick;
        total_frames += static_cast<std::size_t>(std::max(1, static_cast<int>(std::round(duration_s * sample_rate))));
    }

    std::vector<float> out;
    out.reserve(total_frames);

    double phase = 0.0;
    constexpr double two_pi = 6.28318530717958647692;

    for (const auto& note : project.notes) {
        const double duration_s = static_cast<double>(note.length_ticks) * sec_per_tick;
        const int frames = std::max(1, static_cast<int>(std::round(duration_s * sample_rate)));
        const int fade_samples = std::min(frames / 2, std::max(1, sample_rate / 200)); // ~5ms

        if (is_rest_lyric(note.lyric)) {
            out.insert(out.end(), static_cast<std::size_t>(frames), 0.0f);
            continue;
        }

        const double freq = midi_to_freq(note.note_num);

        for (int i = 0; i < frames; ++i) {
            const float raw = static_cast<float>(std::sin(phase));
            phase += two_pi * freq / static_cast<double>(sample_rate);
            if (phase >= two_pi) {
                phase = std::fmod(phase, two_pi);
            }

            float env = 1.0f;
            if (i < fade_samples) {
                env = static_cast<float>(i) / static_cast<float>(fade_samples);
            }
            const int tail = frames - 1 - i;
            if (tail < fade_samples) {
                const float tail_env = static_cast<float>(tail) / static_cast<float>(fade_samples);
                env = std::min(env, tail_env);
            }
            out.push_back(raw * env * 0.2f);
        }
    }

    return out;
}

bool write_wav_pcm16_mono(const std::filesystem::path& out_path,
                          const std::vector<float>& audio,
                          const int sample_rate,
                          std::string& error) {
    std::ofstream out(out_path, std::ios::binary);
    if (!out.is_open()) {
        error = "Cannot open output WAV: " + out_path.string();
        return false;
    }

    if (audio.size() > (std::numeric_limits<std::uint32_t>::max() / sizeof(std::int16_t))) {
        error = "Audio too long for 32-bit WAV data chunk.";
        return false;
    }

    const std::uint16_t channels = 1;
    const std::uint16_t bits_per_sample = 16;
    const std::uint32_t byte_rate = static_cast<std::uint32_t>(sample_rate) * channels * (bits_per_sample / 8);
    const std::uint16_t block_align = channels * (bits_per_sample / 8);
    const std::uint32_t data_size = static_cast<std::uint32_t>(audio.size() * sizeof(std::int16_t));
    const std::uint32_t riff_size = 36 + data_size;

    out.write("RIFF", 4);
    out.write(reinterpret_cast<const char*>(&riff_size), sizeof(riff_size));
    out.write("WAVE", 4);

    out.write("fmt ", 4);
    const std::uint32_t fmt_chunk_size = 16;
    const std::uint16_t audio_format = 1; // PCM
    out.write(reinterpret_cast<const char*>(&fmt_chunk_size), sizeof(fmt_chunk_size));
    out.write(reinterpret_cast<const char*>(&audio_format), sizeof(audio_format));
    out.write(reinterpret_cast<const char*>(&channels), sizeof(channels));
    out.write(reinterpret_cast<const char*>(&sample_rate), sizeof(sample_rate));
    out.write(reinterpret_cast<const char*>(&byte_rate), sizeof(byte_rate));
    out.write(reinterpret_cast<const char*>(&block_align), sizeof(block_align));
    out.write(reinterpret_cast<const char*>(&bits_per_sample), sizeof(bits_per_sample));

    out.write("data", 4);
    out.write(reinterpret_cast<const char*>(&data_size), sizeof(data_size));

    for (const auto s : audio) {
        const float clamped = std::clamp(s, -1.0f, 1.0f);
        const auto pcm = static_cast<std::int16_t>(std::lround(clamped * 32767.0f));
        out.write(reinterpret_cast<const char*>(&pcm), sizeof(pcm));
    }

    if (!out.good()) {
        error = "Failed while writing WAV bytes.";
        return false;
    }
    return true;
}

std::optional<CliArgs> parse_args(int argc, char** argv, std::string& error) {
    CliArgs args;
    for (int i = 1; i < argc; ++i) {
        const std::string k = argv[i];
        if (k == "--help" || k == "-h") {
            args.help = true;
            return args;
        }

        if ((k == "--ust" || k == "--out" || k == "--sr") && i + 1 >= argc) {
            error = "Missing value for argument: " + k;
            return std::nullopt;
        }

        if (k == "--ust") {
            args.ust = argv[++i];
        } else if (k == "--out") {
            args.out = argv[++i];
        } else if (k == "--sr") {
            const auto sr = parse_int(argv[++i]);
            if (!sr.has_value()) {
                error = "Invalid sample rate.";
                return std::nullopt;
            }
            args.sample_rate = std::clamp(*sr, 8000, 192000);
        } else {
            error = "Unknown argument: " + k;
            return std::nullopt;
        }
    }

    if (args.ust.empty() || args.out.empty()) {
        error = "Both --ust and --out are required.";
        return std::nullopt;
    }
    return args;
}

void print_usage() {
    std::cout << "Usage: utau_core_render --ust input.ust --out output.wav [--sr 44100]\n";
}

} // namespace

int main(int argc, char** argv) {
    std::string error;
    const auto args = parse_args(argc, argv, error);
    if (!args.has_value()) {
        if (!error.empty()) {
            std::cerr << "[args] " << error << "\n";
        }
        print_usage();
        return 1;
    }

    if (args->help) {
        print_usage();
        return 0;
    }

    const auto project = parse_ust(args->ust, error);
    if (!project.has_value()) {
        std::cerr << "[parse_ust] " << error << "\n";
        return 2;
    }

    const auto audio = render_project(*project, args->sample_rate);
    if (!write_wav_pcm16_mono(args->out, audio, args->sample_rate, error)) {
        std::cerr << "[write_wav] " << error << "\n";
        return 3;
    }

    std::cout << "Rendered " << project->notes.size() << " notes to " << args->out
              << " at " << args->sample_rate << " Hz\n";
    return 0;
}
