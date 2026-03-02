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
                               unsigned int maxThreads) {
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
    if (progress01 < 0.10) {
        const double t = progress01 / 0.10;
        cents += -24.0 * (1.0 - t);
    }
    if (progress01 > 0.32) {
        const double env = std::min(1.0, (progress01 - 0.32) / 0.22);
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
    const int maxBridgeFrames = std::max(1, static_cast<int>(0.20 * static_cast<double>(noteFrames)));
    const int targetFrames = std::max(1, static_cast<int>(0.035 * static_cast<double>(sampleRate)));
    return std::min(maxBridgeFrames, targetFrames);
}

double computeHighPitchSafetyGain(double frequency, int sampleRate) {
    const double nyquistGuard = 0.45 * static_cast<double>(sampleRate);
    if (frequency <= nyquistGuard) {
        return 1.0;
    }

    const double ratio = nyquistGuard / std::max(frequency, 1.0);
    return std::clamp(ratio, 0.30, 1.0);
}

double harmonicMixGain(double frequency) {
    // OpenUTAU-like practical approach: reduce harmonics as pitch rises to avoid harshness.
    if (frequency < 500.0) {
        return 0.30;
    }
    if (frequency < 1500.0) {
        return 0.20;
    }
    return 0.08;
}

struct WorkerBuffer {
    int startFrame = 0;
    std::vector<float> pcm;
};

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
        totalFrames = std::max(totalFrames, tickToFrame(note.startTick + note.durationTick, framesPerTick));
    }
    if (totalFrames <= 0) {
        return result;
    }

    const auto workerCount = resolveWorkerCount(track.notes.size(), maxThreads);
    result.workerThreads = workerCount;

    std::vector<WorkerBuffer> workerBuffers(workerCount);
    std::vector<std::future<void>> tasks;
    tasks.reserve(workerCount);

    for (std::size_t worker = 0; worker < workerCount; ++worker) {
        tasks.push_back(std::async(std::launch::async, [&, worker]() {
            int localStart = totalFrames;
            int localEnd = 0;

            for (std::size_t i = worker; i < track.notes.size(); i += workerCount) {
                const auto& note = track.notes[i];
                localStart = std::min(localStart, tickToFrame(note.startTick, framesPerTick));
                localEnd = std::max(localEnd, tickToFrame(note.startTick + note.durationTick, framesPerTick));
            }

            if (localStart >= localEnd) {
                workerBuffers[worker].startFrame = 0;
                return;
            }

            workerBuffers[worker].startFrame = localStart;
            auto& local = workerBuffers[worker].pcm;
            local.assign(static_cast<std::size_t>(localEnd - localStart), 0.0F);

            for (std::size_t i = worker; i < track.notes.size(); i += workerCount) {
                const auto& note = track.notes[i];
                (void)voicebank.lookup(note.lyric);
                const Note* nextNote = (i + 1 < track.notes.size()) ? &track.notes[i + 1] : nullptr;

                const int startFrame = tickToFrame(note.startTick, framesPerTick);
                const int noteFrames = std::max(0, tickToFrame(note.durationTick, framesPerTick));
                if (noteFrames <= 0) {
                    continue;
                }

                const int maxWritableFrames = std::max(0, localEnd - startFrame);
                const int renderFrames = std::min(noteFrames, maxWritableFrames);
                if (renderFrames <= 0) {
                    continue;
                }

                const auto baseFrequency = 440.0 * std::pow(2.0, (static_cast<double>(note.pitch) - 69.0) / 12.0);
                const int fadeInFrames = std::max(1, std::min(renderFrames / 4, static_cast<int>(0.010 * static_cast<double>(sampleRate))));
                const int fadeOutFrames = std::max(1, std::min(renderFrames / 4, static_cast<int>(0.020 * static_cast<double>(sampleRate))));
                const int legatoBridgeFrames = resolveLegatoBridgeFrames(note, nextNote, framesPerTick, sampleRate);
                const double targetNextCents = nextNote ? (100.0 * static_cast<double>(nextNote->pitch - note.pitch)) : 0.0;

                double phase = 0.0;
                double phaseH2 = 0.0;
                const double h2Gain = harmonicMixGain(baseFrequency);

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
                    phaseH2 += 2.0 * kPi * (modFrequency * 2.0) / static_cast<double>(sampleRate);

                    const float sample = static_cast<float>(kAmplitude * env * highPitchGain * (std::sin(phase) + h2Gain * std::sin(phaseH2)));
                    local[static_cast<std::size_t>((startFrame - localStart) + f)] += sample;
                }
            }
        }));
    }

    for (auto& task : tasks) {
        task.get();
    }

    pcm.assign(static_cast<std::size_t>(totalFrames), 0.0F);
    for (const auto& local : workerBuffers) {
        for (std::size_t i = 0; i < local.pcm.size(); ++i) {
            const std::size_t dst = static_cast<std::size_t>(local.startFrame) + i;
            if (dst < pcm.size()) {
                pcm[dst] += local.pcm[i];
            }
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
