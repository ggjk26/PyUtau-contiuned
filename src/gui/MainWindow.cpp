#include "MainWindow.h"

#include <algorithm>

#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QSpinBox>
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
    manageVoicebanks();
}

void MainWindow::manageVoicebanks() {
    const QStringList operations{
        "添加声库",
        "移除声库",
        "分配声库到当前音轨",
        "查看已加载声库",
    };

    bool ok = false;
    const auto action = QInputDialog::getItem(this, "声库管理", "请选择操作", operations, 0, false, &ok);
    if (!ok || action.isEmpty()) {
        return;
    }

    if (action == "添加声库") {
        const auto path = QFileDialog::getExistingDirectory(this, "Select Voicebank Folder");
        if (path.isEmpty()) {
            return;
        }

        bool nameOk = false;
        const auto defaultName = QString("VB_%1").arg(m_voicebankPool.size() + 1);
        auto name = QInputDialog::getText(this, "声库名称", "请输入声库 ID", QLineEdit::Normal, defaultName, &nameOk).trimmed();
        if (!nameOk || name.isEmpty()) {
            return;
        }

        pyutau::core::Voicebank vb;
        if (!vb.loadFromDirectory(path.toStdString())) {
            QMessageBox::warning(this, "Voicebank", "oto.ini not found or empty.");
            return;
        }

        const auto key = name.toStdString();
        m_voicebankPool[key] = std::move(vb);
        m_statusLabel->setText(QString("Voicebank added: %1 (%2 aliases)").arg(name).arg(m_voicebankPool[key].size()));
        bindProjectToUi();
        return;
    }

    if (action == "移除声库") {
        if (m_voicebankPool.empty()) {
            QMessageBox::information(this, "声库管理", "当前没有可移除的声库。");
            return;
        }

        QStringList ids;
        for (const auto& [id, _] : m_voicebankPool) {
            ids.push_back(QString::fromStdString(id));
        }
        ids.sort();

        bool idOk = false;
        const auto selected = QInputDialog::getItem(this, "移除声库", "选择要移除的声库", ids, 0, false, &idOk);
        if (!idOk || selected.isEmpty()) {
            return;
        }

        m_voicebankPool.erase(selected.toStdString());
        for (auto& track : m_project.tracks()) {
            if (track.voicebankId == selected.toStdString()) {
                track.voicebankId.clear();
            }
        }

        bindProjectToUi();
        m_statusLabel->setText(QString("Voicebank removed: %1").arg(selected));
        return;
    }

    if (action == "分配声库到当前音轨") {
        if (!m_trackList || m_trackList->currentRow() < 0 || m_trackList->currentRow() >= static_cast<int>(m_project.tracks().size())) {
            QMessageBox::information(this, "声库管理", "请先选中一个音轨。");
            return;
        }
        if (m_voicebankPool.empty()) {
            QMessageBox::information(this, "声库管理", "请先添加声库。");
            return;
        }

        QStringList ids;
        for (const auto& [id, _] : m_voicebankPool) {
            ids.push_back(QString::fromStdString(id));
        }
        ids.sort();

        bool idOk = false;
        const auto selected = QInputDialog::getItem(this, "分配声库", "选择声库", ids, 0, false, &idOk);
        if (!idOk || selected.isEmpty()) {
            return;
        }

        auto& track = m_project.tracks()[static_cast<std::size_t>(m_trackList->currentRow())];
        track.voicebankId = selected.toStdString();
        bindProjectToUi();
        m_statusLabel->setText(QString("Assigned %1 -> %2")
                                   .arg(selected)
                                   .arg(QString::fromStdString(track.name)));
        return;
    }

    if (action == "查看已加载声库") {
        if (m_voicebankPool.empty()) {
            QMessageBox::information(this, "声库管理", "当前没有加载声库。");
            return;
        }

        QStringList rows;
        for (const auto& [id, vb] : m_voicebankPool) {
            rows.push_back(QString("%1 : %2 aliases").arg(QString::fromStdString(id)).arg(vb.size()));
        }
        rows.sort();
        QMessageBox::information(this, "已加载声库", rows.join("\n"));
    }
}

