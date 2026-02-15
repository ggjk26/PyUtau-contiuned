#pragma once

#include <string>

namespace utau::engine {

// Lightweight fallback decoder for plain ASCII/UTF-8 text.
// UTAU files are commonly Shift-JIS; robust conversion can be plugged in later.
std::string normalize_text(std::string raw);

}  // namespace utau::engine
