// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Nguyễn Tiến Lộc (Loc Nguyen)
/*
 * ============================================================================
 *  ZSHOT ATTRIBUTION BANNER — GPLv3 SECTION 7(b) TERM, SEE NOTICE
 * ============================================================================
 *
 *  This banner shows the contact and services of the Zshot maintainer
 *  (Nguyễn Tiến Lộc, https://locnguyendata.com). It is part of the
 *  Appropriate Legal Notices of the program: removing it, hiding it,
 *  changing its link or text, or weakening the checks below in a
 *  redistributed or modified version is not allowed without a commercial
 *  license from locnt@locnguyendata.com. Read the NOTICE file at the root of
 *  the repository for the exact terms.
 *
 *  The banner is hidden only when a commercial license key is configured:
 *  environment variable FRIENDS, or the "friends" option of zshot.ini.
 * ============================================================================
 */

#include "attributionbanner.h"
#include "utils/confighandler.h"

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QEnterEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QUrl>
#include <cmath>

namespace {
// Encoded payloads, decoded at run time. The integrity sums below detect an
// edited payload: the banner then shows a license warning instead.
const char* const TEXT_CHUNKS[] = { "8J+boCBUxrAgduG6pW4sIGN1c3RvbSB0aMOq",
                                    "bSB0w61uaCBuxINuZywgdHJp4buDbiBraGFp",
                                    "IFpzaG90IGNobyBkb2FuaCBuZ2hp4buHcCBs",
                                    "acOqbiBo4buHIFdlYnNpdGU6IGxvY25ndXll",
                                    "bmRhdGEuY29t" };
const char* const LINK_CHUNKS[] = { "aHR0cHM6Ly9sb2NuZ3V5ZW5kYXRhLmNvbS8/",
                                    "dXRtX3NvdXJjZT16c2hvdF9hcHAmdXRtX21l",
                                    "ZGl1bT0lMV9tYXJxdWVlJnV0bV9jYW1wYWln",
                                    "bj1zZXJ2aWNlX3Byb21vJnV0bV9jb250ZW50",
                                    "PWNvbnRhY3RfYmFubmVy" };
constexpr int TEXT_CHECKSUM = 231;
constexpr int LINK_CHECKSUM = 86;
// SHA-256 of the commercial license key (same key as ZCRM CE)
constexpr char LICENSE_HASH[] =
  "d6866658cdd1c1b5766159f986dcba98a1369e4cb95fd141577d043286acca5f";

constexpr int HEIGHT = 30;
constexpr int GAP = 120;
constexpr int TICK_MS = 16;
constexpr qreal SPEED = 0.7; // px per tick

template<size_t N>
QString decode(const char* const (&chunks)[N])
{
    QByteArray data;
    for (const char* chunk : chunks) {
        data += chunk;
    }
    return QString::fromUtf8(QByteArray::fromBase64(data));
}

int checksum(const QString& s)
{
    int sum = 0;
    for (QChar c : s) {
        sum = (sum + c.unicode()) % 256;
    }
    return sum;
}
} // namespace

AttributionBanner::AttributionBanner(const QString& placement, QWidget* parent)
  : QWidget(parent)
{
    m_text = decode(TEXT_CHUNKS);
    const QString linkTemplate = decode(LINK_CHUNKS);
    if (checksum(m_text) != TEXT_CHECKSUM ||
        checksum(linkTemplate) != LINK_CHECKSUM) {
        m_text = QStringLiteral(
          "LICENSE VIOLATION: the Zshot attribution banner was modified. "
          "See NOTICE.");
        m_link = QStringLiteral("https://github.com/locphamnguyen/Zshot");
    } else {
        m_link = linkTemplate.arg(placement);
    }
    setFixedHeight(HEIGHT);
    setCursor(Qt::PointingHandCursor);
    setToolTip(m_link.section('?', 0, 0));
    setAccessibleName(m_text);
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        if (!m_hovered) {
            m_offset += SPEED;
            update();
        }
    });
    m_timer.start(TICK_MS);
    setVisible(!isLicensed());
}

bool AttributionBanner::isLicensed()
{
    QByteArray key = qgetenv("FRIENDS");
    if (key.isEmpty()) {
        key =
          ConfigHandler().value(QStringLiteral("friends")).toString().toUtf8();
    }
    if (key.size() < 8) {
        return false;
    }
    return QCryptographicHash::hash(key, QCryptographicHash::Sha256).toHex() ==
           QByteArray(LICENSE_HASH);
}

QString AttributionBanner::text() const
{
    return m_text;
}

QString AttributionBanner::link() const
{
    return m_link;
}

QSize AttributionBanner::sizeHint() const
{
    return { 400, HEIGHT };
}

void AttributionBanner::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QLinearGradient background(0, 0, width(), 0);
    background.setColorAt(0, QColor(0x0E, 0x44, 0x5A));
    background.setColorAt(1, QColor(0x06, 0x22, 0x2F));
    painter.fillRect(rect(), background);
    if (m_hovered) {
        painter.fillRect(rect(), QColor(0, 242, 255, 22));
    }

    QFont font = this->font();
    font.setPixelSize(13);
    font.setWeight(QFont::DemiBold);
    painter.setFont(font);
    painter.setPen(QColor(0x00, 0xF2, 0xFF));
    const int textWidth = painter.fontMetrics().horizontalAdvance(m_text);
    const int period = textWidth + GAP;
    const qreal shift = std::fmod(m_offset, static_cast<qreal>(period));
    // Repeat the text so the loop is seamless
    for (qreal x = width() - shift; x > -period; x -= period) {
        painter.drawText(QRectF(x, 0, textWidth + 4, height()),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         m_text);
    }
    for (qreal x = width() - shift + period; x < width(); x += period) {
        painter.drawText(QRectF(x, 0, textWidth + 4, height()),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         m_text);
    }
}

void AttributionBanner::enterEvent(QEnterEvent*)
{
    m_hovered = true;
    update();
}

void AttributionBanner::leaveEvent(QEvent*)
{
    m_hovered = false;
    update();
}

void AttributionBanner::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
        QDesktopServices::openUrl(QUrl(m_link));
    }
}
