#include "utau/engine/ust.hpp"

#include "utau/engine/encoding.hpp"

#include <fstream>
#include <stdexcept>

namespace utau::engine {

namespace {

void apply_key_value(Note& note, SongProject& song, const std::string& key, const std::string& value) {
    if (key == "Length") {
        note.lengthTicks = std::stoi(value);
    } else if (key == "Lyric") {
        note.lyric = value;
    } else if (key == "NoteNum") {
        note.midiTone = std::stoi(value);
    } else if (key == "Velocity") {
        note.velocity = std::stoi(value);
    } else if (key == "Tempo") {
        song.tempo = std::stoi(value);
    }
}

}  // namespace

SongProject UstParser::parse_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open ust: " + path);
    }

    SongProject song;
    std::string line;
    bool inNoteBlock = false;
    Note currentNote;
    int noteIndex = 0;

    while (std::getline(in, line)) {
        line = normalize_text(std::move(line));
        if (line.empty()) {
            continue;
        }

        if (line.starts_with("[#")) {
            if (inNoteBlock) {
                currentNote.index = noteIndex++;
                song.notes.push_back(currentNote);
                currentNote = Note{};
            }

            inNoteBlock = (line != "[#VERSION]" && line != "[#SETTING]" && line != "[#TRACKEND]");
            continue;
        }

        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }

        const auto key = line.substr(0, eq);
        const auto value = line.substr(eq + 1);
        if (inNoteBlock) {
            apply_key_value(currentNote, song, key, value);
        } else if (key == "Tempo") {
            song.tempo = std::stoi(value);
        }
    }

    if (inNoteBlock) {
        currentNote.index = noteIndex;
        song.notes.push_back(currentNote);
    }

    return song;
}

}  // namespace utau::engine
