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

std::size_t resolveWorkerCount(std::size_t noteCount,
                               std::size_t totalFrames,
                               unsigned int maxThreads,
                               std::size_t memoryBudgetBytes = 64U * 1024U * 1024U) {
    if (noteCount == 0) {
        return 1;
    }

    const auto hardware = std::max(1U, std::thread::hardware_concurrency());
    const auto requested = maxThreads == 0 ? hardware : maxThreads;

    const std::size_t bytesPerWorker = std::max<std::size_t>(1, totalFrames) * sizeof(float);
    const std::size_t memoryLimited = std::max<std::size_t>(1, memoryBudgetBytes / bytesPerWorker);

    return std::clamp<std::size_t>(requested, 1, std::min(noteCount, memoryLimited));
}

int tickToFrame(int tick, double framesPerTick) {
    return static_cast<int>(std::lround(static_cast<double>(tick) * framesPerTick));
}

double interpolatePitchBendCents(const Note& note, double progress01) {
    if (note.pitchBendCents.empty()) {
        return 0.0;
    }
    if (note.pitchBendCents.size() == 1) {
        return static_cast<double>(note.pitchBendCents.front());
    }

    const double clamped = std::clamp(progress01, 0.0, 1.0);
    const double position = clamped * static_cast<double>(note.pitchBendCents.size() - 1);
    const auto index = static_cast<std::size_t>(position);
    const auto nextIndex = std::min(index + 1, note.pitchBendCents.size() - 1);
    const double frac = position - static_cast<double>(index);
    const double p0 = static_cast<double>(note.pitchBendCents[index]);
    const double p1 = static_cast<double>(note.pitchBendCents[nextIndex]);
    return p0 + (p1 - p0) * frac;
}

double computeAutoPitchCents(const Note& note, double secondsFromStart, double progress01) {
    if (!note.autoPitchEnabled) {
        return 0.0;
    }

    double cents = 0.0;

    // Soft attack scoop into target pitch.
    if (progress01 < 0.12) {
        const double t = progress01 / 0.12;
        cents += -28.0 * (1.0 - t);
    }

    // Controlled vibrato toward note tail.
    if (progress01 > 0.35) {
        const double env = std::min(1.0, (progress01 - 0.35) / 0.20);
        cents += static_cast<double>(note.autoVibratoDepthCents) * env * std::sin(2.0 * kPi * note.autoVibratoRateHz * secondsFromStart);
    }

    return cents;
}

double smoothStep(double x) {
    const double t = std::clamp(x, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

int resolveLegatoBridgeFrames(const Note& note, const Note* nextNote, double framesPerTick, int sampleRate) {
    if (!nextNote) {
        return 0;
    }

    const int gapTicks = nextNote->startTick - (note.startTick + note.durationTick);
    if (gapTicks > 120) {
        return 0;
    }

    const int noteFrames = std::max(1, tickToFrame(note.durationTick, framesPerTick));
    const int maxBridgeFrames = std::max(1, static_cast<int>(0.18 * static_cast<double>(noteFrames)));
    const int targetFrames = std::max(1, static_cast<int>(0.03 * static_cast<double>(sampleRate)));
    return std::min(maxBridgeFrames, targetFrames);
}

double computeHighPitchSafetyGain(double frequency, int sampleRate) {
    const double nyquistGuard = 0.45 * static_cast<double>(sampleRate);
    if (frequency <= nyquistGuard) {
        return 1.0;
    }

    const double ratio = nyquistGuard / std::max(frequency, 1.0);
    return std::clamp(ratio, 0.35, 1.0);
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

    const auto workerCount = resolveWorkerCount(track.notes.size(), static_cast<std::size_t>(totalFrames), maxThreads);
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

                const Note* nextNote = (i + 1 < track.notes.size()) ? &track.notes[i + 1] : nullptr;

                const int startFrame = tickToFrame(note.startTick, framesPerTick);
                const int noteFrames = std::max(0, tickToFrame(note.durationTick, framesPerTick));
                if (startFrame >= totalFrames || noteFrames <= 0) {
                    continue;
                }

                const int renderFrames = std::min(noteFrames, totalFrames - startFrame);
                const auto baseFrequency = 440.0 * std::pow(2.0, (static_cast<double>(note.pitch) - 69.0) / 12.0);
                const int fadeInFrames = std::max(1, std::min(renderFrames / 5, static_cast<int>(0.012 * static_cast<double>(sampleRate))));
                const int fadeOutFrames = std::max(1, std::min(renderFrames / 5, static_cast<int>(0.018 * static_cast<double>(sampleRate))));
                const int legatoBridgeFrames = resolveLegatoBridgeFrames(note, nextNote, framesPerTick, sampleRate);
                const double targetNextCents = nextNote ? (100.0 * static_cast<double>(nextNote->pitch - note.pitch)) : 0.0;

                double phase = 0.0;

                for (int f = 0; f < renderFrames; ++f) {
                    const double progress = static_cast<double>(f) / static_cast<double>(std::max(1, renderFrames - 1));
                    const double secondsFromStart = static_cast<double>(f) / static_cast<double>(sampleRate);
                    const double manualCents = interpolatePitchBendCents(note, progress);
                    const double autoCents = computeAutoPitchCents(note, secondsFromStart, progress);

                    double legatoCents = 0.0;
                    if (legatoBridgeFrames > 0 && f >= (renderFrames - legatoBridgeFrames)) {
                        const int bridgePos = f - (renderFrames - legatoBridgeFrames);
                        const double blend = smoothStep(static_cast<double>(bridgePos) / static_cast<double>(std::max(1, legatoBridgeFrames - 1)));
                        legatoCents = targetNextCents * blend;
                    }

                    const double cents = manualCents + autoCents + legatoCents;
                    const double modFrequency = baseFrequency * std::pow(2.0, cents / 1200.0);

                    double env = 1.0;
                    if (f < fadeInFrames) {
                        env *= smoothStep(static_cast<double>(f) / static_cast<double>(std::max(1, fadeInFrames)));
                    }
                    if (f >= (renderFrames - fadeOutFrames)) {
                        const int tailPos = f - (renderFrames - fadeOutFrames);
                        env *= 1.0 - smoothStep(static_cast<double>(tailPos) / static_cast<double>(std::max(1, fadeOutFrames)));
                    }

                    const double highPitchGain = computeHighPitchSafetyGain(modFrequency, sampleRate);
                    phase += 2.0 * kPi * modFrequency / static_cast<double>(sampleRate);
                    local[static_cast<std::size_t>(startFrame + f)] += static_cast<float>(kAmplitude * env * highPitchGain * std::sin(phase));
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
