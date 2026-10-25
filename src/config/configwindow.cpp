// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "configwindow.h"
#include "config/backgroundconf.h"
#include "config/configresolver.h"
#include "config/filenameeditor.h"
#include "config/generalconf.h"
#include "config/shortcutswidget.h"
#include "config/visualseditor.h"
#include "utils/colorutils.h"
#include "utils/confighandler.h"
#include "utils/globalvalues.h"
#include "utils/pathinfo.h"
#include "utils/zshottheme.h"
#include "widgets/attributionbanner.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFileSystemWatcher>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QSizePolicy>
#include <QTabBar>
#include <QTextStream>
#include <QVBoxLayout>

// ConfigWindow contains the menus where you can configure the application

ConfigWindow::ConfigWindow(QWidget* parent)
  : QWidget(parent)
{
    // Zshot layout: side navigation on the left, the pages of a QTabWidget
    // (tab bar hidden) on the right
    setStyleSheet(ZshotTheme::styleSheet());
    resize(980, 680);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* nav = new QWidget(this);
    nav->setProperty("zsSurface", "nav");
    nav->setAttribute(Qt::WA_StyledBackground);
    nav->setFixedWidth(220);
    auto* navLayout = new QVBoxLayout(nav);
    navLayout->setContentsMargins(12, 16, 12, 14);
    navLayout->setSpacing(2);
    auto* brand = new QHBoxLayout();
    brand->setContentsMargins(6, 2, 6, 14);
    brand->setSpacing(10);
    auto* logo = new QLabel(nav);
    logo->setPixmap(QIcon(GlobalValues::iconPath()).pixmap(30, 30));
    auto* name = new QLabel(
      QStringLiteral("<span style=\"font-size:17px; font-weight:800; "
                     "color:#0F4A60;\">Z<span style=\"color:#1786BE;\">shot"
                     "</span></span><br><span style=\"font-size:11px; "
                     "color:#6B7488;\">%1</span>")
        .arg(tr("version %1").arg(QStringLiteral(APP_VERSION))),
      nav);
    brand->addWidget(logo);
    brand->addWidget(name, 1);
    navLayout->addLayout(brand);
    m_nav = new QListWidget(nav);
    m_nav->setProperty("zsNav", true);
    m_nav->setIconSize(QSize(18, 18));
    m_nav->setFrameShape(QFrame::NoFrame);
    navLayout->addWidget(m_nav, 1);
    auto* credit = new QLabel(tr("Based on Flameshot - GPLv3"), nav);
    credit->setProperty("zsRole", "muted");
    navLayout->addWidget(credit);
    layout->addWidget(nav);

    auto* content = new QWidget(this);
    content->setProperty("zsSurface", "content");
    content->setAttribute(Qt::WA_StyledBackground);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(28, 22, 28, 16);
    contentLayout->setSpacing(12);
    contentLayout->addWidget(
      new AttributionBanner(QStringLiteral("settings"), content));
    m_pageTitle = new QLabel(content);
    m_pageTitle->setProperty("zsRole", "heading");
    contentLayout->addWidget(m_pageTitle);
    // We wrap QTabWidget in a QWidget because of a Qt bug
    m_tabWidget = new QTabWidget(content);
    m_tabWidget->tabBar()->hide();
    m_tabWidget->setDocumentMode(true);
    contentLayout->addWidget(m_tabWidget, 1);
    layout->addWidget(content, 1);

    setAttribute(Qt::WA_DeleteOnClose);
    setWindowIcon(QIcon(GlobalValues::iconPath()));
    setWindowTitle(tr("Configuration"));

    connect(ConfigHandler::getInstance(),
            &ConfigHandler::fileChanged,
            this,
            &ConfigWindow::updateChildren);

    QColor background = this->palette().window().color();
    bool isDark = ColorUtils::colorIsDark(background);
    QString modifier =
      isDark ? PathInfo::whiteIconPath() : PathInfo::blackIconPath();

    // general
    m_generalConfig = new GeneralConf();
    m_generalConfigTab = new QWidget();
    auto* generalConfigLayout = new QVBoxLayout(m_generalConfigTab);
    m_generalConfigTab->setLayout(generalConfigLayout);
    generalConfigLayout->addWidget(m_generalConfig);
    m_tabWidget->addTab(
      m_generalConfigTab, QIcon(modifier + "config.svg"), tr("General"));

    // visuals
    m_visuals = new VisualsEditor();
    m_visualsTab = new QWidget();
    auto* visualsLayout = new QVBoxLayout(m_visualsTab);
    m_visualsTab->setLayout(visualsLayout);
    visualsLayout->addWidget(m_visuals);
    m_tabWidget->addTab(
      m_visualsTab, QIcon(modifier + "graphics.svg"), tr("Interface"));

    // filename
    m_filenameEditor = new FileNameEditor();
    m_filenameEditorTab = new QWidget();
    auto* filenameEditorLayout = new QVBoxLayout(m_filenameEditorTab);
    m_filenameEditorTab->setLayout(filenameEditorLayout);
    filenameEditorLayout->addWidget(m_filenameEditor);
    m_tabWidget->addTab(m_filenameEditorTab,
                        QIcon(modifier + "name_edition.svg"),
                        tr("Filename Editor"));

    // background
    m_backgroundConf = new BackgroundConf();
    m_backgroundTab = new QWidget();
    auto* backgroundLayout = new QVBoxLayout(m_backgroundTab);
    m_backgroundTab->setLayout(backgroundLayout);
    backgroundLayout->addWidget(m_backgroundConf);
    // after General and Interface, as in the Zshot design
    m_tabWidget->insertTab(
      2, m_backgroundTab, QIcon(modifier + "background.svg"), tr("Background"));

    // shortcuts
    m_shortcuts = new ShortcutsWidget();
    m_shortcutsTab = new QWidget();
    auto* shortcutsLayout = new QVBoxLayout(m_shortcutsTab);
    m_shortcutsTab->setLayout(shortcutsLayout);
    shortcutsLayout->addWidget(m_shortcuts);
    m_tabWidget->addTab(
      m_shortcutsTab, QIcon(modifier + "shortcut.svg"), tr("Shortcuts"));

    // side navigation: one entry per page, icons from the app's set
    const QStringList navIcons = { QStringLiteral("config"),
                                   QStringLiteral("graphics"),
                                   QStringLiteral("background"),
                                   QStringLiteral("name_edition"),
                                   QStringLiteral("shortcut") };
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        QIcon icon =
          ZshotTheme::icon(navIcons.value(i), ZshotTheme::Color::ink3, 18);
        icon.addPixmap(
          ZshotTheme::icon(navIcons.value(i), ZshotTheme::Color::brandDark, 18)
            .pixmap(18, 18),
          QIcon::Selected);
        m_nav->addItem(new QListWidgetItem(icon, m_tabWidget->tabText(i)));
    }
    connect(m_nav, &QListWidget::currentRowChanged, this, [this](int row) {
        m_tabWidget->setCurrentIndex(row);
        m_pageTitle->setText(m_tabWidget->tabText(row));
    });
    m_nav->setCurrentRow(0);

    // connect update sigslots
    connect(this,
            &ConfigWindow::updateChildren,
            m_filenameEditor,
            &FileNameEditor::updateComponents);
    connect(this,
            &ConfigWindow::updateChildren,
            m_visuals,
            &VisualsEditor::updateComponents);
    connect(this,
            &ConfigWindow::updateChildren,
            m_generalConfig,
            &GeneralConf::updateComponents);
    connect(this,
            &ConfigWindow::updateChildren,
            m_backgroundConf,
            &BackgroundConf::updateComponents);

    // Error indicator (this must come last)
    initErrorIndicator(m_visualsTab, m_visuals);
    initErrorIndicator(m_filenameEditorTab, m_filenameEditor);
    initErrorIndicator(m_generalConfigTab, m_generalConfig);
    initErrorIndicator(m_shortcutsTab, m_shortcuts);
}

