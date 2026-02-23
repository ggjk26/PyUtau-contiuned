#include "UstParser.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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

std::string toLower(std::string text) {
    for (char& ch : text) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return text;
}

std::size_t findArrayStart(const std::string& text, const std::string& key, std::size_t offset = 0) {
    const auto keyPos = text.find("\"" + key + "\"", offset);
    if (keyPos == std::string::npos) {
        return std::string::npos;
    }
    return text.find('[', keyPos);
}

std::size_t findMatching(const std::string& text, std::size_t start, char openToken, char closeToken) {
    int depth = 0;
    for (std::size_t i = start; i < text.size(); ++i) {
        if (text[i] == openToken) {
            ++depth;
        } else if (text[i] == closeToken) {
            --depth;
            if (depth == 0) {
                return i;
            }
        }
    }
    return std::string::npos;
}

std::vector<std::string> extractObjectsFromArray(const std::string& text,
                                                 const std::string& arrayKey,
                                                 std::size_t offset = 0) {
    std::vector<std::string> objects;
    const auto arrayStart = findArrayStart(text, arrayKey, offset);
    if (arrayStart == std::string::npos) {
        return objects;
    }

    const auto arrayEnd = findMatching(text, arrayStart, '[', ']');
    if (arrayEnd == std::string::npos) {
        return objects;
    }

    int depth = 0;
    std::size_t objStart = std::string::npos;
    for (std::size_t i = arrayStart + 1; i < arrayEnd; ++i) {
        if (text[i] == '{') {
            if (depth == 0) {
                objStart = i;
            }
            ++depth;
        } else if (text[i] == '}') {
            --depth;
            if (depth == 0 && objStart != std::string::npos) {
                objects.push_back(text.substr(objStart, i - objStart + 1));
                objStart = std::string::npos;
            }
        }
    }

    return objects;
}

std::string extractStringValue(const std::string& object, const std::string& key) {
    const auto keyPos = object.find("\"" + key + "\"");
    if (keyPos == std::string::npos) {
        return {};
    }

    const auto colon = object.find(':', keyPos);
    if (colon == std::string::npos) {
        return {};
    }

    const auto begin = object.find('"', colon + 1);
    if (begin == std::string::npos) {
        return {};
    }

    const auto end = object.find('"', begin + 1);
    if (end == std::string::npos) {
        return {};
    }

    return object.substr(begin + 1, end - begin - 1);
}

int extractIntValue(const std::string& object, const std::string& key, int fallback) {
    const auto keyPos = object.find("\"" + key + "\"");
    if (keyPos == std::string::npos) {
        return fallback;
    }

    const auto colon = object.find(':', keyPos);
    if (colon == std::string::npos) {
        return fallback;
    }

    std::size_t begin = colon + 1;
    while (begin < object.size() && std::isspace(static_cast<unsigned char>(object[begin]))) {
        ++begin;
    }

    std::size_t end = begin;
    while (end < object.size() && (std::isdigit(static_cast<unsigned char>(object[end])) || object[end] == '-' || object[end] == '+')) {
        ++end;
    }

    if (end == begin) {
        return fallback;
    }

    return std::stoi(object.substr(begin, end - begin));
}

std::vector<int> parseCsvInts(const std::string& text) {
    std::vector<int> values;
    std::stringstream ss(text);
    std::string token;
    while (std::getline(ss, token, ',')) {
        token = trim(token);
        if (token.empty()) {
            continue;
        }
        values.push_back(std::stoi(token));
    }
    return values;
}

std::vector<int> extractIntArrayValue(const std::string& object, const std::string& key) {
    const auto keyPos = object.find("\"" + key + "\"");
    if (keyPos == std::string::npos) {
        return {};
    }
    const auto start = object.find('[', keyPos);
    if (start == std::string::npos) {
        return {};
    }
    const auto end = findMatching(object, start, "["[0], "]"[0]);
    if (end == std::string::npos || end <= start + 1) {
        return {};
    }
    return parseCsvInts(object.substr(start + 1, end - start - 1));
}

double extractDoubleValue(const std::string& text, const std::string& key, double fallback) {
    const auto keyPos = text.find("\"" + key + "\"");
    if (keyPos == std::string::npos) {
        return fallback;
    }

    const auto colon = text.find(':', keyPos);
    if (colon == std::string::npos) {
        return fallback;
    }

    std::size_t begin = colon + 1;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }

    std::size_t end = begin;
    while (end < text.size() && (std::isdigit(static_cast<unsigned char>(text[end])) || text[end] == '-' || text[end] == '+' || text[end] == '.')) {
        ++end;
    }

    if (end == begin) {
        return fallback;
    }

    return std::stod(text.substr(begin, end - begin));
}

Project parseUst(const std::filesystem::path& filePath) {
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
        } else if (inNote && key == "PBY") {
            currentNote.pitchBendCents = parseCsvInts(value);
        }
    }

    if (inNote) {
        currentNote.startTick = cursorTick;
        track.notes.push_back(currentNote);
    }

    return project;
}

Project parseUstx(const std::filesystem::path& filePath) {
    std::ifstream input(filePath);
    if (!input.is_open()) {
        throw std::runtime_error("Cannot open USTX file: " + filePath.string());
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    const std::string content = buffer.str();

    Project project;
    project.setTitle(filePath.stem().string());

    const auto bpm = extractDoubleValue(content, "bpm", 120.0);
    if (bpm != 120.0) {
        project.addTempo({.tick = 0, .bpm = bpm});
    }

    const auto tracks = extractObjectsFromArray(content, "tracks");
    for (const auto& trackObj : tracks) {
        std::string trackName = extractStringValue(trackObj, "track_name");
        if (trackName.empty()) {
            trackName = extractStringValue(trackObj, "name");
        }
        if (trackName.empty()) {
            trackName = "Track";
        }

        auto& track = project.createTrack(trackName);

        const auto notes = extractObjectsFromArray(trackObj, "notes");
        for (const auto& noteObj : notes) {
            Note note;
            note.startTick = extractIntValue(noteObj, "position", 0);
            note.durationTick = extractIntValue(noteObj, "duration", 480);
            note.pitch = extractIntValue(noteObj, "tone", 60);
            note.lyric = extractStringValue(noteObj, "lyric");
            if (note.lyric.empty()) {
                note.lyric = "a";
            }
            note.pitchBendCents = extractIntArrayValue(noteObj, "pitch");
            track.notes.push_back(note);
        }
    }

    if (project.tracks().empty()) {
        project.createTrack("Vocal");
    }

    return project;
}

} // namespace

Project UstParser::parse(const std::filesystem::path& filePath) const {
    const auto ext = toLower(filePath.extension().string());
    if (ext == ".ustx") {
        return parseUstx(filePath);
    }
    return parseUst(filePath);
}

} // namespace pyutau::core
