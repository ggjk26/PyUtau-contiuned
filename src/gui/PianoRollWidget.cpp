#include "PianoRollWidget.h"

#include <algorithm>
#include <QPainter>

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
    }
}

} // namespace pyutau::gui
