#pragma once

#include "utau/engine/types.hpp"
#include "utau/engine/voicebank.hpp"

namespace utau::engine {

class SynthEngine {
public:
    explicit SynthEngine(RenderConfig config = {});
    AudioBuffer render(const SongProject& song, const Voicebank& voicebank) const;

private:
    RenderConfig config_;
};

}  // namespace utau::engine
