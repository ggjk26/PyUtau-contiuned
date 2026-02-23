#include "MainWindow.h"

#include <algorithm>
#include <cctype>
#include <random>
#include <unordered_map>
#include <unordered_set>

#include <QDialog>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QApplication>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QProgressBar>
#include <QMenuBar>
#include <QMessageBox>
#include <QRegularExpression>
#include <QProcess>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace pyutau::gui {

namespace {
constexpr const char* kCurrentVersion = "0.1.0-preview";
constexpr const char* kFallbackGithubRepo = "OpenUtau/PyUtau-contiuned";

std::string normalizeAliasToken(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        if (!std::isspace(static_cast<unsigned char>(ch))) {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
    }
    return out;
}

std::string kanaToRomaji(const std::string& token) {
    static const std::unordered_map<std::string, std::string> kMap{
        {"あ", "a"}, {"い", "i"}, {"う", "u"}, {"え", "e"}, {"お", "o"},
        {"か", "ka"}, {"き", "ki"}, {"く", "ku"}, {"け", "ke"}, {"こ", "ko"},
        {"さ", "sa"}, {"し", "shi"}, {"す", "su"}, {"せ", "se"}, {"そ", "so"},
        {"た", "ta"}, {"ち", "chi"}, {"つ", "tsu"}, {"て", "te"}, {"と", "to"},
        {"な", "na"}, {"に", "ni"}, {"ぬ", "nu"}, {"ね", "ne"}, {"の", "no"},
        {"は", "ha"}, {"ひ", "hi"}, {"ふ", "fu"}, {"へ", "he"}, {"ほ", "ho"},
        {"ま", "ma"}, {"み", "mi"}, {"む", "mu"}, {"め", "me"}, {"も", "mo"},
        {"や", "ya"}, {"ゆ", "yu"}, {"よ", "yo"},
        {"ら", "ra"}, {"り", "ri"}, {"る", "ru"}, {"れ", "re"}, {"ろ", "ro"},
        {"わ", "wa"}, {"を", "wo"}, {"ん", "n"},
        {"が", "ga"}, {"ぎ", "gi"}, {"ぐ", "gu"}, {"げ", "ge"}, {"ご", "go"},
        {"ざ", "za"}, {"じ", "ji"}, {"ず", "zu"}, {"ぜ", "ze"}, {"ぞ", "zo"},
        {"だ", "da"}, {"で", "de"}, {"ど", "do"},
        {"ば", "ba"}, {"び", "bi"}, {"ぶ", "bu"}, {"べ", "be"}, {"ぼ", "bo"},
        {"ぱ", "pa"}, {"ぴ", "pi"}, {"ぷ", "pu"}, {"ぺ", "pe"}, {"ぽ", "po"},
    };
    const auto found = kMap.find(token);
    if (found != kMap.end()) {
        return found->second;
    }
    return normalizeAliasToken(token);
}

char extractLastVowel(const std::string& token) {
    for (auto it = token.rbegin(); it != token.rend(); ++it) {
        const char c = *it;
        if (c == 'a' || c == 'i' || c == 'u' || c == 'e' || c == 'o') {
            return c;
        }
    }
    return 'a';
}
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    updateWindowTitle();
    resize(1400, 900);

