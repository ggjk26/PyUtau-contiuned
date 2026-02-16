#pragma once

#include "utau_types.hpp"

#include <string>

namespace miniutau {

class UstParser {
public:
    UstProject parse_file(const std::string& path) const;
};

} // namespace miniutau
