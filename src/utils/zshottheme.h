// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Zshot Contributors

#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

class QLabel;
class QWidget;

/**
 * @brief Zshot look: design tokens taken from ZCRM (zcrm-web tokens.css) and
 * the stylesheet the Zshot windows (settings, background editor, launcher)
 * use. The capture overlay keeps its own painting.
 */
namespace ZshotTheme {

namespace Color {
inline const QColor brand{ 0x17, 0x86, 0xBE };
inline const QColor brandHover{ 0x0F, 0x6F, 0xA0 };
inline const QColor brandDark{ 0x0B, 0x58, 0x80 };
inline const QColor brandSoft{ 0xE4, 0xF1, 0xF8 };
inline const QColor navy{ 0x0F, 0x4A, 0x60 };
inline const QColor ink{ 0x14, 0x1A, 0x24 };
inline const QColor ink2{ 0x47, 0x50, 0x66 };
inline const QColor ink3{ 0x6B, 0x74, 0x88 };
inline const QColor line{ 0xE7, 0xEA, 0xF0 };
inline const QColor surface2{ 0xF7, 0xF9, 0xFC };
inline const QColor surface3{ 0xF1, 0xF4, 0xF9 };
inline const QColor canvas{ 0xEE, 0xF1, 0xF6 };
} // namespace Color

// Stylesheet for a Zshot window; widgets opt into the styles with the
// dynamic properties documented in zshottheme.cpp (zsCard, zsChoice, ...)
QString styleSheet();

// A Material icon of the app (":/img/material/black/<name>.svg") in `color`
QIcon icon(const QString& name, const QColor& color, int size = 18);

// Rich text for a keyboard key, for use inside a QLabel
QString kbd(const QString& key);

// Card frame with a bold title and an optional right-aligned value label
QWidget* card(const QString& title,
              QWidget* parent,
              QLabel** valueLabel = nullptr);

} // namespace ZshotTheme