    buildUi();
    applySynthVInspiredStyle();

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

void MainWindow::updateWindowTitle() {
    setWindowTitle(QString("PyUTAU Continued - %1").arg(m_projectFileName));
}

void MainWindow::openUst() {
    const auto path = QFileDialog::getOpenFileName(this, "Open UST / USTX", {}, "UTAU project files (*.ust *.ustx)");
    if (path.isEmpty()) {
        return;
    }

    try {
        m_project = m_parser.parse(path.toStdString());
        m_projectFileName = QFileInfo(path).fileName();
        if (m_projectFileName.isEmpty()) {
            m_projectFileName = "未命名";
        }
        updateWindowTitle();
        bindProjectToUi();
        m_statusLabel->setText(QString("Loaded Project: %1").arg(path));
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Load failed", ex.what());
    }
}

void MainWindow::openVoicebank() {
    manageVoicebanks();
}

void MainWindow::manageVoicebanks() {
    const QStringList operations{
        "添加声库（基础）",
        "添加声库（与OpenUTAU同步）",
        "移除声库",
        "分配声库到当前音轨",
        "查看已加载声库",
    };

    bool ok = false;
    const auto action = QInputDialog::getItem(this, "声库管理", "请选择操作", operations, 0, false, &ok);
    if (!ok || action.isEmpty()) {
        return;
    }

    if (action == "添加声库（基础）" || action == "添加声库（与OpenUTAU同步）") {
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
        pyutau::core::OpenUtauSyncReport syncReport;

        const bool openUtauSync = (action == "添加声库（与OpenUTAU同步）");
        if (openUtauSync) {
            syncReport = vb.loadFromDirectoryWithOpenUtauSync(path.toStdString());
            if (!syncReport.ok) {
                QMessageBox::warning(this, "Voicebank", "OpenUTAU singer import failed (no valid oto aliases found).");
                return;
            }
        } else if (!vb.loadFromDirectory(path.toStdString())) {
            QMessageBox::warning(this, "Voicebank", "oto.ini not found or empty.");
            return;
        }

        const auto key = name.toStdString();
        m_voicebankPool[key] = std::move(vb);

        const auto report = m_voicebankPool[key].runAiPostProcessAndRetrain(m_settings.enableVoicebankAiRetrain);
        if (openUtauSync) {
            const auto singerName = syncReport.singerName.empty() ? QString("(unknown)") : QString::fromStdString(syncReport.singerName);
            m_statusLabel->setText(QString("Voicebank synced: %1 | singer=%2 | otoFiles=%3 aliases=%4 | AI=%5 generated=%6")
                                       .arg(name)
                                       .arg(singerName)
                                       .arg(syncReport.otoFileCount)
                                       .arg(m_voicebankPool[key].size())
                                       .arg(report.enabled ? "ON" : "OFF")
                                       .arg(report.generatedAliases));
        } else {
            m_statusLabel->setText(QString("Voicebank added: %1 (%2 aliases) | AI retrain=%3 generated=%4")
                                       .arg(name)
                                       .arg(m_voicebankPool[key].size())
                                       .arg(report.enabled ? "ON" : "OFF")
                                       .arg(report.generatedAliases));
        }
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

    auto* retrainCheck = new QCheckBox("Enable AI retrain after importing voicebank", &dialog);
    retrainCheck->setChecked(m_settings.enableVoicebankAiRetrain);

    auto* autoPitchCheck = new QCheckBox("Enable auto pitch-line enhancement", &dialog);
    autoPitchCheck->setChecked(m_settings.enableAutoPitchLine);

    auto* vibratoDepthSpin = new QSpinBox(&dialog);
    vibratoDepthSpin->setRange(0, 120);
    vibratoDepthSpin->setValue(m_settings.autoVibratoDepthCents);

    auto* vibratoRateSpin = new QDoubleSpinBox(&dialog);
    vibratoRateSpin->setRange(0.1, 12.0);
    vibratoRateSpin->setDecimals(2);
    vibratoRateSpin->setSingleStep(0.1);
    vibratoRateSpin->setValue(m_settings.autoVibratoRateHz);

    auto* lowEndCheck = new QCheckBox("Low-end device mode (lower CPU/memory)", &dialog);
    lowEndCheck->setChecked(m_settings.lowEndDeviceMode);

    layout->addRow("Render Threads (0=Auto)", threadSpin);
    layout->addRow("Master Gain", gainSpin);
    layout->addRow("Sample Rate", sampleRateSpin);
    layout->addRow("Voicebank", retrainCheck);
    layout->addRow("Pitch", autoPitchCheck);
    layout->addRow("Vibrato Depth (cents)", vibratoDepthSpin);
    layout->addRow("Vibrato Rate (Hz)", vibratoRateSpin);
    layout->addRow("Performance", lowEndCheck);

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
    m_settings.enableVoicebankAiRetrain = retrainCheck->isChecked();
    m_settings.enableAutoPitchLine = autoPitchCheck->isChecked();
    m_settings.autoVibratoDepthCents = vibratoDepthSpin->value();
    m_settings.autoVibratoRateHz = vibratoRateSpin->value();
    m_settings.lowEndDeviceMode = lowEndCheck->isChecked();

    m_statusLabel->setText(QString("Settings saved | threads=%1 masterGain=%2 sampleRate=%3 aiRetrain=%4 autoPitch=%5 vib=%6c/%7Hz lowEnd=%8")
                               .arg(m_settings.maxRenderThreads)
                               .arg(QString::number(m_settings.masterGain, 'f', 2))
                               .arg(m_settings.sampleRate)
                               .arg(m_settings.enableVoicebankAiRetrain ? "ON" : "OFF")
                               .arg(m_settings.enableAutoPitchLine ? "ON" : "OFF")
                               .arg(m_settings.autoVibratoDepthCents)
                               .arg(QString::number(m_settings.autoVibratoRateHz, 'f', 2))
                               .arg(m_settings.lowEndDeviceMode ? "ON" : "OFF"));
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

void MainWindow::openTrackSettings() {
    if (!m_trackList || m_trackList->currentRow() < 0 || m_trackList->currentRow() >= static_cast<int>(m_project.tracks().size())) {
        QMessageBox::information(this, "Track Settings", "请先选中一个音轨。");
        return;
    }

    auto& track = m_project.tracks()[static_cast<std::size_t>(m_trackList->currentRow())];

    QDialog dialog(this);
    dialog.setWindowTitle("Track Settings");
    auto* layout = new QFormLayout(&dialog);

    auto* gainSpin = new QDoubleSpinBox(&dialog);
    gainSpin->setRange(0.0, 2.0);
    gainSpin->setDecimals(2);
    gainSpin->setSingleStep(0.05);
    gainSpin->setValue(track.gain);

    auto* muteCheck = new QCheckBox("Mute this track", &dialog);
    muteCheck->setChecked(track.muted);

    auto* phonemizerBox = new QComboBox(&dialog);
    phonemizerBox->addItem("JP VCV & CVVC Phonemizer (Default)", 0);
    phonemizerBox->addItem("DiffSinger Japanese Phonemizer", 1);
    phonemizerBox->addItem("PyUTAU Native Phonemizer", 2);
    phonemizerBox->setCurrentIndex(std::clamp(track.phonemizerType, 0, 2));

    layout->addRow("Gain", gainSpin);
    layout->addRow("Track", muteCheck);
    layout->addRow("Phonemizer", phonemizerBox);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    track.gain = static_cast<float>(gainSpin->value());
    track.muted = muteCheck->isChecked();
    track.phonemizerType = phonemizerBox->currentIndex();

    const int selectedRow = m_trackList->currentRow();
    bindProjectToUi();
    m_trackList->setCurrentRow(selectedRow);
    m_statusLabel->setText(QString("Track updated: %1 | gain=%2 | muted=%3 | phonemizer=%4")
                               .arg(QString::fromStdString(track.name))
                               .arg(QString::number(track.gain, 'f', 2))
                               .arg(track.muted ? "ON" : "OFF")
                               .arg(phonemizerTypeToLabel(track.phonemizerType)));
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

pyutau::core::Track MainWindow::applyPitchEnhancements(const pyutau::core::Track& track) const {
    auto enhanced = track;
    for (auto& note : enhanced.notes) {
        note.autoPitchEnabled = m_settings.enableAutoPitchLine;
        note.autoVibratoDepthCents = m_settings.autoVibratoDepthCents;
        note.autoVibratoRateHz = m_settings.autoVibratoRateHz;
    }
    return enhanced;
}

pyutau::core::Track MainWindow::applyPhonemizer(const pyutau::core::Track& track) const {
    auto phonemized = track;

    std::string prev = "sil";
    for (auto& note : phonemized.notes) {
        if (note.lyric.empty()) {
            note.lyric = "a";
        }

        const auto baseToken = kanaToRomaji(note.lyric);
        const auto mode = static_cast<PhonemizerType>(std::clamp(track.phonemizerType, 0, 2));

        switch (mode) {
            case PhonemizerType::JpVcvCvvc: {
                if (prev == "sil") {
                    note.lyric = "- " + baseToken;
                } else {
                    const std::string vcv = prev + " " + baseToken;
                    const std::string cvvcTail(1, extractLastVowel(baseToken));
                    note.lyric = vcv + " | " + baseToken + " " + cvvcTail;
                }
                break;
            }
            case PhonemizerType::DiffSingerJapanese: {
                note.lyric = "dsj/" + baseToken;
                break;
            }
            case PhonemizerType::PyUtauNative:
            default: {
                note.lyric = "py:" + baseToken;
                break;
            }
        }

        prev = std::string(1, extractLastVowel(baseToken));
    }

    return phonemized;
}

void MainWindow::updateRenderProgress(int current, int total, const QString& stage) {
    if (!m_renderProgress) {
        return;
    }

    if (total <= 0) {
        m_renderProgress->setRange(0, 1);
        m_renderProgress->setValue(0);
        m_renderProgress->setFormat(stage + " %p%");
    } else {
        m_renderProgress->setRange(0, total);
        m_renderProgress->setValue(std::clamp(current, 0, total));
        m_renderProgress->setFormat(stage + " %p%");
    }

    statusBar()->showMessage(stage, 1000);
    qApp->processEvents();
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

    QDialog optionDialog(this);
    optionDialog.setWindowTitle("导出音频选项");
    auto* optionLayout = new QFormLayout(&optionDialog);

    auto* sampleRateBox = new QComboBox(&optionDialog);
    sampleRateBox->addItems({"22050", "32000", "44100", "48000"});
    sampleRateBox->setCurrentText(QString::number(m_settings.lowEndDeviceMode ? std::min(m_settings.sampleRate, 32000) : m_settings.sampleRate));

    auto* bitDepthBox = new QComboBox(&optionDialog);
    bitDepthBox->addItems({"16", "24", "32"});
    bitDepthBox->setCurrentText("16");

    auto* channelsBox = new QComboBox(&optionDialog);
    channelsBox->addItems({"1", "2"});
    channelsBox->setCurrentText("2");

    auto* breathBox = new QComboBox(&optionDialog);
    breathBox->addItems({"关闭", "轻", "中", "强"});
    breathBox->setCurrentIndex(0);

    optionLayout->addRow("采样率 (Hz)", sampleRateBox);
    optionLayout->addRow("比特率/位深 (bit)", bitDepthBox);
    optionLayout->addRow("声道数量", channelsBox);
    optionLayout->addRow("气音", breathBox);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &optionDialog);
    optionLayout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &optionDialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &optionDialog, &QDialog::reject);

    if (optionDialog.exec() != QDialog::Accepted) {
        return;
    }

    int effectiveSampleRate = sampleRateBox->currentText().toInt();
    if (m_settings.lowEndDeviceMode) {
        effectiveSampleRate = std::min(effectiveSampleRate, 32000);
    }
    const int bitsPerSample = bitDepthBox->currentText().toInt();
    const int channels = channelsBox->currentText().toInt();
    const auto breathMode = breathBox->currentText();
    const unsigned int effectiveThreads = m_settings.lowEndDeviceMode ? 1U : m_settings.maxRenderThreads;

    std::vector<float> mixdown;
    std::size_t usedTracks = 0;
    int progressStep = 0;
    std::size_t totalWorkers = 0;
    double totalRenderMs = 0.0;

    int activeTrackCount = 0;
    std::unordered_set<int> phonemizersInUse;
    for (const auto& track : m_project.tracks()) {
        if (!track.muted) {
            ++activeTrackCount;
            phonemizersInUse.insert(std::clamp(track.phonemizerType, 0, 2));
        }
    }

    if (phonemizersInUse.size() >= 3 && !m_settings.skipMultiPhonemizerWarning) {
        QDialog warningDialog(this);
        warningDialog.setWindowTitle("多音素器警告");
        auto* warningLayout = new QVBoxLayout(&warningDialog);
        warningLayout->addWidget(new QLabel("检测到 3 种及以上音素器同时参与合成。\n这会导致合成速度明显下降，并可能造成程序崩溃。", &warningDialog));

        auto* dontShowAgain = new QCheckBox("不再弹出", &warningDialog);
        warningLayout->addWidget(dontShowAgain);

        auto* buttons = new QDialogButtonBox(&warningDialog);
        auto* continueBtn = buttons->addButton("我已知晓，继续", QDialogButtonBox::AcceptRole);
        auto* abortBtn = buttons->addButton("放弃生成", QDialogButtonBox::RejectRole);
        (void)continueBtn;
        (void)abortBtn;
        warningLayout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &warningDialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &warningDialog, &QDialog::reject);

        if (warningDialog.exec() != QDialog::Accepted) {
            m_statusLabel->setText("Export cancelled by user due to multi-phonemizer warning.");
            return;
        }

        if (dontShowAgain->isChecked()) {
            m_settings.skipMultiPhonemizerWarning = true;
        }
    }

    constexpr int postStages = 3;
    const int totalStages = std::max(1, activeTrackCount + postStages);
    updateRenderProgress(0, totalStages, "Preparing export");

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
        const auto phonemizedTrack = applyPhonemizer(mappedTrack);
        const auto pitchedTrack = applyPitchEnhancements(phonemizedTrack);
        auto rendered = m_resampler.renderTrack(pitchedTrack, *vb, effectiveSampleRate, effectiveThreads);
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
        ++progressStep;
        updateRenderProgress(progressStep, totalStages, QString("Rendering tracks (%1/%2)").arg(progressStep).arg(activeTrackCount));
    }

