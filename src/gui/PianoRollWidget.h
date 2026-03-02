#pragma once

#include "core/Project.h"

#include <QWidget>

namespace pyutau::gui {

class PianoRollWidget : public QWidget {
    Q_OBJECT

public:
    explicit PianoRollWidget(QWidget* parent = nullptr);

    void setTrack(const pyutau::core::Track* track);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    const pyutau::core::Track* m_track = nullptr;
    int m_keyHeight = 12;
    int m_tickWidth = 1;
};

} // namespace pyutau::gui
