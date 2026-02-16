#include "Project.h"

#include <algorithm>
#include <utility>

namespace pyutau::core {

void Project::setTitle(std::string value) {
    m_title = std::move(value);
}

const std::string& Project::title() const {
    return m_title;
}

void Project::setResolution(int tpqn) {
    m_resolution = std::max(1, tpqn);
}

int Project::resolution() const {
    return m_resolution;
}

void Project::addTempo(TempoPoint tempo) {
    m_tempos.push_back(tempo);
    std::ranges::sort(m_tempos, [](const TempoPoint& lhs, const TempoPoint& rhs) {
        return lhs.tick < rhs.tick;
    });
}

const std::vector<TempoPoint>& Project::tempoMap() const {
    return m_tempos;
}

Track& Project::createTrack(std::string name) {
    m_tracks.push_back({.name = std::move(name), .notes = {}});
    return m_tracks.back();
}

std::vector<Track>& Project::tracks() {
    return m_tracks;
}

const std::vector<Track>& Project::tracks() const {
    return m_tracks;
}

std::optional<Track*> Project::firstTrack() {
    if (m_tracks.empty()) {
        return std::nullopt;
    }
    return &m_tracks.front();
}

} // namespace pyutau::core