    if (usedTracks == 0) {
        m_renderProgress->setValue(0);
        m_renderProgress->setFormat("Ready");
        QMessageBox::warning(this, "Export", "没有可导出的有效音轨（音轨可能为空或全部静音）。");
        return;
    }

    ++progressStep;
    updateRenderProgress(progressStep, totalStages, "Applying post effects");

    // Breath noise layer (simple aspirate simulation).
    float breathGain = 0.0F;
    if (breathMode == "轻") {
        breathGain = 0.008F;
    } else if (breathMode == "中") {
        breathGain = 0.015F;
    } else if (breathMode == "强") {
        breathGain = 0.025F;
    }

    if (breathGain > 0.0F) {
        std::mt19937 rng(42U);
        std::uniform_real_distribution<float> noiseDist(-1.0F, 1.0F);
        float last = 0.0F;
        for (auto& sample : mixdown) {
            const float white = noiseDist(rng);
            last = 0.9F * last + 0.1F * white;
            sample += breathGain * last;
        }
    }

    ++progressStep;
    updateRenderProgress(progressStep, totalStages, "Final limiting");
    for (auto& sample : mixdown) {
        sample = std::clamp(sample * m_settings.masterGain, -1.0F, 1.0F);
    }

    ++progressStep;
    updateRenderProgress(progressStep, totalStages, "Writing WAV");
    if (!m_audio.exportWav(path.toStdString(), mixdown, effectiveSampleRate, bitsPerSample, channels)) {
        m_renderProgress->setValue(0);
        m_renderProgress->setFormat("Ready");
        QMessageBox::critical(this, "Export", "Failed to write wav file.");
        return;
    }

