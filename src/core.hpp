#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace utau {

struct NoteEvent {
    int length_ticks = 0;
    int note_num = 60;
    std::string lyric = "a";
};

struct Project {
    double tempo_bpm = 120.0;
    std::vector<NoteEvent> notes;
};

struct RenderOptions {
    float master_gain = 0.2f;
    bool normalize = true;
};

std::optional<Project> parse_ust(const std::filesystem::path& ust_path, std::string& error);
std::vector<float> render_project(const Project& project, int sample_rate, const RenderOptions& options = {});
bool write_wav_pcm16_mono(const std::filesystem::path& out_path,
                          const std::vector<float>& audio,
                          int sample_rate,
                          std::string& error);

bool render_ust_to_wav(const std::filesystem::path& ust_path,
                       const std::filesystem::path& out_wav,
                       int sample_rate,
                       std::string& error,
                       const RenderOptions& options = {});

}  // namespace utau
