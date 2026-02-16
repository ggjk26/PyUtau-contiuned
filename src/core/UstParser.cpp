#include "UstParser.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace pyutau::core {
namespace {

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool startsWith(const std::string& text, const std::string& pattern) {
    return text.rfind(pattern, 0) == 0;
}

} // namespace

Project UstParser::parse(const std::filesystem::path& filePath) const {
    std::ifstream input(filePath);
    if (!input.is_open()) {
        throw std::runtime_error("Cannot open UST file: " + filePath.string());
    }

    Project project;
    project.setTitle(filePath.stem().string());
    auto& track = project.createTrack("Vocal");

    Note currentNote;
    bool inNote = false;
    int cursorTick = 0;

    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        if (startsWith(line, "[#")) {
            if (inNote) {
                currentNote.startTick = cursorTick;
                cursorTick += currentNote.durationTick;
                track.notes.push_back(currentNote);
                currentNote = {};
            }
            inNote = startsWith(line, "[#0") || startsWith(line, "[#1") || startsWith(line, "[#2") || startsWith(line, "[#3") || startsWith(line, "[#4") || startsWith(line, "[#5") || startsWith(line, "[#6") || startsWith(line, "[#7") || startsWith(line, "[#8") || startsWith(line, "[#9");
            continue;
        }

        const auto sep = line.find('=');
        if (sep == std::string::npos) {
            continue;
        }

        const auto key = line.substr(0, sep);
        const auto value = line.substr(sep + 1);

        if (key == "Tempo") {
            project.addTempo({.tick = 0, .bpm = std::stod(value)});
        } else if (inNote && key == "Length") {
            currentNote.durationTick = std::stoi(value);
        } else if (inNote && key == "Lyric") {
            currentNote.lyric = value;
        } else if (inNote && key == "NoteNum") {
            currentNote.pitch = std::stoi(value);
        } else if (inNote && key == "Velocity") {
            currentNote.velocity = std::stoi(value);
        }
    }

    if (inNote) {
        currentNote.startTick = cursorTick;
        track.notes.push_back(currentNote);
    }

    return project;
}

} // namespace pyutau::core