    m_renderProgress->setValue(totalStages);
    m_renderProgress->setFormat("Ready");

    const int nominalKbps = effectiveSampleRate * bitsPerSample * channels / 1000;
    const QString phonemizerSummary = QString::number(static_cast<int>(phonemizersInUse.size())) + " types";
    m_statusLabel->setText(QString("Exported: %1 | tracks %2 | %3Hz/%4bit/%5ch (~%6kbps) | breath=%7 | phonemizer=%8 | render %9 ms")
                               .arg(path)
                               .arg(usedTracks)
                               .arg(effectiveSampleRate)
                               .arg(bitsPerSample)
                               .arg(channels)
                               .arg(nominalKbps)
                               .arg(breathMode)
                               .arg(phonemizerSummary)
                               .arg(QString::number(totalRenderMs, 'f', 2)));
}

bool MainWindow::tryParseVersionTag(const std::string& tag, VersionToken& out) {
    QRegularExpression re("^v?(\\d+)\\.(\\d+)\\.(\\d+)(?:[-_]?([A-Za-z]+))?.*$");
    const auto m = re.match(QString::fromStdString(tag));
    if (!m.hasMatch()) {
        return false;
    }

    out.major = m.captured(1).toInt();
    out.minor = m.captured(2).toInt();
    out.patch = m.captured(3).toInt();

    const auto stageText = m.captured(4).toLower();
    if (stageText == "preview") {
        out.stage = 0;
    } else if (stageText == "alpha") {
        out.stage = 1;
    } else if (stageText == "beta") {
        out.stage = 2;
    } else {
        out.stage = 3;
    }

    return true;
}

