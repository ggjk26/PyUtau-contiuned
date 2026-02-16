#include "MainWindow.h"

#include <algorithm>

#include <QAction>
#include <QFileDialog>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

namespace pyutau::gui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("PyUtau Continued - C++ Prototype");
    resize(1400, 900);

    buildUi();

    auto& lead = m_project.createTrack("Vocal #1");
    lead.notes = {
        {.pitch = 60, .startTick = 0, .durationTick = 480, .lyric = "a"},
        {.pitch = 62, .startTick = 480, .durationTick = 480, .lyric = "i"},
        {.pitch = 64, .startTick = 960, .durationTick = 720, .lyric = "u"},
    };

    auto& harmony = m_project.createTrack("Harmony #1");
    harmony.gain = 0.7F;
    harmony.notes = {
        {.pitch = 55, .startTick = 0, .durationTick = 480, .lyric = "a"},
        {.pitch = 57, .startTick = 480, .durationTick = 480, .lyric = "i"},
        {.pitch = 59, .startTick = 960, .durationTick = 720, .lyric = "u"},
    };

    bindProjectToUi();
}

void MainWindow::openUst() {
    const auto path = QFileDialog::getOpenFileName(this, "Open UST", {}, "UST files (*.ust)");
    if (path.isEmpty()) {
        return;
    }

    try {
        m_project = m_parser.parse(path.toStdString());
        bindProjectToUi();
        m_statusLabel->setText(QString("Loaded UST: %1").arg(path));
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Load failed", ex.what());
    }
}

void MainWindow::openVoicebank() {
    if (!m_trackList || m_trackList->currentRow() < 0 || m_trackList->currentRow() >= static_cast<int>(m_project.tracks().size())) {
        QMessageBox::information(this, "Voicebank", "请先选中一个音轨后再加载声库。");
        return;
    }

    const auto path = QFileDialog::getExistingDirectory(this, "Select Voicebank Folder");
    if (path.isEmpty()) {
        return;
    }

    pyutau::core::Voicebank voicebank;
    if (!voicebank.loadFromDirectory(path.toStdString())) {
        QMessageBox::warning(this, "Voicebank", "oto.ini not found or empty.");
        return;
    }

    const auto key = path.toStdString();
    m_voicebankPool[key] = std::move(voicebank);

    auto& selectedTrack = m_project.tracks()[static_cast<std::size_t>(m_trackList->currentRow())];
    selectedTrack.voicebankId = key;

    bindProjectToUi();

    const auto& assigned = m_voicebankPool[selectedTrack.voicebankId];
    m_statusLabel->setText(QString("Voicebank assigned to %1: %2 aliases")
                               .arg(QString::fromStdString(selectedTrack.name))
                               .arg(assigned.size()));
}

void MainWindow::addTrack() {
    auto& track = m_project.createTrack("Vocal #" + std::to_string(m_project.tracks().size() + 1));
    track.notes = {
        {.pitch = 60, .startTick = 0, .durationTick = 480, .lyric = "a"},
        {.pitch = 62, .startTick = 480, .durationTick = 480, .lyric = "i"},
    };

    bindProjectToUi();
    m_trackList->setCurrentRow(static_cast<int>(m_project.tracks().size() - 1));
    m_statusLabel->setText(QString("Added track: %1").arg(QString::fromStdString(track.name)));
}

void MainWindow::trackSelectionChanged() {
    if (!m_trackList) {
        return;
    }

    const auto idx = m_trackList->currentRow();
    if (idx < 0 || idx >= static_cast<int>(m_project.tracks().size())) {
        return;
    }

    m_pianoRoll->setTrack(&m_project.tracks()[static_cast<std::size_t>(idx)]);
}

