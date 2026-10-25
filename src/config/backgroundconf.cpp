// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "backgroundconf.h"
#include "utils/backgroundrenderer.h"
#include "utils/confighandler.h"
#include "widgets/backgroundeditor.h"
#include "widgets/toggleswitch.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {
constexpr int SAVE_DELAY_MS = 300;
constexpr int EDITOR_WIDTH = 330;
constexpr QSize PREVIEW_SIZE(330, 214);

// A fake application window used to preview the background settings
QPixmap sampleCapture()
{
    QPixmap pixmap(480, 300);
    pixmap.fill(QColor(250, 250, 250));
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(0, 0, pixmap.width(), 36, QColor(225, 225, 230));
    const QColor dots[] = { "#ff5f57", "#febc2e", "#28c840" };
    for (int i = 0; i < 3; ++i) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(dots[i]);
        painter.drawEllipse(QPoint(20 + i * 20, 18), 6, 6);
    }
    painter.setBrush(QColor(23, 134, 190));
    painter.drawRoundedRect(30, 62, 190, 14, 7, 7);
    painter.setBrush(QColor(210, 210, 215));
    for (int i = 0; i < 5; ++i) {
        painter.drawRoundedRect(30, 100 + i * 34, i % 2 ? 300 : 420, 14, 7, 7);
    }
    return pixmap;
}
} // namespace

BackgroundConf::BackgroundConf(QWidget* parent)
  : QWidget(parent)
  , m_autoApply(new ToggleSwitch(this))
  , m_editor(new BackgroundEditor(this))
  , m_preview(new QLabel(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    // "Put every capture on a background" switch
    auto* autoCard = new QFrame(this);
    autoCard->setObjectName(QStringLiteral("zsAutoApply"));
    autoCard->setStyleSheet(QStringLiteral(
      "#zsAutoApply { background: #F2F8FC; border: 1px solid #CFE6F3; "
      "border-radius: 10px; } #zsAutoApply QLabel { background: transparent; "
      "}"));
    auto* autoLayout = new QHBoxLayout(autoCard);
    autoLayout->setContentsMargins(16, 12, 16, 12);
    auto* texts = new QVBoxLayout();
    texts->setSpacing(2);
    auto* title =
      new QLabel(tr("Put every capture on this background"), autoCard);
    title->setStyleSheet(QStringLiteral("font-weight: 600;"));
    auto* hint = new QLabel(
      tr("When off, use the Beautify button (G) of the capture toolbar or the "
         "--background option"),
      autoCard);
    hint->setProperty("zsRole", "muted");
    hint->setWordWrap(true);
    texts->addWidget(title);
    texts->addWidget(hint);
    autoLayout->addLayout(texts, 1);
    m_autoApply->setAccessibleName(title->text());
    autoLayout->addWidget(m_autoApply);
    layout->addWidget(autoCard);

    // Editor on the left, preview on the right
    auto* columns = new QHBoxLayout();
    columns->setSpacing(20);
    auto* scroll = new QScrollArea(this);
    auto* body = new QWidget(scroll);
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 8, 0);
    bodyLayout->addWidget(m_editor);
    scroll->setWidget(body);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFixedWidth(EDITOR_WIDTH + 18);
    columns->addWidget(scroll);

    auto* right = new QVBoxLayout();
    right->setSpacing(8);
    auto* previewTitle = new QLabel(tr("Preview"), this);
    previewTitle->setProperty("zsRole", "muted");
    m_preview->setFixedSize(PREVIEW_SIZE);
    m_preview->setAlignment(Qt::AlignCenter);
    right->addWidget(previewTitle);
    right->addWidget(m_preview);
    right->addStretch();
    columns->addLayout(right, 1);
    layout->addLayout(columns, 1);

    connect(m_autoApply, &ToggleSwitch::clicked, this, [](bool checked) {
        ConfigHandler().setBackgroundAutoApply(checked);
    });
    // Sliders emit many changes, only write the config once they settle
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(SAVE_DELAY_MS);
    connect(&m_saveTimer, &QTimer::timeout, this, [this]() {
        m_editor->settings().saveToConfig();
    });
    connect(m_editor, &BackgroundEditor::settingsChanged, this, [this]() {
        m_saveTimer.start();
        updatePreview();
    });

    updateComponents();
}

void BackgroundConf::updateComponents()
{
    // Don't overwrite changes that are still waiting to be saved
    if (m_saveTimer.isActive()) {
        return;
    }
    m_autoApply->setChecked(ConfigHandler().backgroundAutoApply());
    m_editor->setSettings(BackgroundSettings::fromConfig());
    updatePreview();
}

void BackgroundConf::updatePreview()
{
    static const QPixmap sample = sampleCapture();
    const qreal dpr = devicePixelRatioF();
    QPixmap preview = BackgroundRenderer::render(sample, m_editor->settings())
                        .scaled(PREVIEW_SIZE * dpr,
                                Qt::KeepAspectRatio,
                                Qt::SmoothTransformation);
    preview.setDevicePixelRatio(dpr);
    m_preview->setPixmap(preview);
}
