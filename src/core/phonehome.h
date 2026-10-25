// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Nguyễn Tiến Lộc (Loc Nguyen)

#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

class QNetworkAccessManager;

/**
 * @brief Phone home of Zshot, same as zcrm-core (internal/modules/phonehome).
 *
 * The tray (daemon) process posts exactly { instanceId, version } to the
 * central page 30 s after start and then every 12 h, so the project knows how
 * many copies are in use and on which version. The answer carries an
 * optional announcement (new version, security notice) that the Zshot windows
 * show in a one-line strip.
 *
 * Silent failure: 5 s timeout, any error keeps the cached announcement. The
 * last valid announcement is kept in the config so every Zshot process (and
 * the next start) can show it.
 */
struct Announcement
{
    QString id;
    QString text;
    QString level; // info | warning | critical
    QString link;  // https only, may be empty
    QString linkLabel;
    bool dismissible = true;

    bool isValid() const { return !id.isEmpty(); }
};

class PhoneHome : public QObject
{
    Q_OBJECT
public:
    static PhoneHome* instance();

    // Start the ping loop (tray process only)
    void start();

    // Announcement to show, or an invalid one when there is none or the user
    // dismissed it
    Announcement current() const;
    void dismiss(const QString& id);

    // Pure parts, unit-testable: request body and answer validation
    static QByteArray pingPayload(const QString& instanceId,
                                  const QString& version);
    static Announcement parseResponse(const QByteArray& body);

signals:
    void announcementChanged();

private:
    explicit PhoneHome(QObject* parent = nullptr);
    void ping();
    QString instanceId();
    static QString version();

    QNetworkAccessManager* m_network = nullptr;
    QTimer m_timer;
    bool m_inFlight = false;
};
