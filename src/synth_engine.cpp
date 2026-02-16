#include "synth_engine.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace miniutau {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = kPi * 2.0;

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

struct SynthState {
    double phase = 0.0;
    double noise_lp = 0.0;
    double dc_prev_x = 0.0;
    double dc_prev_y = 0.0;
    double tone_lp = 0.0;
};

HarmonicProfile profile_for_lyric(const std::string& lyric) {
    if (lyric.find('a') != std::string::npos || lyric.find('A') != std::string::npos) {
        return {1.0, 0.45, 0.2, 0.08};
    }
    if (lyric.find('i') != std::string::npos || lyric.find('I') != std::string::npos) {
        return {1.0, 0.2, 0.35, 0.15};
    }
    if (lyric.find('u') != std::string::npos || lyric.find('U') != std::string::npos) {
        return {1.0, 0.25, 0.25, 0.2};
    }
    if (lyric.find('e') != std::string::npos || lyric.find('E') != std::string::npos) {
        return {1.0, 0.3, 0.28, 0.12};
    }
    if (lyric.find('o') != std::string::npos || lyric.find('O') != std::string::npos) {
        return {1.0, 0.5, 0.22, 0.06};
    }
    return {1.0, 0.3, 0.2, 0.1};
}

double smoothstep(double x) {
    const double c = std::clamp(x, 0.0, 1.0);
    return c * c * (3.0 - 2.0 * c);
}

double envelope_value(int i, int total_samples, int attack_samples, int release_samples) {
    if (total_samples <= 1) {
        return 0.0;
    }

    const int s = std::clamp(i, 0, total_samples - 1);
    const int attack = std::max(1, attack_samples);
    const int release = std::max(1, release_samples);

    if (s < attack) {
        return smoothstep(static_cast<double>(s) / static_cast<double>(attack));
    }

    const int release_start = std::max(0, total_samples - release);
    if (s >= release_start) {
        const double p = static_cast<double>(total_samples - s - 1) / static_cast<double>(release);
        return smoothstep(p);
    }

    return 1.0;
}

