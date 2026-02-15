#pragma once

#include "utau/engine/types.hpp"

#include <string>

namespace utau::engine {

class UstParser {
public:
    static SongProject parse_file(const std::string& path);
};

}  // namespace utau::engine
