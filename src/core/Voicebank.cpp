#include "Voicebank.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <sstream>
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

std::string toLowerAscii(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

bool parseOtoFile(const std::filesystem::path& otoPath,
                  std::unordered_map<std::string, OtoEntry>& dictionary) {
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
        entry.wav = trim(line.substr(0, equalPos));

        const auto payload = line.substr(equalPos + 1);
        std::stringstream ss(payload);
        std::string segment;

        std::getline(ss, entry.alias, ',');
        entry.alias = trim(entry.alias);
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

        if (!entry.alias.empty()) {
            dictionary[entry.alias] = entry;
        }
    }

    return true;
}

std::string readYamlValue(const std::filesystem::path& yamlPath, const std::string& key) {
    std::ifstream input(yamlPath);
    if (!input.is_open()) {
        return {};
    }

    std::string line;
    const std::string marker = key + ":";
    while (std::getline(input, line)) {
        const auto trimmed = trim(line);
        if (!trimmed.starts_with(marker)) {
            continue;
        }
        auto value = trim(trimmed.substr(marker.size()));
        if (!value.empty() && (value.front() == '"' || value.front() == '\'')) {
            value.erase(value.begin());
        }
        if (!value.empty() && (value.back() == '"' || value.back() == '\'')) {
            value.pop_back();
        }
        return value;
    }

    return {};
}

} // namespace

bool Voicebank::loadFromDirectory(const std::filesystem::path& path) {
    m_dictionary.clear();
    return parseOtoFile(path / "oto.ini", m_dictionary) && !m_dictionary.empty();
}

OpenUtauSyncReport Voicebank::loadFromDirectoryWithOpenUtauSync(const std::filesystem::path& path) {
    m_dictionary.clear();

    OpenUtauSyncReport report;
    report.synced = true;
    report.singerName = readYamlValue(path / "character.yaml", "name");
    report.author = readYamlValue(path / "character.yaml", "author");

    std::size_t parsedOtoFiles = 0;
    const auto rootOto = path / "oto.ini";
    if (std::filesystem::exists(rootOto)) {
        if (parseOtoFile(rootOto, m_dictionary)) {
            ++parsedOtoFiles;
        }
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (toLowerAscii(entry.path().filename().string()) != "oto.ini") {
            continue;
        }
        if (entry.path() == rootOto) {
            continue;
        }
        if (parseOtoFile(entry.path(), m_dictionary)) {
            ++parsedOtoFiles;
        }
    }

    report.otoFileCount = parsedOtoFiles;
    report.aliasCount = m_dictionary.size();
    report.ok = !m_dictionary.empty();
    return report;
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

VoicebankAiTrainingReport Voicebank::runAiPostProcessAndRetrain(bool enabled) {
    VoicebankAiTrainingReport report;
    report.enabled = enabled;

    if (!enabled || m_dictionary.empty()) {
        return report;
    }

    const auto started = std::chrono::steady_clock::now();
    report.sampleCount = m_dictionary.size();

    std::vector<OtoEntry> generated;
    generated.reserve(m_dictionary.size());

    for (const auto& [alias, entry] : m_dictionary) {
        const auto lowered = toLowerAscii(trim(alias));
        if (lowered.empty() || lowered == alias) {
            continue;
        }

        if (m_dictionary.contains(lowered)) {
            continue;
        }

        OtoEntry clone = entry;
        clone.alias = lowered;
        clone.preutterMs = std::max(0.0, clone.preutterMs * 0.97);
        clone.overlapMs = std::max(0.0, clone.overlapMs * 1.03);
        generated.push_back(std::move(clone));
    }

    for (const auto& entry : generated) {
        m_dictionary[entry.alias] = entry;
    }

    report.generatedAliases = generated.size();
    const auto finished = std::chrono::steady_clock::now();
    report.elapsedMs = std::chrono::duration<double, std::milli>(finished - started).count();
    return report;
}

} // namespace pyutau::core
