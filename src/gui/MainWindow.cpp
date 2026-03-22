#include "MainWindow.h"

#include <QAction>
#include <QFileDialog>
#include <QHBoxLayout>
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

    auto& track = m_project.createTrack("Vocal #1");
    track.notes = {
        {.pitch = 60, .startTick = 0, .durationTick = 480, .lyric = "a"},
        {.pitch = 62, .startTick = 480, .durationTick = 480, .lyric = "i"},
        {.pitch = 64, .startTick = 960, .durationTick = 720, .lyric = "u"},
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
    const auto path = QFileDialog::getExistingDirectory(this, "Select Voicebank Folder");
    if (path.isEmpty()) {
        return;
    }

    if (!m_voicebank.loadFromDirectory(path.toStdString())) {
        QMessageBox::warning(this, "Voicebank", "oto.ini not found or empty.");
        return;
    }

    m_statusLabel->setText(QString("Voicebank loaded: %1 aliases").arg(m_voicebank.size()));
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

    const auto pcm = m_resampler.renderTrack(m_project.tracks().front(), m_voicebank);
    if (!m_audio.exportWav(path.toStdString(), pcm)) {
        QMessageBox::critical(this, "Export", "Failed to write wav file.");
        return;
    }

    m_statusLabel->setText(QString("Exported: %1").arg(path));
}

void MainWindow::buildUi() {
    auto* fileMenu = menuBar()->addMenu("File");
    fileMenu->addAction("Open UST", this, &MainWindow::openUst);
    fileMenu->addAction("Load Voicebank", this, &MainWindow::openVoicebank);
    fileMenu->addAction("Export WAV", this, &MainWindow::exportWav);

    auto* toolbar = addToolBar("Transport");
    toolbar->setMovable(false);
    toolbar->addAction("Open UST", this, &MainWindow::openUst);
    toolbar->addAction("Voicebank", this, &MainWindow::openVoicebank);
    toolbar->addAction("Render", this, &MainWindow::exportWav);

    auto* root = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(root);
    auto* splitter = new QSplitter(Qt::Horizontal, root);

    m_trackList = new QListWidget(splitter);
    m_trackList->setMinimumWidth(220);

    m_pianoRoll = new PianoRollWidget(splitter);

    auto* inspector = new QWidget(splitter);
    inspector->setMinimumWidth(260);
    auto* inspectorLayout = new QVBoxLayout(inspector);
    inspectorLayout->addWidget(new QLabel("类似 Synthesizer V 的 Inspector 占位区", inspector));
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
        m_trackList->addItem(QString::fromStdString(track.name));
    }

    if (!m_project.tracks().empty()) {
        m_pianoRoll->setTrack(&m_project.tracks().front());
    }
}

} // namespace pyutau::gui
