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
    void showAboutAndUpdates();

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

    struct VersionToken {
        int major = 0;
        int minor = 0;
        int patch = 0;
        int stage = 3; // preview(0) < alpha(1) < beta(2) < release(3)
    };

    [[nodiscard]] pyutau::core::Track applyDictionary(const pyutau::core::Track& track) const;
    [[nodiscard]] pyutau::core::Track applyPitchEnhancements(const pyutau::core::Track& track) const;
    [[nodiscard]] static bool tryParseVersionTag(const std::string& tag, VersionToken& out);
    [[nodiscard]] static int compareVersion(const VersionToken& lhs, const VersionToken& rhs);
    [[nodiscard]] static std::string detectGitHubRepoFromGitRemote();
    [[nodiscard]] static std::string fetchLatestReleaseTag(const std::string& repo);
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