int MainWindow::compareVersion(const VersionToken& lhs, const VersionToken& rhs) {
    if (lhs.major != rhs.major) return lhs.major < rhs.major ? -1 : 1;
    if (lhs.minor != rhs.minor) return lhs.minor < rhs.minor ? -1 : 1;
    if (lhs.patch != rhs.patch) return lhs.patch < rhs.patch ? -1 : 1;
    if (lhs.stage != rhs.stage) return lhs.stage < rhs.stage ? -1 : 1;
    return 0;
}

std::string MainWindow::detectGitHubRepoFromGitRemote() {
    QProcess process;
    process.start("git", {"config", "--get", "remote.origin.url"});
    if (!process.waitForFinished(2000)) {
        return kFallbackGithubRepo;
    }

    const auto url = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    if (url.isEmpty()) {
        return kFallbackGithubRepo;
    }

    QRegularExpression httpsRe("github\\.com[:/](.+?/.+?)(?:\\.git)?$");
    const auto match = httpsRe.match(url);
    if (match.hasMatch()) {
        return match.captured(1).toStdString();
    }

    return kFallbackGithubRepo;
}

std::string MainWindow::fetchLatestReleaseTag(const std::string& repo) {
    QProcess process;
    const auto api = QString("https://api.github.com/repos/%1/releases/latest").arg(QString::fromStdString(repo));
    process.start("curl", {"-L", "-s", api});
    if (!process.waitForFinished(8000)) {
        return {};
    }

    const auto json = QString::fromUtf8(process.readAllStandardOutput());
    QRegularExpression tagRe("\"tag_name\"\\s*:\\s*\"([^\"]+)\"");
    const auto match = tagRe.match(json);
    if (!match.hasMatch()) {
        return {};
    }
    return match.captured(1).toStdString();
}

