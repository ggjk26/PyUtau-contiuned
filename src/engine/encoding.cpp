#include "utau/engine/encoding.hpp"

#include <algorithm>

namespace utau::engine {

std::string normalize_text(std::string raw) {
    raw.erase(std::remove(raw.begin(), raw.end(), '\r'), raw.end());
    return raw;
}

}  // namespace utau::engine
