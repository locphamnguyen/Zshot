#include "trayicon.h"
#include "core/capturerequest.h"
#include "core/flameshot.h"
#include "core/flameshotdaemon.h"
#include "core/qguiappcurrentscreen.h"
#include "utils/confighandler.h"
#include "utils/globalvalues.h"
#include "utils/zshottheme.h"
#include "widgets/backgroundwindow.h"

#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QGuiApplication>
#include <QMenu>
#include <QScreen>
#include <QTimer>
#include <QUrl>
#include <QVersionNumber>

#if defined(Q_OS_MACOS)
#include <QOperatingSystemVersion>
#endif

TrayIcon::TrayIcon(QObject* parent)
  : QSystemTrayIcon(parent)
  , m_screenMenu(nullptr)
{
    initMenu();
    initScreenMenu();

    setToolTip(QStringLiteral("Zshot"));
#if defined(Q_OS_MACOS)
    // Because of the following issues on MacOS "Catalina":
    // https://bugreports.qt.io/browse/QTBUG-86393
    // https://developer.apple.com/forums/thread/126072
    auto currentMacOsVersion = QOperatingSystemVersion::current();
    if (currentMacOsVersion >= QOperatingSystemVersion::MacOSBigSur) {
        setContextMenu(m_menu);
    }
#else
    setContextMenu(m_menu);
#endif
    QIcon icon =
      QIcon::fromTheme("flameshot-tray", QIcon(GlobalValues::trayIconPath()));

#if defined(Q_OS_MACOS) && USE_MONOCHROME_ICON
    // Chỉ icon đơn sắc mới là mask; icon Zshot nhiều màu mà đặt mask thì thành
    // khối đặc.
    if (currentMacOsVersion >= QOperatingSystemVersion::MacOSBigSur) {
        icon.setIsMask(true);
    }
#endif

    setIcon(icon);

#if defined(Q_OS_MACOS)
    if (currentMacOsVersion < QOperatingSystemVersion::MacOSBigSur) {
        // Because of the following issues on MacOS "Catalina":
        // https://bugreports.qt.io/browse/QTBUG-86393
        // https://developer.apple.com/forums/thread/126072
        auto trayIconActivated = [this](QSystemTrayIcon::ActivationReason r) {
            if (m_menu->isVisible()) {
                m_menu->hide();
            } else {
                m_menu->popup(QCursor::pos());
            }
        };
        connect(this, &QSystemTrayIcon::activated, this, trayIconActivated);
    }
#else
    connect(this, &TrayIcon::activated, this, [this](ActivationReason r) {
        if (r == Trigger) {
            startGuiCapture();
        }
    });
#endif

#ifdef Q_OS_WIN
    // Ensure proper removal of tray icon when program quits on Windows.
    connect(qApp, &QCoreApplication::aboutToQuit, this, &TrayIcon::hide);
#endif

    show(); // TODO needed?

    if (ConfigHandler().showStartupLaunchMessage()) {
        showMessage(
          "Zshot",
          QObject::tr(
            "Hello, I'm here! Click icon in the tray to take a screenshot or "
            "click with a right button to see more options."),
          icon,
          3000);
    }

    connect(ConfigHandler::getInstance(),
            &ConfigHandler::fileChanged,
            this,
            [this]() { updateCaptureActionShortcut(); });
}

TrayIcon::~TrayIcon()
{
    delete m_menu;
}

#if !defined(DISABLE_UPDATE_CHECKER)
QAction* TrayIcon::appUpdates()
{
    return m_appUpdates;
}
#endif

