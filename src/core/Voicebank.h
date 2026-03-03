#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

namespace pyutau::core {

struct OtoEntry {
    std::string wav;
    std::string alias;
    double offsetMs = 0.0;
    double consonantMs = 0.0;
    double cutoffMs = 0.0;
    double preutterMs = 0.0;
    double overlapMs = 0.0;
};

struct VoicebankAiTrainingReport {
    bool enabled = false;
    std::size_t sampleCount = 0;
    std::size_t generatedAliases = 0;
    double elapsedMs = 0.0;
};

struct OpenUtauSyncReport {
    bool ok = false;
    bool synced = false;
    std::string singerName;
    std::string author;
    std::size_t otoFileCount = 0;
    std::size_t aliasCount = 0;
};

class Voicebank {
public:
    bool loadFromDirectory(const std::filesystem::path& path);
    [[nodiscard]] OpenUtauSyncReport loadFromDirectoryWithOpenUtauSync(const std::filesystem::path& path);

    [[nodiscard]] const OtoEntry* lookup(const std::string& lyric) const;
    [[nodiscard]] std::size_t size() const;

    [[nodiscard]] VoicebankAiTrainingReport runAiPostProcessAndRetrain(bool enabled);

private:
    std::unordered_map<std::string, OtoEntry> m_dictionary;
};

} // namespace pyutau::core
