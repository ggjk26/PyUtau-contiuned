#include "PianoRollWidget.h"

#include <algorithm>
#include <cmath>
#include <QPainter>
#include <QPainterPath>

namespace pyutau::gui {

namespace {
int clampSnap(int value) {
    return std::clamp(value, 15, 960);
}
}

PianoRollWidget::PianoRollWidget(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(2400, 1100);
    setAutoFillBackground(true);
}

void PianoRollWidget::setTrack(const pyutau::core::Track* track) {
    m_track = track;
    update();
}

void PianoRollWidget::setSnapTick(int snapTick) {
    m_snapTick = clampSnap(snapTick);
    update();
}

void PianoRollWidget::setPixelsPerQuarter(int pixels) {
    m_pixelsPerQuarter = std::clamp(pixels, 40, 360);
    update();
}

int PianoRollWidget::noteIndexAt(const QPoint& pos) const {
    if (!m_track) {
        return -1;
    }

    for (int i = 0; i < static_cast<int>(m_track->notes.size()); ++i) {
        const auto& note = m_track->notes[static_cast<std::size_t>(i)];
        const int x = note.startTick * m_pixelsPerQuarter / 480;
        const int w = std::max(8, note.durationTick * m_pixelsPerQuarter / 480);
        const int y = (83 - (note.pitch - 24)) * m_keyHeight;
        QRect rect(x, y, w, m_keyHeight - 1);
        if (rect.contains(pos)) {
            return i;
        }
    }
    return -1;
}

pyutau::core::Note PianoRollWidget::makeNoteAt(const QPoint& pos, int durationTick) const {
    pyutau::core::Note note;
    const int snappedTick = static_cast<int>(std::lround(static_cast<double>(pos.x()) * 480.0 / static_cast<double>(m_pixelsPerQuarter) / static_cast<double>(m_snapTick))) * m_snapTick;
    const int keyIndex = std::clamp(pos.y() / std::max(1, m_keyHeight), 0, 83);
    note.startTick = std::max(0, snappedTick);
    note.pitch = std::clamp(24 + (83 - keyIndex), 24, 107);
    note.durationTick = std::max(30, durationTick);
    note.lyric = "a";
    return note;
}

void PianoRollWidget::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor("#151820"));

    const int totalKeys = 84;
    for (int key = 0; key < totalKeys; ++key) {
        const int y = key * m_keyHeight;
        const bool isBlack = (key % 12 == 1 || key % 12 == 3 || key % 12 == 6 || key % 12 == 8 || key % 12 == 10);
        painter.fillRect(0, y, width(), m_keyHeight, isBlack ? QColor("#1d2430") : QColor("#202a39"));
    }

    painter.setPen(QColor("#30394a"));
    const int snapStepPx = std::max(6, m_snapTick * m_pixelsPerQuarter / 480);
    for (int x = 0; x < width(); x += snapStepPx) {
        painter.drawLine(x, 0, x, height());
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#4fc3f7"));

    if (!m_track) {
        return;
    }

    for (const auto& note : m_track->notes) {
        const int x = note.startTick * m_pixelsPerQuarter / 480;
        const int w = std::max(8, note.durationTick * m_pixelsPerQuarter / 480);
        const int y = (83 - (note.pitch - 24)) * m_keyHeight;

        painter.drawRoundedRect(x, y, w, m_keyHeight - 1, 3, 3);

        QPainterPath path;
        const int pointCount = std::max(8, std::min(128, w));
        for (int i = 0; i < pointCount; ++i) {
            const double t = static_cast<double>(i) / static_cast<double>(std::max(1, pointCount - 1));
            double cents = 0.0;

            if (!note.pitchBendCents.empty()) {
                const double pos = t * static_cast<double>(note.pitchBendCents.size() - 1);
                const auto idx = static_cast<std::size_t>(pos);
                const auto next = std::min(idx + 1, note.pitchBendCents.size() - 1);
                const double frac = pos - static_cast<double>(idx);
                cents += static_cast<double>(note.pitchBendCents[idx]) * (1.0 - frac) + static_cast<double>(note.pitchBendCents[next]) * frac;
            }

            const double yOffset = -cents / 100.0 * static_cast<double>(m_keyHeight);
            const QPointF pt(static_cast<double>(x) + t * static_cast<double>(w),
                             static_cast<double>(y) + static_cast<double>(m_keyHeight) / 2.0 + yOffset);
            if (i == 0) {
                path.moveTo(pt);
            } else {
                path.lineTo(pt);
            }
        }

        painter.setPen(QPen(QColor("#ffeb3b"), 1.2));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#4fc3f7"));
    }
}

} // namespace pyutau::gui
