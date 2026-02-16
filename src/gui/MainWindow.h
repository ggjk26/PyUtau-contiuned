#pragma once

#include "audio/AudioEngine.h"
#include "core/Project.h"
#include "core/Resampler.h"
#include "core/UstParser.h"
#include "core/Voicebank.h"
#include "gui/PianoRollWidget.h"

#include <QMainWindow>
#include <unordered_map>

QT_BEGIN_NAMESPACE
class QLabel;
class QListWidget;
QT_END_NAMESPACE

namespace pyutau::gui {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void openUst();
    void openVoicebank();
    void manageVoicebanks();
    void manageDictionary();
    void openSettings();
    void addTrack();
    void trackSelectionChanged();
    void exportWav();

private:
    struct AppSettings {
        unsigned int maxRenderThreads = 0;
        float masterGain = 0.95F;
        int sampleRate = 44100;
        bool enableVoicebankAiRetrain = false;
        bool enableAutoPitchLine = true;
        int autoVibratoDepthCents = 18;
        double autoVibratoRateHz = 5.5;
    };

    [[nodiscard]] pyutau::core::Track applyDictionary(const pyutau::core::Track& track) const;
    [[nodiscard]] pyutau::core::Track applyPitchEnhancements(const pyutau::core::Track& track) const;
    void buildUi();
    void bindProjectToUi();

    pyutau::core::Project m_project;
    pyutau::core::UstParser m_parser;
    pyutau::core::Resampler m_resampler;
    pyutau::audio::AudioEngine m_audio;

    std::unordered_map<std::string, pyutau::core::Voicebank> m_voicebankPool;
    std::unordered_map<std::string, std::string> m_lyricDictionary;
    AppSettings m_settings;

    QLabel* m_statusLabel = nullptr;
    QListWidget* m_trackList = nullptr;
    PianoRollWidget* m_pianoRoll = nullptr;
};

} // namespace pyutau::gui
