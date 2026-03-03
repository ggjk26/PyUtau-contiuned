#pragma once

#include "core/Project.h"

#include <QPoint>
#include <QWidget>

namespace pyutau::gui {

class PianoRollWidget : public QWidget {
    Q_OBJECT

public:
    explicit PianoRollWidget(QWidget* parent = nullptr);

    void setTrack(const pyutau::core::Track* track);
    void setSnapTick(int snapTick);
    void setPixelsPerQuarter(int pixels);
    [[nodiscard]] int noteIndexAt(const QPoint& pos) const;
    [[nodiscard]] pyutau::core::Note makeNoteAt(const QPoint& pos, int durationTick) const;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    const pyutau::core::Track* m_track = nullptr;
    int m_keyHeight = 12;
    int m_pixelsPerQuarter = 120;
    int m_snapTick = 120;
};

} // namespace pyutau::gui
