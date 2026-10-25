// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Zshot Contributors

#include "toggleswitch.h"
#include "utils/zshottheme.h"

#include <QPainter>

ToggleSwitch::ToggleSwitch(QWidget* parent)
  : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QSize ToggleSwitch::sizeHint() const
{
    return { 36, 20 };
}

void ToggleSwitch::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF track = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QColor trackColor =
      isChecked() ? ZshotTheme::Color::brand : QColor(0xD4, 0xD8, 0xDE);
    if (!isEnabled()) {
        trackColor.setAlpha(110);
    }
    painter.setPen(hasFocus() ? QPen(ZshotTheme::Color::brandDark, 1.5)
                              : Qt::NoPen);
    painter.setBrush(trackColor);
    painter.drawRoundedRect(track, track.height() / 2, track.height() / 2);
    const qreal knob = height() - 4;
    const qreal x = isChecked() ? width() - knob - 2 : 2;
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawEllipse(QRectF(x, 2, knob, knob));
}
