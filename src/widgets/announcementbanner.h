// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Nguyễn Tiến Lộc (Loc Nguyen)

#pragma once

#include <QFrame>

class QLabel;
class QToolButton;

/**
 * @brief One-line strip showing the announcement of the central page (new
 * version, security notice), received by the phone home. Same as the
 * ServerAnnouncementBanner of ZCRM: plain text, optional https link, close
 * button unless the announcement is not dismissible. Hidden when there is no
 * announcement.
 */
class AnnouncementBanner : public QFrame
{
    Q_OBJECT
public:
    explicit AnnouncementBanner(QWidget* parent = nullptr);

private:
    void refresh();

    QString m_id;
    QLabel* m_icon;
    QLabel* m_text;
    QLabel* m_link;
    QToolButton* m_close;
};