std::vector<std::pair<QString, QString>> MainWindow::fetchOpenIssues(const std::string& repo) {
    QProcess process;
    const auto api = QString("https://api.github.com/repos/%1/issues?state=open&per_page=30").arg(QString::fromStdString(repo));
    process.start("curl", {"-L", "-s", api});
    if (!process.waitForFinished(8000)) {
        return {};
    }

    const auto json = QString::fromUtf8(process.readAllStandardOutput());
    std::vector<std::pair<QString, QString>> issues;

    QRegularExpression objectRe(R"(\{[^\{\}]*"title"\s*:\s*"([^"]+)"[^\{\}]*"html_url"\s*:\s*"([^"]+/issues/\d+)"[^\{\}]*\})");
    auto it = objectRe.globalMatch(json);
    while (it.hasNext()) {
        const auto m = it.next();
        issues.emplace_back(m.captured(1), m.captured(2));
    }

    return issues;
}

void MainWindow::showIssueFeedbackDialog() {
    const auto repo = detectGitHubRepoFromGitRemote();
    const QString repoText = QString::fromStdString(repo);

    QDialog dialog(this);
    dialog.setWindowTitle("问题反馈");
    dialog.resize(720, 520);

    auto* layout = new QVBoxLayout(&dialog);
    auto* hint = new QLabel(QString("当前仓库：%1\n可查看 GitHub Issues，并跳转创建新 Issue。双击条目可在浏览器打开。").arg(repoText), &dialog);
    hint->setWordWrap(true);

    auto* issueList = new QListWidget(&dialog);

    auto refreshIssues = [issueList, repo]() {
        issueList->clear();
        const auto issues = MainWindow::fetchOpenIssues(repo);
        if (issues.empty()) {
            issueList->addItem("未获取到 issue（可能无公开 issue、API 限流或网络异常）。");
            return;
        }

        for (const auto& [title, url] : issues) {
            auto* item = new QListWidgetItem(title, issueList);
            item->setToolTip(url);
            item->setData(Qt::UserRole, url);
        }
    };

    auto* refreshBtn = new QPushButton("刷新 Issues", &dialog);
    QObject::connect(refreshBtn, &QPushButton::clicked, &dialog, refreshIssues);

    QObject::connect(issueList, &QListWidget::itemDoubleClicked, &dialog, [](QListWidgetItem* item) {
        const auto url = item->data(Qt::UserRole).toString();
        if (!url.isEmpty()) {
            QDesktopServices::openUrl(QUrl(url));
        }
    });

    auto* createBtn = new QPushButton("创建 Issue", &dialog);
    QObject::connect(createBtn, &QPushButton::clicked, &dialog, [repoText]() {
        QDesktopServices::openUrl(QUrl(QString("https://github.com/%1/issues/new").arg(repoText)));
    });

    auto* closeBtn = new QPushButton("关闭", &dialog);
    QObject::connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    auto* actionRow = new QHBoxLayout();
    actionRow->addWidget(refreshBtn);
    actionRow->addWidget(createBtn);
    actionRow->addStretch(1);
    actionRow->addWidget(closeBtn);

    layout->addWidget(hint);
    layout->addWidget(issueList, 1);
    layout->addLayout(actionRow);

    refreshIssues();
    dialog.exec();
}

