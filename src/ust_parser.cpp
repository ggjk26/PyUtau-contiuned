#include "ust_parser.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace miniutau {
namespace {

int safe_to_int(const std::string& input, int default_value) {
    try {
        return std::stoi(input);
    } catch (...) {
        return default_value;
    }
}

Note note_from_map(const std::unordered_map<std::string, std::string>& kv) {
    Note note;
    if (auto it = kv.find("Lyric"); it != kv.end()) {
        note.lyric = it->second;
    }
    if (auto it = kv.find("Length"); it != kv.end()) {
        note.length_ticks = safe_to_int(it->second, note.length_ticks);
    }
    if (auto it = kv.find("NoteNum"); it != kv.end()) {
        note.note_num = safe_to_int(it->second, note.note_num);
    }
    if (auto it = kv.find("Velocity"); it != kv.end()) {
        note.velocity = safe_to_int(it->second, note.velocity);
    }
    return note;
}

bool is_note_tag(const std::string& tag) {
    return tag.rfind("[#", 0) == 0 && tag != "[#SETTING]" && tag != "[#VERSION]" && tag != "[#TRACKEND]";
}

} // namespace

UstProject UstParser::parse_file(const std::string& path) const {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("无法打开UST文件: " + path);
    }

    UstProject project;
    std::unordered_map<std::string, std::string> current_map;
    bool in_note = false;
    std::string line;

    auto flush_note = [&]() {
        if (in_note && !current_map.empty()) {
            project.notes.push_back(note_from_map(current_map));
            current_map.clear();
        }
    };

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (!line.empty() && line.front() == '[' && line.back() == ']') {
            if (line == "[#TRACKEND]") {
                flush_note();
                break;
            }
            if (line == "[#SETTING]") {
                flush_note();
                in_note = false;
                continue;
            }
            flush_note();
            in_note = is_note_tag(line);
            continue;
        }

        auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }

        auto key = line.substr(0, pos);
        auto value = line.substr(pos + 1);

        if (!in_note) {
            if (key == "Tempo") {
                project.tempo = safe_to_int(value, project.tempo);
            }
            continue;
        }

        current_map[key] = value;
    }

    flush_note();
    return project;
}

} // namespace miniutau
