#include "Voicebank.h"

#include <fstream>
#include <sstream>

namespace pyutau::core {

bool Voicebank::loadFromDirectory(const std::filesystem::path& path) {
    m_dictionary.clear();

    const auto otoPath = path / "oto.ini";
    std::ifstream input(otoPath);
    if (!input.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(input, line)) {
        const auto equalPos = line.find('=');
        if (equalPos == std::string::npos) {
            continue;
        }

        OtoEntry entry;
        entry.wav = line.substr(0, equalPos);

        const auto payload = line.substr(equalPos + 1);
        std::stringstream ss(payload);
        std::string segment;

        std::getline(ss, entry.alias, ',');
        std::getline(ss, segment, ',');
        entry.offsetMs = std::stod(segment.empty() ? "0" : segment);
        std::getline(ss, segment, ',');
        entry.consonantMs = std::stod(segment.empty() ? "0" : segment);
        std::getline(ss, segment, ',');
        entry.cutoffMs = std::stod(segment.empty() ? "0" : segment);
        std::getline(ss, segment, ',');
        entry.preutterMs = std::stod(segment.empty() ? "0" : segment);
        std::getline(ss, segment, ',');
        entry.overlapMs = std::stod(segment.empty() ? "0" : segment);

        m_dictionary[entry.alias] = entry;
    }

    return !m_dictionary.empty();
}

const OtoEntry* Voicebank::lookup(const std::string& lyric) const {
    const auto found = m_dictionary.find(lyric);
    if (found == m_dictionary.end()) {
        return nullptr;
    }
    return &found->second;
}

std::size_t Voicebank::size() const {
    return m_dictionary.size();
}

} // namespace pyutau::core
