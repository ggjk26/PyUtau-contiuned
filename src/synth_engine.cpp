#include "synth_engine.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <stdexcept>

namespace miniutau {
namespace {

constexpr double kPi = 3.14159265358979323846;

void append_note(Pcm16Wave& wave, const Note& note, int tempo, const RenderOptions& options) {
    constexpr double ticks_per_quarter = 480.0;
    const double sec_per_quarter = 60.0 / static_cast<double>(tempo);
    const double duration_sec = (static_cast<double>(note.length_ticks) / ticks_per_quarter) * sec_per_quarter;
    const int samples = std::max(1, static_cast<int>(duration_sec * options.sample_rate));

    if (note.lyric == "R") {
        wave.samples.insert(wave.samples.end(), samples, 0);
        return;
    }

    const double freq = midi_note_to_hz(note.note_num);
    for (int i = 0; i < samples; ++i) {
        double t = static_cast<double>(i) / static_cast<double>(options.sample_rate);
        double amp = options.master_volume;
        if (options.enable_simple_envelope) {
            const double pos = static_cast<double>(i) / static_cast<double>(samples);
            if (pos < 0.05) {
                amp *= (pos / 0.05);
            } else if (pos > 0.9) {
                amp *= ((1.0 - pos) / 0.1);
            }
            amp = std::max(0.0, amp);
        }
        const double sample = std::sin(2.0 * kPi * freq * t) * amp;
        const auto pcm = static_cast<int16_t>(std::clamp(sample * 32767.0, -32768.0, 32767.0));
        wave.samples.push_back(pcm);
    }
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

} // namespace

Pcm16Wave SynthEngine::render(const UstProject& project, const RenderOptions& options) const {
    Pcm16Wave wave;
    wave.sample_rate = options.sample_rate;

    for (const auto& note : project.notes) {
        append_note(wave, note, project.tempo, options);
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