void MainWindow::exportWav() {
    if (m_project.tracks().empty()) {
        QMessageBox::information(this, "Export", "No tracks available.");
        return;
    }

    const auto path = QFileDialog::getSaveFileName(this, "Export WAV", "output.wav", "WAV (*.wav)");
    if (path.isEmpty()) {
        return;
    }

    std::vector<float> mixdown;
    std::size_t usedTracks = 0;
    std::size_t totalWorkers = 0;
    double totalRenderMs = 0.0;

    for (const auto& track : m_project.tracks()) {
        if (track.muted) {
            continue;
        }

        pyutau::core::Voicebank emptyVoicebank;
        const pyutau::core::Voicebank* vb = &emptyVoicebank;
        if (!track.voicebankId.empty()) {
            const auto found = m_voicebankPool.find(track.voicebankId);
            if (found != m_voicebankPool.end()) {
                vb = &found->second;
            }
        }

        auto rendered = m_resampler.renderTrack(track, *vb);
        if (rendered.pcm.empty()) {
            continue;
        }

        if (mixdown.size() < rendered.pcm.size()) {
            mixdown.resize(rendered.pcm.size(), 0.0F);
        }

        for (std::size_t i = 0; i < rendered.pcm.size(); ++i) {
            mixdown[i] += rendered.pcm[i] * track.gain;
        }

        ++usedTracks;
        totalWorkers += rendered.workerThreads;
        totalRenderMs += rendered.elapsedMs;
    }

    if (usedTracks == 0) {
        QMessageBox::warning(this, "Export", "没有可导出的有效音轨（音轨可能为空或全部静音）。");
        return;
    }

    for (auto& sample : mixdown) {
        sample = std::clamp(sample, -1.0F, 1.0F);
    }

    if (!m_audio.exportWav(path.toStdString(), mixdown)) {
        QMessageBox::critical(this, "Export", "Failed to write wav file.");
        return;
    }

    m_statusLabel->setText(QString("Exported: %1 | tracks %2 | render %3 ms | workers %4")
                               .arg(path)
                               .arg(usedTracks)
                               .arg(QString::number(totalRenderMs, 'f', 2))
                               .arg(totalWorkers));
}

void MainWindow::buildUi() {
    auto* fileMenu = menuBar()->addMenu("File");
    fileMenu->addAction("Open UST", this, &MainWindow::openUst);
    fileMenu->addAction("Load Voicebank to Selected Track", this, &MainWindow::openVoicebank);
    fileMenu->addAction("Add Track", this, &MainWindow::addTrack);
    fileMenu->addAction("Export WAV", this, &MainWindow::exportWav);

    auto* toolbar = addToolBar("Transport");
    toolbar->setMovable(false);
    toolbar->addAction("Open UST", this, &MainWindow::openUst);
    toolbar->addAction("Assign Voicebank", this, &MainWindow::openVoicebank);
    toolbar->addAction("Add Track", this, &MainWindow::addTrack);
    toolbar->addAction("Render Mix", this, &MainWindow::exportWav);

    auto* root = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(root);
    auto* splitter = new QSplitter(Qt::Horizontal, root);

    m_trackList = new QListWidget(splitter);
    m_trackList->setMinimumWidth(280);
    connect(m_trackList, &QListWidget::currentRowChanged, this, &MainWindow::trackSelectionChanged);

    m_pianoRoll = new PianoRollWidget(splitter);

    auto* inspector = new QWidget(splitter);
    inspector->setMinimumWidth(280);
    auto* inspectorLayout = new QVBoxLayout(inspector);
    inspectorLayout->addWidget(new QLabel("类似 Synthesizer V 的 Inspector 占位区\n（可扩展：轨道静音/独奏/声库选择/增益）", inspector));
    inspectorLayout->addStretch(1);

    splitter->addWidget(m_trackList);
    splitter->addWidget(m_pianoRoll);
    splitter->addWidget(inspector);
    splitter->setStretchFactor(1, 1);

    rootLayout->addWidget(splitter);
    setCentralWidget(root);

    m_statusLabel = new QLabel("Ready", this);
    statusBar()->addWidget(m_statusLabel, 1);
}

void MainWindow::bindProjectToUi() {
    m_trackList->clear();
    for (const auto& track : m_project.tracks()) {
        const QString voicebankText = track.voicebankId.empty()
                                          ? "(No Voicebank)"
                                          : QString::fromStdString(track.voicebankId);
        const QString muteText = track.muted ? "[M] " : "";
        m_trackList->addItem(QString("%1%2 | gain %3 | %4")
                                 .arg(muteText)
                                 .arg(QString::fromStdString(track.name))
                                 .arg(QString::number(track.gain, 'f', 2))
                                 .arg(voicebankText));
    }

    if (!m_project.tracks().empty()) {
        m_trackList->setCurrentRow(0);
        m_pianoRoll->setTrack(&m_project.tracks().front());
    }
}

} // namespace pyutau::gui