void ConfigWindow::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) {
        close();
    }
}

void ConfigWindow::initErrorIndicator(QWidget* tab, QWidget* widget)
{
    auto* label = new QLabel(tab);
    auto* btnResolve = new QPushButton(tr("Resolve"), tab);
    auto* btnLayout = new QHBoxLayout();

    // Set up label
    label->setText(tr(
      "<b>Configuration file has errors. Resolve them before continuing.</b>"));
    label->setStyleSheet(QStringLiteral(":disabled { color: %1; }")
                           .arg(qApp->palette().color(QPalette::Text).name()));
    label->setVisible(ConfigHandler().hasError());

    // Set up "Show errors" button
    btnResolve->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    btnLayout->addWidget(btnResolve);
    btnResolve->setVisible(ConfigHandler().hasError());

    widget->setEnabled(!ConfigHandler().hasError());

    // Add label and button to the parent widget's layout
    auto* layout = static_cast<QBoxLayout*>(tab->layout());
    if (layout != nullptr) {
        layout->insertWidget(0, label);
        layout->insertLayout(1, btnLayout);
    } else {
        widget->layout()->addWidget(label);
        widget->layout()->addWidget(btnResolve);
    }

    // Sigslots
    connect(
      ConfigHandler::getInstance(), &ConfigHandler::error, widget, [=, this]() {
          widget->setEnabled(false);
          label->show();
          btnResolve->show();
      });
    connect(ConfigHandler::getInstance(),
            &ConfigHandler::errorResolved,
            widget,
            [=]() {
                widget->setEnabled(true);
                label->hide();
                btnResolve->hide();
            });
    connect(btnResolve, &QPushButton::clicked, this, [this]() {
        ConfigResolver().exec();
    });
}