double soft_clip(double x) {
    // 比 tanh 更轻量的软削波，减少爆音且保留动态。
    return x / (1.0 + std::abs(x));
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

int estimate_total_samples(const UstProject& project, const RenderOptions& options) {
    constexpr double ticks_per_quarter = 480.0;
    const double sec_per_quarter = 60.0 / static_cast<double>(std::max(1, project.tempo));

    double total_sec = 0.0;
    for (const auto& note : project.notes) {
        const double dur = std::max(0.02, (static_cast<double>(note.length_ticks) / ticks_per_quarter) * sec_per_quarter);
        total_sec += dur;
    }

    return std::max(1, static_cast<int>(total_sec * options.sample_rate));
}

void render_note(Pcm16Wave& wave,
                 const Note& note,
                 const Note* prev_note,
                 int tempo,
                 const std::unordered_map<std::string, const OtoEntry*>& oto_index,
                 const RenderOptions& options,
                 NoiseGen& noise,
                 SynthState& state) {
    constexpr double ticks_per_quarter = 480.0;
    const double sec_per_quarter = 60.0 / static_cast<double>(std::max(1, tempo));

    double duration_sec = std::max(0.02,
        (static_cast<double>(note.length_ticks) / ticks_per_quarter) * sec_per_quarter);

    const auto it = oto_index.find(note.lyric);
    const OtoEntry* oto = (it != oto_index.end()) ? it->second : nullptr;

    if (note.lyric == "R") {
        const int rest_samples = std::max(1, static_cast<int>(duration_sec * options.sample_rate));
        wave.samples.insert(wave.samples.end(), rest_samples, 0);
        return;
    }

    if (oto != nullptr && oto->cutoff_ms > 0.0) {
        duration_sec = std::max(0.02, duration_sec - oto->cutoff_ms / 1000.0);
    }

    int samples = std::max(1, static_cast<int>(duration_sec * options.sample_rate));

    const double target_freq = midi_note_to_hz(note.note_num);
    const double prev_freq = (prev_note != nullptr && prev_note->lyric != "R")
        ? midi_note_to_hz(prev_note->note_num)
        : target_freq;

    const double preutter = (oto != nullptr) ? std::max(0.0, oto->preutter_ms / 1000.0) : 0.03;
    const double overlap = (oto != nullptr) ? std::max(0.0, oto->overlap_ms / 1000.0) : 0.0;

    const int consonant_samples = std::clamp(
        static_cast<int>(preutter * options.sample_rate),
        0,
        std::max(1, static_cast<int>(samples * 0.45)));

    const int overlap_samples = std::clamp(
        static_cast<int>(overlap * options.sample_rate),
        0,
        std::max(0, samples / 3));

    const int attack_samples = std::max(
        8,
        static_cast<int>((0.016 + (140 - std::clamp(note.velocity, 0, 200)) * 0.00018)
                         * options.sample_rate));

    const int release_samples = std::max(8, static_cast<int>(std::min(0.08, duration_sec * 0.4) * options.sample_rate));
    const int glide_samples = std::max(1, static_cast<int>(std::min(0.045, duration_sec * 0.25) * options.sample_rate));

    const HarmonicProfile hp = profile_for_lyric(note.lyric);
    const double harmonic_norm = std::max(1e-6, hp.h1 + hp.h2 + hp.h3 + hp.h4);

    for (int i = 0; i < samples; ++i) {
        const double pos = static_cast<double>(i) / static_cast<double>(samples);

        double curr_freq = target_freq;
        if (i < glide_samples && std::abs(prev_freq - target_freq) > 1e-3) {
            const double r = smoothstep(static_cast<double>(i) / static_cast<double>(glide_samples));
            curr_freq = prev_freq + (target_freq - prev_freq) * r;
        }

        if (pos > 0.62) {
            const double vib_progress = smoothstep((pos - 0.62) / 0.38);
            const double vib_hz = 5.8;
            const double vib_cents = 18.0 * vib_progress * std::sin(kTwoPi * vib_hz * (static_cast<double>(i) / options.sample_rate));
            curr_freq *= std::pow(2.0, vib_cents / 1200.0);
        }

        state.phase += kTwoPi * curr_freq / static_cast<double>(options.sample_rate);
        if (state.phase >= kTwoPi) {
            state.phase = std::fmod(state.phase, kTwoPi);
        }

        const double s1 = std::sin(state.phase);
        const double s2 = std::sin(2.0 * state.phase);
        const double s3 = std::sin(3.0 * state.phase);
        const double s4 = std::sin(4.0 * state.phase);

        double voiced = (hp.h1 * s1 + hp.h2 * s2 + hp.h3 * s3 + hp.h4 * s4) / harmonic_norm;

        if (i < consonant_samples && consonant_samples > 0) {
            const double cp = static_cast<double>(i) / static_cast<double>(consonant_samples);
            const double mix = 1.0 - smoothstep(cp);

            // 低通噪声，让辅音不过于刺耳。
            state.noise_lp = state.noise_lp * 0.85 + noise.next() * 0.15;
            voiced = voiced * (0.45 + 0.55 * cp) + state.noise_lp * 0.35 * mix;
        }

        // overlap 不再插入静音；改为提前进入稳定段，减少断裂感。
        if (overlap_samples > 0 && i < overlap_samples) {
            voiced *= 0.88 + 0.12 * static_cast<double>(i) / static_cast<double>(overlap_samples);
        }

        double amp = options.master_volume;
        if (options.enable_simple_envelope) {
            amp *= envelope_value(i, samples, attack_samples, release_samples);
        }

        // 轻微音色平滑（一阶低通），提升听感。
        state.tone_lp = state.tone_lp * 0.78 + voiced * 0.22;
        double y = state.tone_lp * amp;

        // DC block，避免低频偏置带来的闷和爆。
        const double dc_alpha = 0.995;
        const double dc = y - state.dc_prev_x + dc_alpha * state.dc_prev_y;
        state.dc_prev_x = y;
        state.dc_prev_y = dc;

        const double out = soft_clip(dc * 1.15);
        const auto pcm = static_cast<int16_t>(std::clamp(out * 32767.0, -32768.0, 32767.0));
        wave.samples.push_back(pcm);
    }
}

} // namespace

Pcm16Wave SynthEngine::render(const UstProject& project,
                              const std::vector<OtoEntry>& oto_entries,
                              const RenderOptions& options) const {
    Pcm16Wave wave;
    wave.sample_rate = options.sample_rate;
    wave.samples.reserve(static_cast<size_t>(estimate_total_samples(project, options)));

    std::unordered_map<std::string, const OtoEntry*> oto_index;
    oto_index.reserve(oto_entries.size());
    for (const auto& entry : oto_entries) {
        oto_index[entry.alias] = &entry;
    }

    NoiseGen noise;
    SynthState state;
    for (size_t i = 0; i < project.notes.size(); ++i) {
        const Note* prev = i > 0 ? &project.notes[i - 1] : nullptr;
        render_note(wave, project.notes[i], prev, project.tempo, oto_index, options, noise, state);
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
