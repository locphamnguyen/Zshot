// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Zshot Contributors

#include "zshottheme.h"

#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include <QSvgRenderer>
#include <QVBoxLayout>

namespace ZshotTheme {

/* Dynamic properties understood by the stylesheet:
 *   zsCard="true"         QFrame: white card, 1px line, radius 12
 *   zsRole="title|muted|value|heading|nav-brand"   QLabel text styles
 *   zsChoice="true"       checkable QPushButton in a choice grid
 *   zsSegment="true"      QFrame holding segmented buttons
 *   zsSegmentItem="true"  checkable QPushButton inside a segment
 *   zsVariant="primary|outlined"   QPushButton
 *   zsSurface="canvas|side|nav"    QWidget backgrounds
 */
QString styleSheet()
{
    return QStringLiteral(R"(
QWidget { font-family: "Segoe UI", "Open Sans", sans-serif; font-size: 13px; color: #141A24; }
QWidget[zsSurface="canvas"] { background: #EEF1F6; }
QWidget[zsSurface="side"] { background: #F7F9FC; border-left: 1px solid #E7EAF0; }
QWidget[zsSurface="nav"] { background: #F7F9FC; border-right: 1px solid #E7EAF0; }
QWidget[zsSurface="footer"] { background: #FFFFFF; border-top: 1px solid #E7EAF0; }
QFrame[zsCard="true"] { background: #FFFFFF; border: 1px solid #E7EAF0; border-radius: 12px; }
QFrame[zsCard="true"] QLabel { border: 0; background: transparent; }
QLabel[zsRole="title"] { font-weight: 700; }
QLabel[zsRole="heading"] { font-size: 20px; font-weight: 700; }
QLabel[zsRole="muted"] { color: #6B7488; font-size: 12px; }
QLabel[zsRole="value"] { color: #141A24; font-family: "Roboto Mono", Consolas, monospace; font-size: 12px; }
QLabel[zsRole="pill"] { background: #FFFFFF; border: 1px solid #E7EAF0; border-radius: 13px; padding: 4px 12px; color: #475066; font-size: 12px; }
QFrame[zsCard="true"] QLabel[zsRole="drop"], QLabel[zsRole="drop"] { background: #F2F8FC; border: 1px dashed #9CCDE6; border-radius: 9px; color: #0B5880; padding: 12px; }
QWidget[zsSurface="content"] { background: #FFFFFF; }
QTabWidget::pane { border: 0; background: #FFFFFF; }
QTabWidget > QStackedWidget > QWidget { background: #FFFFFF; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: #D4D8DE; border-radius: 3px; min-height: 28px; }
QScrollBar::handle:vertical:hover { background: #A6A9AE; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: #D4D8DE; border-radius: 3px; min-width: 28px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QPushButton[zsChoice="true"] { background: #FFFFFF; border: 1px solid #E7EAF0; border-radius: 7px; min-height: 30px; padding: 0 6px; color: #475066; }
QPushButton[zsChoice="true"]:hover { border-color: #9CCDE6; }
QPushButton[zsChoice="true"]:checked { background: #E4F1F8; border-color: #1786BE; color: #0B5880; font-weight: 600; }
QFrame[zsSegment="true"] { background: #F1F4F9; border: 0; border-radius: 10px; }
QPushButton[zsSegmentItem="true"] { background: transparent; border: 1px solid transparent; border-radius: 7px; min-height: 30px; padding: 0 1px; color: #475066; font-size: 12px; }
QPushButton[zsSegmentItem="true"]:hover { color: #141A24; }
QPushButton[zsSegmentItem="true"]:checked { background: #E4F1F8; border-color: #1786BE; color: #0B5880; font-weight: 600; }
QPushButton[zsVariant="primary"] { background: #1786BE; color: #FFFFFF; border: 0; border-radius: 8px; min-height: 36px; padding: 0 16px; font-weight: 600; }
QPushButton[zsVariant="primary"]:hover { background: #0F6FA0; }
QPushButton[zsVariant="primary"]:pressed { background: #0B5880; }
QPushButton[zsVariant="outlined"] { background: #FFFFFF; color: #475066; border: 1px solid #E7EAF0; border-radius: 8px; min-height: 36px; padding: 0 14px; font-weight: 600; }
QPushButton[zsVariant="outlined"]:hover { background: #F7F9FC; border-color: #D4D8DE; }
QPushButton[zsVariant="link"] { background: transparent; border: 0; color: #1786BE; font-weight: 600; padding: 0 2px; }
QPushButton[zsVariant="link"]:hover { color: #0B5880; }
QToolButton[zsSwatch="true"] { border: 1px solid rgba(20,26,36,26); border-radius: 8px; }
QToolButton[zsSwatch="true"]:checked { border: 2px solid #1786BE; }
QSlider::groove:horizontal { height: 4px; background: #E7EAF0; border-radius: 2px; }
QSlider::sub-page:horizontal { background: #1786BE; border-radius: 2px; }
QSlider::handle:horizontal { background: #FFFFFF; border: 2px solid #1786BE; width: 12px; height: 12px; margin: -6px 0; border-radius: 8px; }
QScrollArea { border: 0; background: transparent; }
QScrollArea > QWidget > QWidget { background: transparent; }
QListWidget[zsGallery="true"] { border: 0; background: transparent; outline: 0; }
QListWidget[zsGallery="true"]::item { border: 2px solid transparent; border-radius: 9px; padding: 0; }
QListWidget[zsGallery="true"]::item:selected { border-color: #1786BE; background: transparent; }
QListWidget[zsNav="true"] { border: 0; background: transparent; outline: 0; }
QListWidget[zsNav="true"]::item { height: 36px; padding-left: 8px; border-radius: 8px; color: #475066; }
QListWidget[zsNav="true"]::item:hover { background: #EEF1F6; }
QListWidget[zsNav="true"]::item:selected { background: #E4F1F8; color: #0B5880; font-weight: 600; }
QLineEdit { border: 1px solid #A6A9AE; border-radius: 8px; min-height: 30px; padding: 0 8px; background: #FFFFFF; selection-background-color: #1786BE; }
QLineEdit:focus { border-color: #1786BE; }
QComboBox { border: 1px solid #D4D8DE; border-radius: 8px; min-height: 30px; padding: 0 8px; background: #FFFFFF; }
QComboBox:focus { border-color: #1786BE; }
QGroupBox { border: 1px solid #E7EAF0; border-radius: 12px; margin-top: 18px; padding: 12px; background: #FFFFFF; font-weight: 700; }
QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; }
QCheckBox { spacing: 8px; }
)");
}

QIcon icon(const QString& name, const QColor& color, int size)
{
    QFile file(QStringLiteral(":/img/material/black/%1.svg").arg(name));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    // The black icons draw with fill="#000000" or the default black fill
    QString svg = QString::fromUtf8(file.readAll());
    svg.replace(QStringLiteral("#000000"), color.name());
    svg.replace(QStringLiteral("currentColor"), color.name());
    static const QRegularExpression pathTag(QStringLiteral("<path\\b[^>]*>"));
    QString colored;
    qsizetype last = 0;
    for (const auto& match : pathTag.globalMatch(svg)) {
        QString tag = match.captured();
        if (!tag.contains(QLatin1String("fill="))) {
            tag.insert(5, QStringLiteral(" fill=\"%1\"").arg(color.name()));
        }
        colored += svg.mid(last, match.capturedStart() - last) + tag;
        last = match.capturedEnd();
    }
    colored += svg.mid(last);
    QSvgRenderer renderer(colored.toUtf8());
    QIcon result;
    for (int scale : { 1, 2 }) {
        QPixmap pixmap(size * scale, size * scale);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        renderer.render(&painter);
        painter.end();
        pixmap.setDevicePixelRatio(scale);
        result.addPixmap(pixmap);
    }
    return result;
}

QString kbd(const QString& key)
{
    return QStringLiteral(
             "<span style=\"font-family:Consolas,monospace; font-size:11px; "
             "color:#475066; background:#F1F4F9; border:1px solid #E7EAF0;\">"
             "&nbsp;%1&nbsp;</span>")
      .arg(key.toHtmlEscaped());
}

QWidget* card(const QString& title, QWidget* parent, QLabel** valueLabel)
{
    auto* frame = new QFrame(parent);
    frame->setProperty("zsCard", true);
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(14, 12, 14, 14);
    layout->setSpacing(10);
    auto* head = new QHBoxLayout();
    auto* titleLabel = new QLabel(title, frame);
    titleLabel->setProperty("zsRole", "title");
    head->addWidget(titleLabel);
    head->addStretch();
    if (valueLabel != nullptr) {
        *valueLabel = new QLabel(frame);
        (*valueLabel)->setProperty("zsRole", "muted");
        head->addWidget(*valueLabel);
    }
    layout->addLayout(head);
    return frame;
}

} // namespace ZshotTheme
