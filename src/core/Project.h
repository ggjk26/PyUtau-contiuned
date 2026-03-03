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

    // Manual pitch line points in cents sampled uniformly across the note duration.
    std::vector<int> pitchBendCents;

    // Automatic pitch-line enhancement controls.
    bool autoPitchEnabled = true;
    int autoVibratoDepthCents = 18;
    double autoVibratoRateHz = 5.5;
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
    int phonemizerType = 0; // 0=JP VCV/CVVC, 1=DiffSinger JP, 2=PyUTAU Native
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
