#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace pyutau::core {

struct Note {
    int pitch = 60;
    int startTick = 0;
    int durationTick = 480;
    int velocity = 100;
    std::string lyric = "a";
};

struct TempoPoint {
    int tick = 0;
    double bpm = 120.0;
};

struct Track {
    std::string name = "Track";
    std::string voicebankId;
    float gain = 1.0F;
    bool muted = false;
    std::vector<Note> notes;
};

class Project {
public:
    void setTitle(std::string value);
    [[nodiscard]] const std::string& title() const;

    void setResolution(int tpqn);
    [[nodiscard]] int resolution() const;

    void addTempo(TempoPoint tempo);
    [[nodiscard]] const std::vector<TempoPoint>& tempoMap() const;

    Track& createTrack(std::string name);
    [[nodiscard]] std::vector<Track>& tracks();
    [[nodiscard]] const std::vector<Track>& tracks() const;

    [[nodiscard]] std::optional<Track*> firstTrack();

private:
    std::string m_title = "Untitled";
    int m_resolution = 480;
    std::vector<TempoPoint> m_tempos{{0, 120.0}};
    std::vector<Track> m_tracks;
};

} // namespace pyutau::core