void TrayIcon::initMenu()
{
    m_menu = new QMenu();

    m_captureAction = new QAction(tr("&Take Screenshot"), this);

    updateCaptureActionShortcut();

    connect(m_captureAction, &QAction::triggered, this, [this]() {
#if defined(Q_OS_MACOS)
        auto currentMacOsVersion = QOperatingSystemVersion::current();
        if (currentMacOsVersion >= QOperatingSystemVersion::MacOSBigSur) {
            startGuiCapture();
        } else {
            // It seems it is not relevant for MacOS BigSur (Wait 400 ms to hide
            // the QMenu)
            QTimer::singleShot(400, this, [this]() { startGuiCapture(); });
        }
#else
    // Wait 400 ms to hide the QMenu
    QTimer::singleShot(400, this, [this]() {
        startGuiCapture();
    });
#endif
    });
    m_launcherAction = new QAction(tr("&Open Launcher"), this);
    connect(m_launcherAction,
            &QAction::triggered,
            Flameshot::instance(),
            &Flameshot::launcher);
    auto* configAction = new QAction(tr("&Configuration"), this);
    connect(configAction,
            &QAction::triggered,
            Flameshot::instance(),
            &Flameshot::config);
    m_infoAction = new QAction(tr("&About"), this);
    connect(m_infoAction,
            &QAction::triggered,
            Flameshot::instance(),
            &Flameshot::info);

#if !defined(DISABLE_UPDATE_CHECKER)
    m_appUpdates = new QAction(tr("Check for updates"), this);
    connect(m_appUpdates,
            &QAction::triggered,
            FlameshotDaemon::instance(),
            &FlameshotDaemon::checkForUpdates);

    connect(FlameshotDaemon::instance(),
            &FlameshotDaemon::newVersionAvailable,
            this,
            [this](const QVersionNumber& version) {
                if (ConfigHandler().checkForUpdates()) {
                    QString newVersion =
                      tr("Download version %1").arg(version.toString());
                    m_appUpdates->setText(newVersion);
                    m_appUpdates->setVisible(true);

                    // hack to work around menu not updating when the text /
                    // visibility is modified Force menu refresh by removing and
                    // re-adding the action
                    m_menu->removeAction(m_appUpdates);
                    m_menu->insertAction(m_infoAction, m_appUpdates);
                }
            });
    updateCheckUpdatesMenuVisibility();
#endif

    QAction* quitAction = new QAction(tr("&Quit"), this);
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

#ifdef ENABLE_IMGUR
    // recent screenshots
    QAction* recentAction = new QAction(tr("&Latest Uploads"), this);
    connect(recentAction,
            &QAction::triggered,
            Flameshot::instance(),
            &Flameshot::history);
#endif
    auto* openSavePathAction = new QAction(tr("&Open Save Path"), this);
    connect(openSavePathAction,
            &QAction::triggered,
            Flameshot::instance(),
            &Flameshot::openSavePath);

    // Open the image of the clipboard in the background editor
    auto* beautifyAction =
      new QAction(tr("&Beautify the clipboard image"), this);
    connect(beautifyAction, &QAction::triggered, this, []() {
        const QPixmap image = QApplication::clipboard()->pixmap();
        if (image.isNull()) {
            FlameshotDaemon::instance()->sendTrayNotification(
              tr("The clipboard has no image to beautify."));
            return;
        }
        QRect geometry(QPoint(), image.deviceIndependentSize().toSize());
        geometry.moveCenter(QCursor::pos());
        auto* window = new BackgroundWindow(
          image, geometry, CaptureRequest(CaptureRequest::GRAPHICAL_MODE));
        window->show();
        window->activateWindow();
    });

    namespace ZC = ZshotTheme::Color;
    m_captureAction->setIcon(
      ZshotTheme::icon(QStringLiteral("square-outline"), ZC::ink3));
    m_launcherAction->setIcon(
      ZshotTheme::icon(QStringLiteral("apps"), ZC::ink3));
    beautifyAction->setIcon(
      ZshotTheme::icon(QStringLiteral("background"), ZC::ink3));
    openSavePathAction->setIcon(
      ZshotTheme::icon(QStringLiteral("filepath"), ZC::ink3));
    configAction->setIcon(ZshotTheme::icon(QStringLiteral("config"), ZC::ink3));
    quitAction->setIcon(ZshotTheme::icon(QStringLiteral("exit-to-app"),
                                         QColor(0xC0, 0x39, 0x2B)));
    m_menu->setStyleSheet(QStringLiteral(
      "QMenu { background: #FFFFFF; border: 1px solid #E7EAF0; padding: 6px; }"
      "QMenu::item { padding: 7px 28px 7px 10px; color: #141A24; "
      "border-radius: 6px; }"
      "QMenu::item:selected { background: #E4F1F8; color: #0B5880; }"
      "QMenu::icon { padding-left: 10px; }"
      "QMenu::separator { height: 1px; background: #E7EAF0; margin: 4px 6px; "
      "}"));

    m_menu->addAction(m_captureAction);
    m_menu->addAction(m_launcherAction);
    m_menu->addAction(beautifyAction);
    m_menu->addSeparator();
#ifdef ENABLE_IMGUR
    m_menu->addAction(recentAction);
#endif
    m_menu->addAction(openSavePathAction);
    m_menu->addSeparator();
    m_menu->addAction(configAction);
    m_menu->addSeparator();
#if !defined(DISABLE_UPDATE_CHECKER)
    m_menu->addAction(m_appUpdates);
#endif
    m_menu->addAction(m_infoAction);
    m_menu->addSeparator();
    m_menu->addAction(quitAction);
}

