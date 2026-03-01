#pragma once

#include "Project.h"

#include <filesystem>

namespace pyutau::core {

class UstParser {
public:
    [[nodiscard]] Project parse(const std::filesystem::path& filePath) const;
};

} // namespace pyutau::core