void MainWindow::showAboutAndUpdates() {
    QDialog dialog(this);
    dialog.setWindowTitle("关于 / 更新");
    dialog.resize(620, 420);

    auto* layout = new QVBoxLayout(&dialog);
    auto* info = new QLabel(QString("<b>PyUtau Continued</b><br/>当前版本：%1").arg(kCurrentVersion), &dialog);
    info->setTextFormat(Qt::RichText);

    auto* log = new QPlainTextEdit(&dialog);
    log->setReadOnly(true);
    log->setPlainText("点击“手动检查更新”从 GitHub Releases 获取最新版本。\n版本顺序：preview < alpha < beta < release。");

    auto* checkBtn = new QPushButton("手动检查更新", &dialog);
    auto* feedbackBtn = new QPushButton("问题反馈", &dialog);
    QObject::connect(checkBtn, &QPushButton::clicked, &dialog, [log]() {
        const auto repo = MainWindow::detectGitHubRepoFromGitRemote();
        const auto latestTag = MainWindow::fetchLatestReleaseTag(repo);

        if (latestTag.empty()) {
            log->appendPlainText("更新检查失败：无法从 GitHub Releases 获取 tag_name。\n请检查网络或仓库 release 设置。");
            return;
        }

        MainWindow::VersionToken current{};
        MainWindow::VersionToken latest{};
        if (!MainWindow::tryParseVersionTag(kCurrentVersion, current) || !MainWindow::tryParseVersionTag(latestTag, latest)) {
            log->appendPlainText(QString("版本解析失败。当前=%1, 最新=%2").arg(kCurrentVersion).arg(QString::fromStdString(latestTag)));
            return;
        }

        const int cmp = MainWindow::compareVersion(current, latest);
        log->appendPlainText(QString("仓库：%1\n当前：%2\n最新：%3")
                                 .arg(QString::fromStdString(repo))
                                 .arg(kCurrentVersion)
                                 .arg(QString::fromStdString(latestTag)));
        if (cmp < 0) {
            log->appendPlainText("发现新版本，可更新。\n");
        } else if (cmp == 0) {
            log->appendPlainText("当前已是最新版本。\n");
        } else {
            log->appendPlainText("当前版本高于最新 release（可能为开发版）。\n");
        }
    });

    QObject::connect(feedbackBtn, &QPushButton::clicked, this, &MainWindow::showIssueFeedbackDialog);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

    layout->addWidget(info);
    layout->addWidget(checkBtn);
    layout->addWidget(feedbackBtn);
    layout->addWidget(log, 1);
    layout->addWidget(buttons);

    dialog.exec();
}

