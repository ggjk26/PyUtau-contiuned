#pragma once

#include "utau_types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace miniutau {

class OtoParser {
public:
    std::vector<OtoEntry> parse_file(const std::string& path) const;
    std::optional<OtoEntry> find_alias(const std::vector<OtoEntry>& entries, const std::string& alias) const;
};

} // namespace miniutau
