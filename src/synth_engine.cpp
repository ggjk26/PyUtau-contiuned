#include "synth_engine.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>

namespace miniutau {
namespace {

constexpr double kPi = 3.14159265358979323846;

struct HarmonicProfile {
    double h1 = 1.0;
    double h2 = 0.2;
    double h3 = 0.1;
    double h4 = 0.05;
};

class NoiseGen {
public:
    explicit NoiseGen(uint32_t seed = 0x12345678u) : state_(seed) {}

    double next() {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return (static_cast<double>(state_ & 0xFFFFu) / 32768.0) - 1.0;
    }

private:
    uint32_t state_;
};

std::optional<OtoEntry> lookup_oto(const std::vector<OtoEntry>& entries, const std::string& lyric) {
    for (const auto& e : entries) {
        if (e.alias == lyric) {
            return e;
        }
    }
    return std::nullopt;
}

HarmonicProfile profile_for_lyric(const std::string& lyric) {
    if (lyric.find('a') != std::string::npos || lyric.find("A") != std::string::npos) {
        return {1.0, 0.45, 0.2, 0.08};
    }
    if (lyric.find('i') != std::string::npos || lyric.find("I") != std::string::npos) {
        return {1.0, 0.2, 0.35, 0.15};
    }
    if (lyric.find('u') != std::string::npos || lyric.find("U") != std::string::npos) {
        return {1.0, 0.25, 0.25, 0.2};
    }
    if (lyric.find('e') != std::string::npos || lyric.find("E") != std::string::npos) {
        return {1.0, 0.3, 0.28, 0.12};
    }
    if (lyric.find('o') != std::string::npos || lyric.find("O") != std::string::npos) {
        return {1.0, 0.5, 0.22, 0.06};
    }
    return {1.0, 0.3, 0.2, 0.1};
}

double envelope(double pos, double attack_s, double release_s, double total_s) {
    if (total_s <= 0.0) {
        return 0.0;
    }
    const double t = pos * total_s;
    if (t < attack_s && attack_s > 1e-6) {
        return t / attack_s;
    }

    const double release_start = std::max(0.0, total_s - release_s);
    if (t > release_start && release_s > 1e-6) {
        return std::max(0.0, (total_s - t) / release_s);
    }

    return 1.0;
}

void write_u16(std::ofstream& out, std::uint16_t value) {
    out.put(static_cast<char>(value & 0xFF));
    out.put(static_cast<char>((value >> 8) & 0xFF));
}

void write_u32(std::ofstream& out, std::uint32_t value) {
    out.put(static_cast<char>(value & 0xFF));
    out.put(static_cast<char>((value >> 8) & 0xFF));
    out.put(static_cast<char>((value >> 16) & 0xFF));
    out.put(static_cast<char>((value >> 24) & 0xFF));
}

void append_note(Pcm16Wave& wave,
                 const Note& note,
                 const Note* prev_note,
                 int tempo,
                 const std::vector<OtoEntry>& oto_entries,
                 const RenderOptions& options,
                 NoiseGen& noise) {
    constexpr double ticks_per_quarter = 480.0;
    const double sec_per_quarter = 60.0 / static_cast<double>(std::max(1, tempo));

    double duration_sec = (static_cast<double>(note.length_ticks) / ticks_per_quarter) * sec_per_quarter;
    duration_sec = std::max(0.02, duration_sec);

    if (note.lyric == "R") {
        const int rest_samples = std::max(1, static_cast<int>(duration_sec * options.sample_rate));
        wave.samples.insert(wave.samples.end(), rest_samples, 0);
        return;
    }

    const auto oto = lookup_oto(oto_entries, note.lyric);
    if (oto && oto->cutoff_ms > 0.0) {
        duration_sec = std::max(0.02, duration_sec - oto->cutoff_ms / 1000.0);
    }

    const int samples = std::max(1, static_cast<int>(duration_sec * options.sample_rate));
    const double target_freq = midi_note_to_hz(note.note_num);

    double prev_freq = target_freq;
    if (prev_note != nullptr && prev_note->lyric != "R") {
        prev_freq = midi_note_to_hz(prev_note->note_num);
    }

    const double consonant_sec = std::clamp(
        oto ? std::max(0.0, oto->preutter_ms / 1000.0) : 0.03,
        0.0,
        duration_sec * 0.4);
    const int consonant_samples = static_cast<int>(consonant_sec * options.sample_rate);

    const double base_attack = std::clamp(0.02 + (140 - std::clamp(note.velocity, 0, 200)) * 0.0002, 0.005, 0.045);
    const double release = std::min(0.06, duration_sec * 0.35);
    const double glide_sec = std::min(0.04, duration_sec * 0.25);

    const HarmonicProfile hp = profile_for_lyric(note.lyric);
    const double harmonic_norm = std::max(1e-6, hp.h1 + hp.h2 + hp.h3 + hp.h4);

    for (int i = 0; i < samples; ++i) {
        const double pos = static_cast<double>(i) / static_cast<double>(samples);
        const double t = static_cast<double>(i) / static_cast<double>(options.sample_rate);

        double curr_freq = target_freq;
        if (t < glide_sec && std::abs(prev_freq - target_freq) > 1e-3) {
            const double r = t / std::max(1e-6, glide_sec);
            curr_freq = prev_freq + (target_freq - prev_freq) * r;
        }

        if (pos > 0.65) {
            const double vib_pos = (pos - 0.65) / 0.35;
            const double vib_cents = 22.0 * std::sin(2.0 * kPi * 5.5 * t) * vib_pos;
            curr_freq *= std::pow(2.0, vib_cents / 1200.0);
        }

        double voiced = (
            hp.h1 * std::sin(2.0 * kPi * curr_freq * t) +
            hp.h2 * std::sin(2.0 * kPi * curr_freq * 2.0 * t) +
            hp.h3 * std::sin(2.0 * kPi * curr_freq * 3.0 * t) +
            hp.h4 * std::sin(2.0 * kPi * curr_freq * 4.0 * t)
        ) / harmonic_norm;

        double consonant_mix = 0.0;
        if (i < consonant_samples && consonant_samples > 0) {
            const double cp = static_cast<double>(i) / static_cast<double>(consonant_samples);
            consonant_mix = 1.0 - cp;
            voiced = voiced * (0.3 + cp * 0.7) + noise.next() * 0.5 * consonant_mix;
        }

        double amp = options.master_volume;
        if (options.enable_simple_envelope) {
            amp *= envelope(pos, base_attack, release, duration_sec);
        }

        const double sample = voiced * amp;
        const auto pcm = static_cast<int16_t>(std::clamp(sample * 32767.0, -32768.0, 32767.0));
        wave.samples.push_back(pcm);
    }

    if (oto && oto->overlap_ms > 0.0) {
        const int overlap_silence = std::min(static_cast<int>(oto->overlap_ms / 1000.0 * options.sample_rate),
                                             options.sample_rate / 200);
        if (overlap_silence > 0) {
            wave.samples.insert(wave.samples.end(), overlap_silence, 0);
        }
    }
}

} // namespace

Pcm16Wave SynthEngine::render(const UstProject& project,
                              const std::vector<OtoEntry>& oto_entries,
                              const RenderOptions& options) const {
    Pcm16Wave wave;
    wave.sample_rate = options.sample_rate;

    NoiseGen noise;
    for (size_t i = 0; i < project.notes.size(); ++i) {
        const Note* prev = i > 0 ? &project.notes[i - 1] : nullptr;
        append_note(wave, project.notes[i], prev, project.tempo, oto_entries, options, noise);
    }

    return wave;
}

void write_wave_file(const Pcm16Wave& wave, const char* output_path) {
    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("无法写入WAV文件");
    }

    const std::uint32_t data_size = static_cast<std::uint32_t>(wave.samples.size() * sizeof(std::int16_t));
    const std::uint32_t chunk_size = 36 + data_size;
    const std::uint16_t channels = 1;
    const std::uint16_t bits_per_sample = 16;
    const std::uint32_t byte_rate = wave.sample_rate * channels * bits_per_sample / 8;
    const std::uint16_t block_align = channels * bits_per_sample / 8;

    out.write("RIFF", 4);
    write_u32(out, chunk_size);
    out.write("WAVE", 4);

    out.write("fmt ", 4);
    write_u32(out, 16);
    write_u16(out, 1);
    write_u16(out, channels);
    write_u32(out, static_cast<std::uint32_t>(wave.sample_rate));
    write_u32(out, byte_rate);
    write_u16(out, block_align);
    write_u16(out, bits_per_sample);

    out.write("data", 4);
    write_u32(out, data_size);
    out.write(reinterpret_cast<const char*>(wave.samples.data()), static_cast<std::streamsize>(data_size));
}

double midi_note_to_hz(int note_num) {
    return 440.0 * std::pow(2.0, (static_cast<double>(note_num) - 69.0) / 12.0);
}

} // namespace miniutau
