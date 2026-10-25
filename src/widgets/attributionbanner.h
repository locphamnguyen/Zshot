// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Nguyễn Tiến Lộc (Loc Nguyen)

#pragma once

#include <QTimer>
#include <QWidget>

/**
 * @brief Scrolling attribution banner of Zshot (same as the "contact-marquee"
 * of ZCRM CE): the maintainer's contact and services, linking to
 * locnguyendata.com with UTM parameters so its clicks can be measured.
 *
 * It is part of the Appropriate Legal Notices of Zshot, see the NOTICE file
 * (GPLv3 section 7(b) attribution term). It is hidden only with a commercial
 * license key (environment variable FRIENDS or the "friends" config option).
 */
class AttributionBanner : public QWidget
{
    Q_OBJECT
public:
    // `placement` names where the banner is shown
    // (utm_medium=<placement>_marquee)
    explicit AttributionBanner(const QString& placement,
                               QWidget* parent = nullptr);

    static bool isLicensed();
    QString text() const;
    QString link() const;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QString m_text;
    QString m_link;
    qreal m_offset = 0;
    bool m_hovered = false;
    QTimer m_timer;
};
