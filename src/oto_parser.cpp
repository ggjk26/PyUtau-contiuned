#include "oto_parser.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace miniutau {
namespace {

double safe_to_double(const std::string& input, double default_value) {
    try {
        return std::stod(input);
    } catch (...) {
        return default_value;
    }
}

} // namespace

std::vector<OtoEntry> OtoParser::parse_file(const std::string& path) const {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("无法打开oto.ini文件: " + path);
    }

    std::vector<OtoEntry> entries;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            continue;
        }

        OtoEntry entry;
        entry.wav_file = line.substr(0, eq_pos);

        std::string rhs = line.substr(eq_pos + 1);
        std::stringstream ss(rhs);
        std::string token;
        std::vector<std::string> fields;
        while (std::getline(ss, token, ',')) {
            fields.push_back(token);
        }

        if (fields.size() >= 6) {
            entry.alias = fields[0];
            entry.offset_ms = safe_to_double(fields[1], 0.0);
            entry.consonant_ms = safe_to_double(fields[2], 0.0);
            entry.cutoff_ms = safe_to_double(fields[3], 0.0);
            entry.preutter_ms = safe_to_double(fields[4], 0.0);
            entry.overlap_ms = safe_to_double(fields[5], 0.0);
        } else {
            entry.alias = fields.empty() ? "" : fields[0];
        }

        entries.push_back(entry);
    }

    return entries;
}

std::optional<OtoEntry> OtoParser::find_alias(const std::vector<OtoEntry>& entries, const std::string& alias) const {
    for (const auto& entry : entries) {
        if (entry.alias == alias) {
            return entry;
        }
    }
    return std::nullopt;
}

} // namespace miniutau
