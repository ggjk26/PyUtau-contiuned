#include "PianoRollWidget.h"

#include <algorithm>
#include <cmath>
#include <QPainter>
#include <QPainterPath>

namespace pyutau::gui {

PianoRollWidget::PianoRollWidget(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(900, 520);
    setAutoFillBackground(true);
}

void PianoRollWidget::setTrack(const pyutau::core::Track* track) {
    m_track = track;
    update();
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
    for (int tick = 0; tick < width(); tick += 120 * m_tickWidth) {
        painter.drawLine(tick, 0, tick, height());
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#4fc3f7"));

    if (!m_track) {
        return;
    }

    for (const auto& note : m_track->notes) {
        const int x = note.startTick * m_tickWidth / 8;
        const int w = std::max(8, note.durationTick * m_tickWidth / 8);
        const int y = (83 - (note.pitch - 24)) * m_keyHeight;

        painter.drawRoundedRect(x, y, w, m_keyHeight - 1, 3, 3);

        // Pitch-line overlay (manual + auto-curve preview).
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

            if (note.autoPitchEnabled) {
                if (t < 0.12) {
                    cents += -28.0 * (1.0 - (t / 0.12));
                }
                if (t > 0.35) {
                    const double env = std::min(1.0, (t - 0.35) / 0.20);
                    cents += static_cast<double>(note.autoVibratoDepthCents) * env * std::sin(2.0 * 3.14159265358979323846 * note.autoVibratoRateHz * t);
                }
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

        painter.setPen(QPen(QColor("#ffeb3b"), 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#4fc3f7"));
    }
}

} // namespace pyutau::gui
