#pragma once

#include "utau/engine/types.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace utau::engine {

class Voicebank {
public:
    void load_oto(const std::string& otoPath);
    std::optional<OtoEntry> lookup(const std::string& alias) const;

private:
    std::vector<OtoEntry> entries_;
    std::unordered_map<std::string, std::size_t> indexByAlias_;
};

}  // namespace utau::engine