void TrayIcon::updateCaptureActionShortcut()
{
#if defined(Q_OS_MACOS)
    if (!m_captureAction) {
        return;
    }

    QString shortcut = ConfigHandler().shortcut("TAKE_SCREENSHOT");
    m_captureAction->setShortcut(QKeySequence(shortcut));
#endif
}

#if !defined(DISABLE_UPDATE_CHECKER)
void TrayIcon::updateCheckUpdatesMenuVisibility()
{
    if (m_appUpdates == nullptr) {
        return;
    }

    bool autoCheckEnabled = ConfigHandler().checkForUpdates();
    if (autoCheckEnabled) {
        // When auto-check is enabled, hide the menu item initially
        // It will be shown when a new version is available via a callback
        m_appUpdates->setVisible(false);
    } else {
        m_appUpdates->setVisible(true);
        m_appUpdates->setText(tr("Check for updates"));
    }
}
#endif

void TrayIcon::initScreenMenu()
{
#ifndef Q_OS_MACOS
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (screens.size() <= 1) {
        return;
    }

    m_screenMenu = new QMenu(tr("Select Screen"));

    QList<QAction*> actions = m_menu->actions();
    int index = actions.indexOf(m_launcherAction);
    if (index >= 0 && index + 1 < actions.size()) {
        m_menu->insertMenu(actions[index + 1], m_screenMenu);
    } else {
        m_menu->addMenu(m_screenMenu);
    }

    QScreen* currentScreen = QGuiAppCurrentScreen().currentScreen();
    int currentIndex = screens.indexOf(currentScreen);

    for (int i = 0; i < screens.size(); ++i) {
        QScreen* screen = screens[i];
        QRect geom = screen->geometry();
        QString screenDescription = tr("Monitor %1: %2 (%3x%4)")
                                      .arg(i + 1)
                                      .arg(screen->name())
                                      .arg(geom.width())
                                      .arg(geom.height());

        QAction* screenAction = m_screenMenu->addAction(screenDescription);
        connect(screenAction, &QAction::triggered, this, [this, i]() {
            // Wait and hide the menu
            QTimer::singleShot(
              100, this, [this, i]() { startGuiCaptureOnScreen(i); });
        });
    }
#endif
}

void TrayIcon::startGuiCapture()
{
    auto* widget = Flameshot::instance()->gui();
#if !defined(DISABLE_UPDATE_CHECKER)
    FlameshotDaemon::instance()->showUpdateNotificationIfAvailable(widget);
#endif
}

void TrayIcon::startGuiCaptureOnScreen(int screenIndex)
{
    CaptureRequest req(CaptureRequest::GRAPHICAL_MODE, 400);
    req.setSelectedMonitor(screenIndex);
    Flameshot::instance()->requestCapture(req);
}
