// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Nguyễn Tiến Lộc (Loc Nguyen)
//
// Port of zcrm-core internal/modules/phonehome (service.go, announcement.go).
// CONTRACT WITH THE CENTRAL PAGE (updater.zopen.vn, shared by the Zopen products):
//
//   send:    { instanceId, version }   — EXACTLY these two fields, nothing more
//   receive: { announcement: null | { id, text, level?, link?, linkLabel?,
//                                     dismissible? } }
//
// Everything received is outside data: each field is checked, lengths are
// cut, only https links are accepted. One wrong field means no announcement.
//
// There is no switch to turn the phone home off (same decision as ZCRM): it is
// part of the release, it is how the project counts the copies in use and
// announces updates. The address is pinned in PHONE_HOME_URL.

#include "phonehome.h"
#include "utils/confighandler.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUuid>

namespace {
const QString PHONE_HOME_URL =
  QStringLiteral("https://updater.zopen.vn/zshot/v1/ping");
constexpr int PING_INTERVAL_MS = 12 * 60 * 60 * 1000;
constexpr int BOOT_DELAY_MS = 30 * 1000;
constexpr int REQUEST_TIMEOUT_MS = 5000;
// A valid answer is under 3 KB; a broken central page must not fill the RAM
constexpr qint64 MAX_BODY = 64 * 1024;

constexpr int TEXT_MAX_LEN = 500;
constexpr int ID_MAX_LEN = 100;
constexpr int LINK_LABEL_MAX_LEN = 60;
constexpr int LINK_MAX_LEN = 2048;

const QString KEY_INSTANCE = QStringLiteral("instanceId");
const QString KEY_ANNOUNCEMENT = QStringLiteral("announcement");
const QString KEY_DISMISSED = QStringLiteral("announcementDismissed");

// Cut by characters (code points), not by UTF-16 units
QString truncate(const QString& s, int n)
{
    const QList<uint> codePoints = s.toUcs4();
    if (codePoints.size() <= n) {
        return s;
    }
    return QString::fromUcs4(
      reinterpret_cast<const char32_t*>(codePoints.constData()), n);
}

QJsonObject toJson(const Announcement& a)
{
    QJsonObject o{ { "id", a.id },
                   { "text", a.text },
                   { "level", a.level },
                   { "dismissible", a.dismissible } };
    o.insert("link", a.link.isEmpty() ? QJsonValue() : QJsonValue(a.link));
    o.insert("linkLabel",
             a.linkLabel.isEmpty() ? QJsonValue() : QJsonValue(a.linkLabel));
    return o;
}
} // namespace

PhoneHome* PhoneHome::instance()
{
    static auto* phoneHome = new PhoneHome();
    return phoneHome;
}

PhoneHome::PhoneHome(QObject* parent)
  : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        m_timer.setInterval(PING_INTERVAL_MS);
        ping();
    });
}

void PhoneHome::start()
{
    if (m_timer.isActive()) {
        return;
    }
    m_network = new QNetworkAccessManager(this);
    m_timer.start(BOOT_DELAY_MS);
}

QString PhoneHome::version()
{
    QString v = QStringLiteral(APP_VERSION);
    return v.startsWith('v') ? v.mid(1) : v;
}

QString PhoneHome::instanceId()
{
    ConfigHandler config;
    QString id = config.value(KEY_INSTANCE).toString();
    if (id.isEmpty()) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        config.setValue(KEY_INSTANCE, id);
    }
    return id;
}

QByteArray PhoneHome::pingPayload(const QString& instanceId,
                                  const QString& version)
{
    // Exactly two fields: adding one changes what every copy sends out
    return QJsonDocument(QJsonObject{ { "instanceId", instanceId },
                                      { "version", version } })
      .toJson(QJsonDocument::Compact);
}

Announcement PhoneHome::parseResponse(const QByteArray& body)
{
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    const QJsonValue raw = doc.object().value(QStringLiteral("announcement"));
    if (!raw.isObject()) {
        return {};
    }
    const QJsonObject a = raw.toObject();

    const QString rawId = a.value("id").toString();
    const QString id = rawId.trimmed();
    if (id.isEmpty() || rawId.toUcs4().size() > ID_MAX_LEN) {
        return {};
    }
    const QString text = a.value("text").toString().trimmed();
    if (text.isEmpty()) {
        return {};
    }

    QString level = a.value("level").toString();
    if (level != QLatin1String("warning") &&
        level != QLatin1String("critical")) {
        level = QStringLiteral("info");
    }

    QString link;
    const QJsonValue rawLink = a.value("link");
    if (!rawLink.isUndefined() && !rawLink.isNull()) {
        if (!rawLink.isString()) {
            return {};
        }
        link = rawLink.toString();
        if (!link.isEmpty()) {
            const QUrl url(link, QUrl::StrictMode);
            if (link.size() > LINK_MAX_LEN || !url.isValid() ||
                url.scheme() != QLatin1String("https") ||
                url.host().isEmpty()) {
                return {};
            }
            link = url.toString();
        }
    }

    QString linkLabel;
    const QJsonValue rawLabel = a.value("linkLabel");
    if (!rawLabel.isUndefined() && !rawLabel.isNull() &&
        rawLabel != QJsonValue(QString())) {
        if (!rawLabel.isString()) {
            return {};
        }
        linkLabel = truncate(rawLabel.toString().trimmed(), LINK_LABEL_MAX_LEN);
    }

    // No field means closable; with the field only `true` is closable
    bool dismissible = true;
    if (a.contains("dismissible")) {
        dismissible = a.value("dismissible") == QJsonValue(true);
    }

    return { id,         truncate(text, TEXT_MAX_LEN), level, link, linkLabel,
             dismissible };
}

void PhoneHome::ping()
{
    if (m_inFlight || m_network == nullptr) {
        return;
    }
    m_inFlight = true;
    QNetworkRequest request{ QUrl(PHONE_HOME_URL) };
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(REQUEST_TIMEOUT_MS);
    QNetworkReply* reply =
      m_network->post(request, pingPayload(instanceId(), version()));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_inFlight = false;
        const int status =
          reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status < 200 ||
            status >= 300) {
            // No network is a normal situation, keep the cached announcement
            return;
        }
        // A 2xx answer is the word of the central page: "no announcement" or
        // a malformed body removes the old one, that is how it is withdrawn
        const Announcement a = parseResponse(reply->read(MAX_BODY));
        const QString stored =
          a.isValid() ? QString::fromUtf8(QJsonDocument(toJson(a)).toJson(
                          QJsonDocument::Compact))
                      : QString();
        ConfigHandler config;
        if (config.value(KEY_ANNOUNCEMENT).toString() != stored) {
            config.setValue(KEY_ANNOUNCEMENT, stored);
            emit announcementChanged();
        }
    });
}

Announcement PhoneHome::current() const
{
    ConfigHandler config;
    const QByteArray stored =
      config.value(KEY_ANNOUNCEMENT).toString().toUtf8();
    if (stored.isEmpty()) {
        return {};
    }
    Announcement a =
      parseResponse(QByteArray(R"({"announcement":)") + stored + "}");
    if (a.isValid() && a.dismissible &&
        config.value(KEY_DISMISSED).toString() == a.id) {
        return {};
    }
    return a;
}

void PhoneHome::dismiss(const QString& id)
{
    ConfigHandler().setValue(KEY_DISMISSED, id);
    emit announcementChanged();
}
