#pragma once

#include "utau/engine/types.hpp"

#include <string>

namespace utau::engine {

void write_wav(const AudioBuffer& buffer, const std::string& path);

}  // namespace utau::engine
