#include "core.hpp"

#include <algorithm>
#include <iostream>
#include <optional>
#include <string>

namespace {

struct CliArgs {
    std::filesystem::path ust;
    std::filesystem::path out;
    int sample_rate = 44100;
    utau::RenderOptions render_options;
    bool help = false;
};

std::optional<int> parse_int(const std::string& value) {
    try {
        size_t idx = 0;
        const int out = std::stoi(value, &idx);
        if (idx != value.size()) {
            return std::nullopt;
        }
        return out;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<float> parse_float(const std::string& value) {
    try {
        size_t idx = 0;
        const float out = std::stof(value, &idx);
        if (idx != value.size()) {
            return std::nullopt;
        }
        return out;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<CliArgs> parse_args(int argc, char** argv, std::string& error) {
    CliArgs args;
    for (int i = 1; i < argc; ++i) {
        const std::string k = argv[i];
        if (k == "--help" || k == "-h") {
            args.help = true;
            return args;
        }

        if ((k == "--ust" || k == "--out" || k == "--sr" || k == "--gain") && i + 1 >= argc) {
            error = "Missing value for argument: " + k;
            return std::nullopt;
        }

        if (k == "--ust") {
            args.ust = argv[++i];
        } else if (k == "--out") {
            args.out = argv[++i];
        } else if (k == "--sr") {
            const auto sr = parse_int(argv[++i]);
            if (!sr.has_value()) {
                error = "Invalid sample rate.";
                return std::nullopt;
            }
            args.sample_rate = std::clamp(*sr, 8000, 192000);
        } else if (k == "--gain") {
            const auto gain = parse_float(argv[++i]);
            if (!gain.has_value() || *gain <= 0.0f || *gain > 4.0f) {
                error = "Invalid gain, expected (0.0, 4.0].";
                return std::nullopt;
            }
            args.render_options.master_gain = *gain;
        } else if (k == "--no-normalize") {
            args.render_options.normalize = false;
        } else {
            error = "Unknown argument: " + k;
            return std::nullopt;
        }
    }

    if (args.ust.empty() || args.out.empty()) {
        error = "Both --ust and --out are required.";
        return std::nullopt;
    }
    return args;
}

void print_usage() {
    std::cout << "Usage: utau_core_render --ust input.ust --out output.wav [--sr 44100] [--gain 0.2] [--no-normalize]\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::string error;
    const auto args = parse_args(argc, argv, error);
    if (!args.has_value()) {
        if (!error.empty()) {
            std::cerr << "[args] " << error << "\n";
        }
        print_usage();
        return 1;
    }

    if (args->help) {
        print_usage();
        return 0;
    }

    if (!utau::render_ust_to_wav(args->ust, args->out, args->sample_rate, error, args->render_options)) {
        std::cerr << "[render] " << error << "\n";
        return 2;
    }

    std::cout << "Rendered to " << args->out << " at " << args->sample_rate << " Hz"
              << " (gain=" << args->render_options.master_gain
              << ", normalize=" << (args->render_options.normalize ? "on" : "off") << ")\n";
    return 0;
}
