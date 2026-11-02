// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "backgroundwindow.h"
#include "core/flameshot.h"
#include "utils/globalvalues.h"
#include "utils/zshottheme.h"
#include "widgets/announcementbanner.h"
#include "widgets/backgroundeditor.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QVBoxLayout>

namespace {
constexpr int RENDER_DELAY_MS = 40;
constexpr int SAVE_DELAY_MS = 300;
constexpr int SIDE_WIDTH = 348;
constexpr QSize MIN_PREVIEW_SIZE(480, 320);
constexpr QSize WINDOW_SIZE(1280, 800);

QPushButton* actionButton(const QString& text,
                          const QString& icon,
                          bool primary,
                          QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setProperty("zsVariant", primary ? "primary" : "outlined");
    button->setIcon(ZshotTheme::icon(
      icon, primary ? QColor(Qt::white) : ZshotTheme::Color::ink2, 16));
    button->setCursor(Qt::PointingHandCursor);
    return button;
}
} // namespace

BackgroundWindow::BackgroundWindow(const QPixmap& capture,
                                   const QRect& selection,
                                   const CaptureRequest& request,
                                   QWidget* parent)
  : QWidget(parent)
  , m_capture(capture)
  , m_selection(selection)
  , m_request(request)
  , m_editor(new BackgroundEditor(this))
  , m_preview(new QLabel(this))
  , m_info(new QLabel(this))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(Qt::Window);
    setWindowIcon(QIcon(GlobalValues::iconPath()));
    setWindowTitle(tr("Zshot - Beautify capture"));
    setStyleSheet(ZshotTheme::styleSheet());
    resize(WINDOW_SIZE);

    // Preview on the left
    auto* canvas = new QWidget(this);
    canvas->setProperty("zsSurface", "canvas");
    canvas->setAttribute(Qt::WA_StyledBackground);
    auto* canvasLayout = new QVBoxLayout(canvas);
    canvasLayout->setContentsMargins(32, 32, 32, 20);
    canvasLayout->setSpacing(14);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumSize(MIN_PREVIEW_SIZE);
    // Keep the label from growing with the pixmap it shows
    m_preview->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_info->setProperty("zsRole", "pill");
    canvasLayout->addWidget(m_preview, 1);
    canvasLayout->addWidget(m_info, 0, Qt::AlignHCenter);

    // Settings on the right
    auto* side = new QWidget(this);
    side->setProperty("zsSurface", "side");
    side->setAttribute(Qt::WA_StyledBackground);
    side->setFixedWidth(SIDE_WIDTH);
    auto* sideLayout = new QVBoxLayout(side);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(0);
    auto* scroll = new QScrollArea(side);
    auto* scrollBody = new QWidget(scroll);
    auto* scrollLayout = new QVBoxLayout(scrollBody);
    scrollLayout->setContentsMargins(16, 16, 16, 16);
    scrollLayout->addWidget(m_editor);
    scroll->setWidget(scrollBody);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sideLayout->addWidget(scroll, 1);

    auto* footer = new QWidget(side);
    footer->setProperty("zsSurface", "footer");
    footer->setAttribute(Qt::WA_StyledBackground);
    auto* footerLayout = new QVBoxLayout(footer);
    footerLayout->setContentsMargins(16, 12, 16, 14);
    footerLayout->setSpacing(8);
    auto* buttons = new QHBoxLayout();
    buttons->setSpacing(8);
    auto* hint = new QLabel(footer);
    hint->setProperty("zsRole", "muted");
    using CR = CaptureRequest;
    const auto presetTasks =
      CR::COPY | CR::SAVE | CR::PIN | CR::UPLOAD | CR::PRINT_RAW;
    if (m_request.tasks() & presetTasks) {
        // Tasks were requested on the command line, just run them
        auto* accept =
          actionButton(tr("Done"), QStringLiteral("accept"), true, footer);
        connect(accept, &QPushButton::clicked, this, [this]() {
            exportWith(CR::NO_TASK);
        });
        accept->setDefault(true);
        buttons->addWidget(accept, 1);
        hint->setText(ZshotTheme::kbd(QStringLiteral("Esc")) +
                      QStringLiteral(" ") + tr("close"));
    } else {
        auto* pin =
          actionButton(tr("Pin"), QStringLiteral("pin"), false, footer);
        auto* save = actionButton(
          tr("Save"), QStringLiteral("content-save"), false, footer);
        auto* copy = actionButton(
          tr("Copy"), QStringLiteral("content-copy"), true, footer);
        connect(
          pin, &QPushButton::clicked, this, [this]() { exportWith(CR::PIN); });
        connect(save, &QPushButton::clicked, this, [this]() {
            exportWith(CR::SAVE);
        });
        connect(copy, &QPushButton::clicked, this, [this]() {
            exportWith(CR::COPY);
        });
        copy->setDefault(true);
        buttons->addWidget(pin);
        buttons->addWidget(save);
        buttons->addWidget(copy, 1);
        new QShortcut(
          QKeySequence::Copy, this, [this]() { exportWith(CR::COPY); });
        new QShortcut(
          QKeySequence::Save, this, [this]() { exportWith(CR::SAVE); });
        hint->setText(QStringLiteral("%1 %2 &nbsp; %3 %4 &nbsp; %5 %6")
                        .arg(ZshotTheme::kbd(QStringLiteral("Ctrl+C")),
                             tr("copy"),
                             ZshotTheme::kbd(QStringLiteral("Ctrl+S")),
                             tr("save"),
                             ZshotTheme::kbd(QStringLiteral("Esc")),
                             tr("close")));
    }
    new QShortcut(Qt::Key_Escape, this, [this]() { close(); });
    footerLayout->addLayout(buttons);
    footerLayout->addWidget(hint);
    sideLayout->addWidget(footer);

    auto* row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    row->addWidget(canvas, 1);
    row->addWidget(side);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(new AnnouncementBanner(this));
    layout->addLayout(row, 1);

    m_renderTimer.setSingleShot(true);
    m_renderTimer.setInterval(RENDER_DELAY_MS);
    connect(&m_renderTimer, &QTimer::timeout, this, &BackgroundWindow::render);
    // Remember the last used settings for the next captures
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(SAVE_DELAY_MS);
    connect(&m_saveTimer, &QTimer::timeout, this, [this]() {
        m_editor->settings().saveToConfig();
    });
    connect(m_editor, &BackgroundEditor::settingsChanged, this, [this]() {
        m_renderTimer.start();
        m_saveTimer.start();
    });

    m_editor->setSettings(BackgroundSettings::fromConfig());
    render();
}

void BackgroundWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // the preview label gets its new size once the layout ran
    QTimer::singleShot(0, this, &BackgroundWindow::updatePreview);
}

void BackgroundWindow::closeEvent(QCloseEvent* event)
{
    if (m_saveTimer.isActive()) {
        m_saveTimer.stop();
        m_editor->settings().saveToConfig();
    }
    if (!m_exported) {
        Flameshot::instance()->setExternalWidget(false);
        emit Flameshot::instance()->captureFailed();
    }
    QWidget::closeEvent(event);
}

void BackgroundWindow::render()
{
    m_result = BackgroundRenderer::render(m_capture, m_editor->settings());
    updatePreview();
}

void BackgroundWindow::updatePreview()
{
    if (m_result.isNull()) {
        return;
    }
    const qreal dpr = devicePixelRatioF();
    QPixmap preview = m_result.scaled(
      m_preview->size() * dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    preview.setDevicePixelRatio(dpr);
    m_preview->setPixmap(preview);
    const int percent = qRound(100.0 * preview.width() / m_result.width());
    m_info->setText(tr("%1 x %2 px - fit - %3%")
                      .arg(m_result.width())
                      .arg(m_result.height())
                      .arg(percent));
}

void BackgroundWindow::exportWith(CaptureRequest::ExportTask task)
{
    if (m_renderTimer.isActive()) {
        m_renderTimer.stop();
        render();
    }
    CaptureRequest request = m_request;
    request.setOpenBackgroundEditor(false);
    request.setBackgroundMode(CaptureRequest::BACKGROUND_NEVER);
    // The selection geometry was already printed before opening the editor
    request.removeTask(CaptureRequest::PRINT_GEOMETRY);
    if (task == CaptureRequest::SAVE) {
        // addTask() throws for SAVE: an uncaught exception in this slot
        // terminated the whole app. No path means "ask / use the save path",
        // as the Save button of the capture toolbar does.
        request.addSaveTask();
    } else if (task != CaptureRequest::NO_TASK) {
        request.addTask(task);
    }
    // Keep a pinned capture centered on the original selection
    QRect geometry = m_selection;
    geometry.setSize(m_result.deviceIndependentSize().toSize());
    geometry.moveCenter(m_selection.center());

    m_exported = true;
    Flameshot::instance()->setExternalWidget(false);
    Flameshot::instance()->exportCapture(m_result, geometry, request);
    close();
}
