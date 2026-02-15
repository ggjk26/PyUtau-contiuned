#include "utau/engine/voicebank.hpp"

#include "utau/engine/oto.hpp"

namespace utau::engine {

void Voicebank::load_oto(const std::string& otoPath) {
    entries_ = OtoParser::parse_file(otoPath);
    indexByAlias_.clear();

    for (std::size_t i = 0; i < entries_.size(); ++i) {
        const auto& alias = entries_[i].alias.empty() ? entries_[i].wavFile : entries_[i].alias;
        indexByAlias_[alias] = i;
    }
}

std::optional<OtoEntry> Voicebank::lookup(const std::string& alias) const {
    const auto it = indexByAlias_.find(alias);
    if (it == indexByAlias_.end()) {
        return std::nullopt;
    }
    return entries_[it->second];
}

}  // namespace utau::engine