void MainWindow::applySynthVInspiredStyle() {
    const QString style = R"(
QMainWindow { background: #1a1c22; color: #d7dce6; }
QMenuBar { background: #232733; color: #d7dce6; }
QMenuBar::item:selected { background: #3a4154; }
QMenu { background: #232733; color: #d7dce6; border: 1px solid #3a4154; }
QMenu::item:selected { background: #3a4154; }
QToolBar { background: #20242f; border-bottom: 1px solid #3a4154; spacing: 4px; }
QToolButton { background: #2d3342; color: #e2e7f0; border: 1px solid #444d65; border-radius: 4px; padding: 4px 8px; }
QToolButton:hover { background: #3b4358; }
QListWidget { background: #151821; color: #d7dce6; border: 1px solid #3a4154; }
QListWidget::item:selected { background: #4d6aa8; color: #ffffff; }
QStatusBar { background: #1f2330; color: #d7dce6; }
QLabel { color: #d7dce6; }
QProgressBar { background: #11141c; border: 1px solid #3b4358; border-radius: 4px; text-align: center; color: #d7dce6; min-width: 220px; }
QProgressBar::chunk { background-color: #5c83d6; }
)";
    setStyleSheet(style);
}

QString MainWindow::phonemizerTypeToLabel(int type) {
    const auto clamped = static_cast<PhonemizerType>(std::clamp(type, 0, 2));
    switch (clamped) {
        case PhonemizerType::DiffSingerJapanese: return "DiffSinger JP";
        case PhonemizerType::PyUtauNative: return "PyUTAU Native";
        case PhonemizerType::JpVcvCvvc:
        default: return "JP VCV&CVVC";
    }
}

void MainWindow::buildUi() {
    auto* fileMenu = menuBar()->addMenu("File");
    fileMenu->addAction("Open UST/USTX", this, &MainWindow::openUst);
    fileMenu->addAction("Voicebank Manager", this, &MainWindow::manageVoicebanks);
    fileMenu->addAction("Dictionary", this, &MainWindow::manageDictionary);
    fileMenu->addAction("Settings", this, &MainWindow::openSettings);
    fileMenu->addAction("Add Track", this, &MainWindow::addTrack);
    fileMenu->addAction("Track Settings", this, &MainWindow::openTrackSettings);
    fileMenu->addAction("Export WAV", this, &MainWindow::exportWav);

    auto* helpMenu = menuBar()->addMenu("Help");
    helpMenu->addAction("About / Updates", this, &MainWindow::showAboutAndUpdates);

    auto* toolbar = addToolBar("Transport");
    toolbar->setMovable(false);
    toolbar->addAction("Open UST/USTX", this, &MainWindow::openUst);
    toolbar->addAction("Voicebank", this, &MainWindow::openVoicebank);
    toolbar->addAction("Dictionary", this, &MainWindow::manageDictionary);
    toolbar->addAction("Settings", this, &MainWindow::openSettings);
    toolbar->addAction("Add Track", this, &MainWindow::addTrack);
    toolbar->addAction("Track Settings", this, &MainWindow::openTrackSettings);
    toolbar->addAction("Render Mix", this, &MainWindow::exportWav);
    toolbar->addAction("About/Updates", this, &MainWindow::showAboutAndUpdates);

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
    m_renderProgress = new QProgressBar(this);
    m_renderProgress->setRange(0, 100);
    m_renderProgress->setValue(0);
    m_renderProgress->setFormat("Ready");
    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_renderProgress);
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
                                 .arg(voicebankText + " | " + phonemizerTypeToLabel(track.phonemizerType)));
    }

    if (!m_project.tracks().empty()) {
        m_trackList->setCurrentRow(0);
        m_pianoRoll->setTrack(&m_project.tracks().front());
    }
}

} // namespace pyutau::gui
