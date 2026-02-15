#pragma once

#include "utau/engine/types.hpp"

#include <string>
#include <vector>

namespace utau::engine {

class OtoParser {
public:
    static std::vector<OtoEntry> parse_file(const std::string& path);
};

}  // namespace utau::engine
