#include "core.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>

namespace utau {
namespace {

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
    return t == "R" || t == "r" || t == "rest" || t == "REST" || t == "pau" || t == "sil";
}

double midi_to_freq(const int midi_note) {
    return 440.0 * std::pow(2.0, (static_cast<double>(midi_note) - 69.0) / 12.0);
}

float peak_abs(const std::vector<float>& audio) {
    float peak = 0.0f;
    for (const float s : audio) {
        peak = std::max(peak, std::abs(s));
    }
    return peak;
}

}  // namespace

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
        if (t.empty() || starts_with(t, ";")) {
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

std::vector<float> render_project(const Project& project, const int sample_rate, const RenderOptions& options) {
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
        const int fade_samples = std::min(frames / 2, std::max(1, sample_rate / 200));

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
            out.push_back(raw * env * options.master_gain);
        }
    }

    if (options.normalize) {
        const float peak = peak_abs(out);
        constexpr float target_peak = 0.95f;
        if (peak > 0.0f && peak < target_peak) {
            const float scale = target_peak / peak;
            for (float& s : out) {
                s *= scale;
            }
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
    const std::uint16_t audio_format = 1;
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

bool render_ust_to_wav(const std::filesystem::path& ust_path,
                       const std::filesystem::path& out_wav,
                       const int sample_rate,
                       std::string& error,
                       const RenderOptions& options) {
    const auto project = parse_ust(ust_path, error);
    if (!project.has_value()) {
        return false;
    }

    const auto audio = render_project(*project, sample_rate, options);
    return write_wav_pcm16_mono(out_wav, audio, sample_rate, error);
}

}  // namespace utau