void MainWindow::manageDictionary() {
    const QStringList operations{
        "添加/更新词典项",
        "删除词典项",
        "查看词典",
        "清空词典",
    };

    bool ok = false;
    const auto action = QInputDialog::getItem(this, "词典管理", "请选择操作", operations, 0, false, &ok);
    if (!ok || action.isEmpty()) {
        return;
    }

    if (action == "添加/更新词典项") {
        bool fromOk = false;
        const auto from = QInputDialog::getText(this, "词典管理", "输入原歌词", QLineEdit::Normal, {}, &fromOk).trimmed();
        if (!fromOk || from.isEmpty()) {
            return;
        }

        bool toOk = false;
        const auto to = QInputDialog::getText(this, "词典管理", "输入替换歌词", QLineEdit::Normal, from, &toOk).trimmed();
        if (!toOk || to.isEmpty()) {
            return;
        }

        m_lyricDictionary[from.toStdString()] = to.toStdString();
        m_statusLabel->setText(QString("Dictionary updated: %1 -> %2").arg(from).arg(to));
        return;
    }

    if (action == "删除词典项") {
        if (m_lyricDictionary.empty()) {
            QMessageBox::information(this, "词典管理", "词典为空。");
            return;
        }

        QStringList keys;
        for (const auto& [k, _] : m_lyricDictionary) {
            keys.push_back(QString::fromStdString(k));
        }
        keys.sort();

        bool keyOk = false;
        const auto key = QInputDialog::getItem(this, "删除词典项", "选择原歌词", keys, 0, false, &keyOk);
        if (!keyOk || key.isEmpty()) {
            return;
        }

        m_lyricDictionary.erase(key.toStdString());
        m_statusLabel->setText(QString("Dictionary entry removed: %1").arg(key));
        return;
    }

    if (action == "查看词典") {
        if (m_lyricDictionary.empty()) {
            QMessageBox::information(this, "词典管理", "词典为空。");
            return;
        }

        QStringList rows;
        for (const auto& [from, to] : m_lyricDictionary) {
            rows.push_back(QString("%1 -> %2").arg(QString::fromStdString(from)).arg(QString::fromStdString(to)));
        }
        rows.sort();
        QMessageBox::information(this, "词典项", rows.join("\n"));
        return;
    }

    if (action == "清空词典") {
        m_lyricDictionary.clear();
        m_statusLabel->setText("Dictionary cleared");
    }
}

void MainWindow::openSettings() {
    QDialog dialog(this);
    dialog.setWindowTitle("Settings");

    auto* layout = new QFormLayout(&dialog);

    auto* threadSpin = new QSpinBox(&dialog);
    threadSpin->setRange(0, 128);
    threadSpin->setValue(static_cast<int>(m_settings.maxRenderThreads));
    threadSpin->setSpecialValueText("Auto");

    auto* gainSpin = new QDoubleSpinBox(&dialog);
    gainSpin->setRange(0.0, 2.0);
    gainSpin->setDecimals(2);
    gainSpin->setSingleStep(0.05);
    gainSpin->setValue(m_settings.masterGain);

    auto* sampleRateSpin = new QSpinBox(&dialog);
    sampleRateSpin->setRange(8000, 192000);
    sampleRateSpin->setSingleStep(1000);
    sampleRateSpin->setValue(m_settings.sampleRate);

    layout->addRow("Render Threads (0=Auto)", threadSpin);
    layout->addRow("Master Gain", gainSpin);
    layout->addRow("Sample Rate", sampleRateSpin);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    m_settings.maxRenderThreads = static_cast<unsigned int>(threadSpin->value());
    m_settings.masterGain = static_cast<float>(gainSpin->value());
    m_settings.sampleRate = sampleRateSpin->value();

    m_statusLabel->setText(QString("Settings saved | threads=%1 masterGain=%2 sampleRate=%3")
                               .arg(m_settings.maxRenderThreads)
                               .arg(QString::number(m_settings.masterGain, 'f', 2))
                               .arg(m_settings.sampleRate));
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

pyutau::core::Track MainWindow::applyDictionary(const pyutau::core::Track& track) const {
    auto replaced = track;
    for (auto& note : replaced.notes) {
        const auto found = m_lyricDictionary.find(note.lyric);
        if (found != m_lyricDictionary.end()) {
            note.lyric = found->second;
        }
    }
    return replaced;
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

        const auto mappedTrack = applyDictionary(track);
        auto rendered = m_resampler.renderTrack(mappedTrack, *vb, m_settings.sampleRate, m_settings.maxRenderThreads);
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
        sample = std::clamp(sample * m_settings.masterGain, -1.0F, 1.0F);
    }

    if (!m_audio.exportWav(path.toStdString(), mixdown, m_settings.sampleRate)) {
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
    fileMenu->addAction("Voicebank Manager", this, &MainWindow::manageVoicebanks);
    fileMenu->addAction("Dictionary", this, &MainWindow::manageDictionary);
    fileMenu->addAction("Settings", this, &MainWindow::openSettings);
    fileMenu->addAction("Add Track", this, &MainWindow::addTrack);
    fileMenu->addAction("Export WAV", this, &MainWindow::exportWav);

    auto* toolbar = addToolBar("Transport");
    toolbar->setMovable(false);
    toolbar->addAction("Open UST", this, &MainWindow::openUst);
    toolbar->addAction("Voicebank", this, &MainWindow::openVoicebank);
    toolbar->addAction("Dictionary", this, &MainWindow::manageDictionary);
    toolbar->addAction("Settings", this, &MainWindow::openSettings);
    toolbar->addAction("Add Track", this, &MainWindow::addTrack);
    toolbar->addAction("Render Mix", this, &MainWindow::exportWav);

    auto* root = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(root);
    auto* splitter = new QSplitter(Qt::Horizontal, root);

    m_trackList = new QListWidget(splitter);
    m_trackList->setMinimumWidth(320);
    connect(m_trackList, &QListWidget::currentRowChanged, this, &MainWindow::trackSelectionChanged);

    m_pianoRoll = new PianoRollWidget(splitter);

    auto* inspector = new QWidget(splitter);
    inspector->setMinimumWidth(300);
    auto* inspectorLayout = new QVBoxLayout(inspector);
    inspectorLayout->addWidget(new QLabel("Inspector 占位区\n- 轨道参数\n- 声库管理入口\n- 词典映射状态", inspector));
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
