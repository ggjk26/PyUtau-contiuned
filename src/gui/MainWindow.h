#pragma once

#include "audio/AudioEngine.h"
#include "core/Project.h"
#include "core/Resampler.h"
#include "core/UstParser.h"
#include "core/Voicebank.h"
#include "gui/PianoRollWidget.h"

#include <QMainWindow>

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
    void exportWav();

private:
    void buildUi();
    void bindProjectToUi();

    pyutau::core::Project m_project;
    pyutau::core::Voicebank m_voicebank;
    pyutau::core::UstParser m_parser;
    pyutau::core::Resampler m_resampler;
    pyutau::audio::AudioEngine m_audio;

    QLabel* m_statusLabel = nullptr;
    QListWidget* m_trackList = nullptr;
    PianoRollWidget* m_pianoRoll = nullptr;
};

} // namespace pyutau::gui
