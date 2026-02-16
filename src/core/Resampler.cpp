#include "Resampler.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <future>
#include <thread>

namespace pyutau::core {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTicksPerQuarter = 480.0;
constexpr double kDefaultBpm = 120.0;
constexpr float kAmplitude = 0.15F;

std::size_t resolveWorkerCount(std::size_t noteCount, unsigned int maxThreads) {
    if (noteCount == 0) {
        return 1;
    }

    const auto hardware = std::max(1U, std::thread::hardware_concurrency());
    const auto requested = maxThreads == 0 ? hardware : maxThreads;
    return std::clamp<std::size_t>(requested, 1, noteCount);
}

int tickToFrame(int tick, double framesPerTick) {
    return static_cast<int>(std::lround(static_cast<double>(tick) * framesPerTick));
}

} // namespace

RenderResult Resampler::renderTrack(const Track& track,
                                    const Voicebank& voicebank,
                                    int sampleRate,
                                    unsigned int maxThreads) const {
    RenderResult result;
    auto& pcm = result.pcm;

    if (track.notes.empty() || sampleRate <= 0) {
        return result;
    }

    const auto started = std::chrono::steady_clock::now();
    const auto framesPerTick = (60.0 * static_cast<double>(sampleRate)) / (kDefaultBpm * kTicksPerQuarter);

    int totalFrames = 0;
    for (const auto& note : track.notes) {
        const int noteEndFrame = tickToFrame(note.startTick + note.durationTick, framesPerTick);
        totalFrames = std::max(totalFrames, noteEndFrame);
    }

    if (totalFrames <= 0) {
        return result;
    }

    const auto workerCount = resolveWorkerCount(track.notes.size(), maxThreads);
    result.workerThreads = workerCount;

    std::vector<std::vector<float>> workerBuffers(workerCount, std::vector<float>(static_cast<std::size_t>(totalFrames), 0.0F));
    std::vector<std::future<void>> tasks;
    tasks.reserve(workerCount);

    for (std::size_t worker = 0; worker < workerCount; ++worker) {
        tasks.push_back(std::async(std::launch::async, [&, worker]() {
            auto& local = workerBuffers[worker];

            for (std::size_t i = worker; i < track.notes.size(); i += workerCount) {
                const auto& note = track.notes[i];
                (void)voicebank.lookup(note.lyric);

                const int startFrame = tickToFrame(note.startTick, framesPerTick);
                const int noteFrames = std::max(0, tickToFrame(note.durationTick, framesPerTick));
                if (startFrame >= totalFrames || noteFrames <= 0) {
                    continue;
                }

                const int renderFrames = std::min(noteFrames, totalFrames - startFrame);
                const auto frequency = 440.0 * std::pow(2.0, (static_cast<double>(note.pitch) - 69.0) / 12.0);
                const auto phaseStep = 2.0 * kPi * frequency / static_cast<double>(sampleRate);

                for (int f = 0; f < renderFrames; ++f) {
                    const auto phase = phaseStep * static_cast<double>(f);
                    local[static_cast<std::size_t>(startFrame + f)] += static_cast<float>(kAmplitude * std::sin(phase));
                }
            }
        }));
    }

    for (auto& task : tasks) {
        task.get();
    }

    pcm.assign(static_cast<std::size_t>(totalFrames), 0.0F);
    for (const auto& local : workerBuffers) {
        for (std::size_t i = 0; i < local.size(); ++i) {
            pcm[i] += local[i];
        }
    }

    for (auto& sample : pcm) {
        sample = std::clamp(sample, -1.0F, 1.0F);
    }

    const auto finished = std::chrono::steady_clock::now();
    result.elapsedMs = std::chrono::duration<double, std::milli>(finished - started).count();
    return result;
}

} // namespace pyutau::core
