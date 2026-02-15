#include "utau/engine/oto.hpp"

#include "utau/engine/encoding.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace utau::engine {

namespace {

double to_double(const std::string& value) {
    return value.empty() ? 0.0 : std::stod(value);
}

}  // namespace

std::vector<OtoEntry> OtoParser::parse_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open oto.ini: " + path);
    }

    std::vector<OtoEntry> entries;
    std::string line;

    while (std::getline(in, line)) {
        line = normalize_text(std::move(line));
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }

        OtoEntry entry;
        entry.wavFile = line.substr(0, eq);

        const std::string rhs = line.substr(eq + 1);
        std::stringstream ss(rhs);
        std::string token;
        std::vector<std::string> fields;
        while (std::getline(ss, token, ',')) {
            fields.push_back(token);
        }

        if (!fields.empty()) entry.alias = fields[0];
        if (fields.size() > 1) entry.offsetMs = to_double(fields[1]);
        if (fields.size() > 2) entry.consonantMs = to_double(fields[2]);
        if (fields.size() > 3) entry.cutoffMs = to_double(fields[3]);
        if (fields.size() > 4) entry.preutterMs = to_double(fields[4]);
        if (fields.size() > 5) entry.overlapMs = to_double(fields[5]);

        entries.push_back(std::move(entry));
    }

    return entries;
}

}  // namespace utau::engine
