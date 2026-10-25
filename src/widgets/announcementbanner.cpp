// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Nguyễn Tiến Lộc (Loc Nguyen)

#include "announcementbanner.h"
#include "core/phonehome.h"
#include "utils/confighandler.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

namespace {
struct LevelStyle
{
    const char* background;
    const char* text;
    const char* border;
    const char* icon;
};

LevelStyle styleFor(const QString& level)
{
    if (level == QLatin1String("critical")) {
        return { "#FDE8E6", "#7A1C12", "#F2B8B2", "⚠" };
    }
    if (level == QLatin1String("warning")) {
        return { "#FFF4E0", "#6B4400", "#F5D9A0", "!" };
    }
    return { "#EEF4FB", "#0B3A4F", "#CBDDF0", "ⓘ" };
}
} // namespace

AnnouncementBanner::AnnouncementBanner(QWidget* parent)
  : QFrame(parent)
  , m_icon(new QLabel(this))
  , m_text(new QLabel(this))
  , m_link(new QLabel(this))
  , m_close(new QToolButton(this))
{
    setObjectName(QStringLiteral("zsAnnouncement"));
    setAccessibleName(tr("Announcement"));
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(20, 6, 12, 6);
    layout->setSpacing(10);
    m_icon->setStyleSheet(QStringLiteral("font-weight: 700;"));
    m_text->setTextFormat(Qt::PlainText);
    m_text->setWordWrap(true);
    m_link->setTextFormat(Qt::RichText);
    m_link->setOpenExternalLinks(true);
    m_close->setText(QStringLiteral("✕"));
    m_close->setAutoRaise(true);
    m_close->setCursor(Qt::PointingHandCursor);
    m_close->setToolTip(tr("Close this announcement"));
    m_close->setAccessibleName(tr("Close this announcement"));
    layout->addWidget(m_icon);
    layout->addWidget(m_text, 1);
    layout->addWidget(m_link);
    layout->addWidget(m_close);

    connect(m_close, &QToolButton::clicked, this, [this]() {
        PhoneHome::instance()->dismiss(m_id);
    });
    connect(PhoneHome::instance(),
            &PhoneHome::announcementChanged,
            this,
            &AnnouncementBanner::refresh);
    // The tray process may receive a new announcement while this window is
    // open in another process: it lands in the config file
    connect(ConfigHandler::getInstance(),
            &ConfigHandler::fileChanged,
            this,
            &AnnouncementBanner::refresh);
    refresh();
}

void AnnouncementBanner::refresh()
{
    const Announcement a = PhoneHome::instance()->current();
    setVisible(a.isValid());
    if (!a.isValid()) {
        m_id.clear();
        return;
    }
    m_id = a.id;
    const LevelStyle style = styleFor(a.level);
    setStyleSheet(QStringLiteral("#zsAnnouncement { background: %1; border: 0; "
                                 "border-bottom: 1px solid %3; }"
                                 "#zsAnnouncement QLabel, #zsAnnouncement "
                                 "QToolButton { color: %2; background: "
                                 "transparent; border: 0; font-size: 13px; }")
                    .arg(style.background, style.text, style.border));
    m_icon->setText(QString::fromUtf8(style.icon));
    m_text->setText(a.text);
    m_link->setVisible(!a.link.isEmpty());
    if (!a.link.isEmpty()) {
        const QString label =
          a.linkLabel.isEmpty() ? tr("View details") : a.linkLabel;
        m_link->setText(
          QStringLiteral(
            "<a href=\"%1\" style=\"color:%2; font-weight:700;\">%3</a>")
            .arg(a.link.toHtmlEscaped(), style.text, label.toHtmlEscaped()));
    }
    m_close->setVisible(a.dismissible);
}
