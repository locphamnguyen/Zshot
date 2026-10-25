// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "backgroundeditor.h"
#include "utils/backgroundlibrary.h"
#include "utils/zshottheme.h"

#include <QButtonGroup>
#include <QColorDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSlider>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVBoxLayout>

namespace ZC = ZshotTheme::Color;

namespace {
constexpr QSize THUMBNAIL_SIZE(84, 55);
constexpr QSize GALLERY_CELL(88, 60);
constexpr int GALLERY_ROWS = 3;
constexpr int GALLERY_ROWS_EXPANDED = 7;
constexpr int SWATCH = 30;
constexpr int GRADIENT_COLUMNS = 8;
constexpr int KIND_ROLE = Qt::UserRole + 1;
constexpr int REMOVABLE_ROLE = Qt::UserRole + 2;
constexpr int KIND_IMAGE = 0;
constexpr int KIND_ADD = 1;

const QStringList RATIOS = { "auto", "1:1", "4:5",    "9:16",
                             "16:9", "2:1", "1.91:1", "3:4" };
const QList<QColor> SWATCH_COLORS = { "#FFFFFF", "#F1F4F9", "#141A24",
                                      "#1786BE", "#5F54FF", "#12B76A",
                                      "#F5A524", "#F04438" };

QRect badgeRect(const QRect& item)
{
    return { item.right() - 25, item.top() + 5, 20, 20 };
}

// Thumbnails with rounded corners, the brand border when selected, a check
// badge on the selected wallpaper, a remove badge on the user's images and a
// dashed "Add" tile
class GalleryDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem&,
                   const QModelIndex&) const override
    {
        return GALLERY_CELL;
    }

    void paint(QPainter* p,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(option.rect).adjusted(2, 2, -2, -2);
        QPainterPath shape;
        shape.addRoundedRect(r, 7, 7);
        if (index.data(KIND_ROLE).toInt() == KIND_ADD) {
            p->setPen(QPen(ZC::brand, 1, Qt::DashLine));
            p->setBrush(Qt::white);
            p->drawPath(shape);
            p->setPen(QPen(ZC::brand, 2, Qt::SolidLine, Qt::RoundCap));
            const QPointF c(r.center().x(), r.top() + 19);
            p->drawLine(c + QPointF(-7, 0), c + QPointF(7, 0));
            p->drawLine(c + QPointF(0, -7), c + QPointF(0, 7));
            QFont font = option.font;
            font.setPixelSize(12);
            font.setBold(true);
            p->setFont(font);
            p->setPen(ZC::brand);
            p->drawText(r.adjusted(0, 30, 0, 0),
                        Qt::AlignHCenter | Qt::AlignTop,
                        index.data(Qt::DisplayRole).toString());
            p->restore();
            return;
        }
        p->setClipPath(shape);
        const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
        p->drawPixmap(r.toRect(), icon.pixmap(r.size().toSize()));
        p->setClipping(false);
        const bool selected = option.state & QStyle::State_Selected;
        if (selected) {
            p->setPen(QPen(ZC::brand, 2));
            p->setBrush(Qt::NoBrush);
            p->drawRoundedRect(r.adjusted(1, 1, -1, -1), 6, 6);
        }
        const QRectF badge = badgeRect(option.rect);
        if (index.data(REMOVABLE_ROLE).toBool()) {
            p->setPen(Qt::NoPen);
            p->setBrush(QColor(20, 26, 36, 150));
            p->drawEllipse(badge);
            p->setPen(QPen(Qt::white, 1.6, Qt::SolidLine, Qt::RoundCap));
            const QPointF c = badge.center();
            p->drawLine(c + QPointF(-3.5, -3.5), c + QPointF(3.5, 3.5));
            p->drawLine(c + QPointF(3.5, -3.5), c + QPointF(-3.5, 3.5));
        } else if (selected) {
            p->setPen(Qt::NoPen);
            p->setBrush(ZC::brand);
            p->drawEllipse(badge.adjusted(1, 1, -1, -1));
            p->setPen(
              QPen(Qt::white, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            const QPointF c = badge.center();
            p->drawPolyline(QPolygonF(
              { c + QPointF(-4, 0), c + QPointF(-1, 3), c + QPointF(4, -3) }));
        }
        p->restore();
    }
};

QIcon gradientIcon(const GradientPreset& preset, int size)
{
    QPixmap pixmap(size * 2, size * 2);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addRoundedRect(pixmap.rect(), 14, 14);
    painter.setClipPath(clip);
    BackgroundRenderer::paintGradient(painter, pixmap.rect(), preset);
    painter.end();
    pixmap.setDevicePixelRatio(2);
    return QIcon(pixmap);
}

QIcon swatchIcon(const QColor& color, int size)
{
    QPixmap pixmap(size * 2, size * 2);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(20, 26, 36, 30), 2));
    painter.setBrush(color);
    painter.drawRoundedRect(
      QRectF(pixmap.rect()).adjusted(1, 1, -1, -1), 12, 12);
    painter.end();
    pixmap.setDevicePixelRatio(2);
    return QIcon(pixmap);
}

QToolButton* swatchButton(QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setCheckable(true);
    button->setProperty("zsSwatch", true);
    button->setFixedSize(SWATCH, SWATCH);
    button->setIconSize(QSize(SWATCH - 4, SWATCH - 4));
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

QPushButton* choiceButton(const QString& text,
                          QWidget* parent,
                          const char* property)
{
    auto* button = new QPushButton(text, parent);
    button->setCheckable(true);
    button->setProperty(property, true);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

QStringList imageFiles(const QMimeData* mime)
{
    QStringList files;
    const QList<QByteArray> formats = QImageReader::supportedImageFormats();
    for (const QUrl& url : mime->urls()) {
        const QString path = url.toLocalFile();
        if (formats.contains(QFileInfo(path).suffix().toLower().toLatin1())) {
            files << path;
        }
    }
    return files;
}
} // namespace

BackgroundEditor::BackgroundEditor(QWidget* parent)
  : QWidget(parent)
{
    setAcceptDrops(true);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->addWidget(buildRatioCard());
    layout->addWidget(buildBackgroundCard());
    layout->addWidget(buildAdjustCard());
    layout->addStretch();
    updateComponents();
}

QWidget* BackgroundEditor::buildRatioCard()
{
    QWidget* card = ZshotTheme::card(tr("Output ratio"), this, &m_ratioValue);
    auto* grid = new QGridLayout();
    grid->setSpacing(6);
    m_ratioGroup = new QButtonGroup(this);
    for (qsizetype i = 0; i < RATIOS.size(); ++i) {
        QString label = RATIOS[i] == QLatin1String("auto")
                          ? tr("Auto", "output ratio")
                          : RATIOS[i];
        auto* button = choiceButton(label, card, "zsChoice");
        m_ratioGroup->addButton(button, static_cast<int>(i));
        grid->addWidget(
          button, static_cast<int>(i) / 4, static_cast<int>(i) % 4);
    }
    static_cast<QVBoxLayout*>(card->layout())->addLayout(grid);
    connect(m_ratioGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_settings.aspectRatio = RATIOS[id];
        notify();
    });
    return card;
}

QWidget* BackgroundEditor::buildBackgroundCard()
{
    QWidget* card = ZshotTheme::card(tr("Background"), this);
    auto* layout = static_cast<QVBoxLayout*>(card->layout());

    auto* segment = new QFrame(card);
    segment->setProperty("zsSegment", true);
    auto* segmentLayout = new QHBoxLayout(segment);
    segmentLayout->setContentsMargins(4, 4, 4, 4);
    segmentLayout->setSpacing(4);
    m_tabGroup = new QButtonGroup(this);
    const QStringList tabs = {
        tr("Wallpaper"), tr("Gradient"), tr("Color"), tr("Image")
    };
    for (qsizetype i = 0; i < tabs.size(); ++i) {
        auto* button = choiceButton(tabs[i], segment, "zsSegmentItem");
        m_tabGroup->addButton(button, static_cast<int>(i));
        segmentLayout->addWidget(button);
    }
    layout->addWidget(segment);
    connect(m_tabGroup, &QButtonGroup::idClicked, this, [this](int id) {
        selectTab(static_cast<Tab>(id));
    });

    m_pages = new QStackedWidget(card);
    m_pages->addWidget(buildWallpaperPage());
    m_pages->addWidget(buildGradientPage());
    m_pages->addWidget(buildColorPage());
    m_pages->addWidget(buildImagesPage());
    layout->addWidget(m_pages);
    return card;
}

QListWidget* BackgroundEditor::createGallery(QWidget* parent)
{
    auto* gallery = new QListWidget(parent);
    gallery->setProperty("zsGallery", true);
    gallery->setItemDelegate(new GalleryDelegate(gallery));
    gallery->setViewMode(QListView::IconMode);
    gallery->setIconSize(THUMBNAIL_SIZE);
    gallery->setGridSize(GALLERY_CELL);
    gallery->setResizeMode(QListView::Adjust);
    gallery->setMovement(QListView::Static);
    gallery->setUniformItemSizes(true);
    gallery->setSelectionMode(QAbstractItemView::SingleSelection);
    gallery->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    gallery->setFrameShape(QFrame::NoFrame);
    gallery->setFixedHeight(GALLERY_CELL.height() * GALLERY_ROWS);
    gallery->viewport()->installEventFilter(this);
    connect(gallery,
            &QListWidget::currentItemChanged,
            this,
            [this](QListWidgetItem* item) {
                if (item == nullptr || m_updating ||
                    item->data(KIND_ROLE).toInt() != KIND_IMAGE) {
                    return;
                }
                m_settings.type = BackgroundSettings::IMAGE;
                m_settings.imagePath = item->data(Qt::UserRole).toString();
                notify();
            });
    return gallery;
}

QWidget* BackgroundEditor::buildWallpaperPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    m_wallpapers = createGallery(page);
    layout->addWidget(m_wallpapers);
    auto* footer = new QHBoxLayout();
    m_wallpaperCredit = new QLabel(page);
    m_wallpaperCredit->setProperty("zsRole", "muted");
    m_wallpaperCredit->setWordWrap(true);
    footer->addWidget(m_wallpaperCredit, 1);
    m_showAllWallpapers = new QPushButton(tr("Show all"), page);
    m_showAllWallpapers->setProperty("zsVariant", "link");
    m_showAllWallpapers->setCursor(Qt::PointingHandCursor);
    footer->addWidget(m_showAllWallpapers);
    layout->addLayout(footer);
    connect(m_showAllWallpapers, &QPushButton::clicked, this, [this]() {
        m_wallpapersExpanded = !m_wallpapersExpanded;
        m_wallpapers->setFixedHeight(
          GALLERY_CELL.height() *
          (m_wallpapersExpanded ? GALLERY_ROWS_EXPANDED : GALLERY_ROWS));
        m_showAllWallpapers->setText(m_wallpapersExpanded ? tr("Show less")
                                                          : tr("Show all"));
    });
    return page;
}

QWidget* BackgroundEditor::buildGradientPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    auto* grid = new QGridLayout();
    grid->setSpacing(6);
    m_gradientGroup = new QButtonGroup(this);
    const auto& presets = BackgroundRenderer::gradientPresets();
    for (qsizetype i = 0; i < presets.size(); ++i) {
        auto* button = swatchButton(page);
        button->setIcon(gradientIcon(presets[i], SWATCH - 4));
        button->setToolTip(presets[i].name);
        m_gradientGroup->addButton(button, static_cast<int>(i));
        grid->addWidget(button,
                        static_cast<int>(i) / GRADIENT_COLUMNS,
                        static_cast<int>(i) % GRADIENT_COLUMNS);
    }
    layout->addLayout(grid);
    m_gradientLabel = new QLabel(page);
    m_gradientLabel->setProperty("zsRole", "muted");
    layout->addWidget(m_gradientLabel);
    connect(m_gradientGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_settings.type = BackgroundSettings::GRADIENT;
        m_settings.gradientId = BackgroundRenderer::gradientPresets()[id].id;
        notify();
    });
    return page;
}

QWidget* BackgroundEditor::buildColorPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto* specials = new QHBoxLayout();
    specials->setSpacing(6);
    m_autoColor = choiceButton(tr("Auto"), page, "zsChoice");
    m_autoColor->setIcon(
      ZshotTheme::icon(QStringLiteral("colorize"), ZC::ink2, 16));
    m_autoColor->setToolTip(tr("Average color of the capture edges"));
    m_transparent = choiceButton(tr("Transparent"), page, "zsChoice");
    m_transparent->setIcon(
      ZshotTheme::icon(QStringLiteral("square-outline"), ZC::ink2, 16));
    specials->addWidget(m_autoColor);
    specials->addWidget(m_transparent);
    layout->addLayout(specials);
    connect(m_autoColor, &QPushButton::clicked, this, [this]() {
        m_settings.type = BackgroundSettings::AUTO;
        notify();
    });
    connect(m_transparent, &QPushButton::clicked, this, [this]() {
        m_settings.type = BackgroundSettings::TRANSPARENT;
        notify();
    });

    auto* grid = new QGridLayout();
    grid->setSpacing(6);
    for (qsizetype i = 0; i < SWATCH_COLORS.size(); ++i) {
        auto* button = swatchButton(page);
        const QColor color = SWATCH_COLORS[i];
        button->setIcon(swatchIcon(color, SWATCH - 4));
        button->setToolTip(color.name().toUpper());
        grid->addWidget(button, 0, static_cast<int>(i));
        m_swatches.append({ button, color });
        connect(button, &QToolButton::clicked, this, [this, color]() {
            m_settings.type = BackgroundSettings::SOLID;
            m_settings.color = color;
            notify();
        });
    }
    layout->addLayout(grid);

    auto* custom = new QFrame(page);
    custom->setProperty("zsCard", true);
    auto* customLayout = new QHBoxLayout(custom);
    customLayout->setContentsMargins(10, 4, 6, 4);
    m_colorChip = new QLabel(custom);
    m_colorChip->setFixedSize(18, 18);
    m_colorHex = new QLabel(custom);
    m_colorHex->setProperty("zsRole", "value");
    auto* pick = new QPushButton(tr("Choose color..."), custom);
    pick->setProperty("zsVariant", "link");
    pick->setCursor(Qt::PointingHandCursor);
    customLayout->addWidget(m_colorChip);
    customLayout->addWidget(m_colorHex, 1);
    customLayout->addWidget(pick);
    layout->addWidget(custom);
    connect(pick, &QPushButton::clicked, this, [this]() {
        QColor color =
          QColorDialog::getColor(m_settings.color, this, tr("Choose a color"));
        if (color.isValid()) {
            m_settings.type = BackgroundSettings::SOLID;
            m_settings.color = color;
            notify();
        }
    });
    return page;
}

QWidget* BackgroundEditor::buildImagesPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    m_myImages = createGallery(page);
    m_myImages->setFixedHeight(GALLERY_CELL.height() * 2);
    layout->addWidget(m_myImages);
    auto* drop = new QLabel(page);
    drop->setProperty("zsRole", "drop");
    drop->setAlignment(Qt::AlignCenter);
    drop->setWordWrap(true);
    drop->setText(
      QStringLiteral(
        "<b>%1 <a href=\"#\" style=\"color:#1786BE;\">%2</a></b><br>"
        "<span style=\"color:#6B7488; font-size:12px;\">%3</span>")
        .arg(tr("Drop images here or"),
             tr("choose from the computer"),
             tr("Images are kept in the library for next time")));
    connect(drop, &QLabel::linkActivated, this, [this]() { chooseImages(); });
    layout->addWidget(drop);
    return page;
}

QWidget* BackgroundEditor::buildAdjustCard()
{
    QWidget* card = ZshotTheme::card(tr("Adjustments"), this);
    auto* layout = static_cast<QVBoxLayout*>(card->layout());
    addSlider(layout,
              tr("Padding"),
              50,
              QStringLiteral("%"),
              &BackgroundSettings::paddingPercent);
    addSlider(layout,
              tr("Corner radius"),
              100,
              QStringLiteral(" px"),
              &BackgroundSettings::cornerRadius);
    addSlider(layout,
              tr("Shadow"),
              100,
              QStringLiteral(" px"),
              &BackgroundSettings::shadow);
    m_blurRow = addSlider(layout,
                          tr("Background blur"),
                          100,
                          QStringLiteral(" px"),
                          &BackgroundSettings::blur);
    QHBoxLayout* borderRow = nullptr;
    addSlider(layout,
              tr("Border"),
              50,
              QStringLiteral(" px"),
              &BackgroundSettings::borderWidth,
              &borderRow);
    borderRow->addWidget(
      createColorButton(&BackgroundSettings::borderColor), 0, Qt::AlignBottom);
    return card;
}

QWidget* BackgroundEditor::addSlider(QVBoxLayout* layout,
                                     const QString& label,
                                     int max,
                                     const QString& suffix,
                                     int BackgroundSettings::*member,
                                     QHBoxLayout** rowLayout)
{
    auto* row = new QWidget(this);
    auto* rowBox = new QHBoxLayout(row);
    rowBox->setContentsMargins(0, 0, 0, 0);
    rowBox->setSpacing(10);
    auto* column = new QVBoxLayout();
    column->setSpacing(4);
    auto* head = new QHBoxLayout();
    auto* name = new QLabel(label, row);
    name->setStyleSheet(QStringLiteral("color: #475066;"));
    auto* value = new QLabel(QStringLiteral("0") + suffix, row);
    value->setProperty("zsRole", "value");
    head->addWidget(name);
    head->addStretch();
    head->addWidget(value);
    auto* slider = new QSlider(Qt::Horizontal, row);
    slider->setRange(0, max);
    slider->setAccessibleName(label);
    column->addLayout(head);
    column->addWidget(slider);
    rowBox->addLayout(column, 1);
    layout->addWidget(row);
    m_sliders.append({ slider, member });
    connect(slider, &QSlider::valueChanged, this, [=, this](int v) {
        value->setText(QString::number(v) + suffix);
        m_settings.*member = v;
        notify();
    });
    if (rowLayout != nullptr) {
        *rowLayout = rowBox;
    }
    return row;
}

QToolButton* BackgroundEditor::createColorButton(
  QColor BackgroundSettings::*member)
{
    auto* button = swatchButton(this);
    button->setCheckable(false);
    button->setToolTip(tr("Border color"));
    m_colorButtons.append({ button, member });
    connect(button, &QToolButton::clicked, this, [=, this]() {
        QColor color = QColorDialog::getColor(
          m_settings.*member, this, tr("Choose a color"));
        if (color.isValid()) {
            m_settings.*member = color;
            notify();
        }
    });
    return button;
}

BackgroundSettings BackgroundEditor::settings() const
{
    return m_settings;
}

void BackgroundEditor::setSettings(const BackgroundSettings& settings)
{
    m_settings = settings;
    m_tab = tabFor(settings);
    updateComponents();
}

BackgroundEditor::Tab BackgroundEditor::tabFor(
  const BackgroundSettings& s) const
{
    switch (s.type) {
        case BackgroundSettings::GRADIENT:
            return GRADIENT;
        case BackgroundSettings::IMAGE:
            return s.imagePath.isEmpty() ||
                       BackgroundLibrary::isBuiltIn(s.imagePath)
                     ? WALLPAPER
                     : MY_IMAGES;
        default:
            return COLOR;
    }
}

void BackgroundEditor::selectTab(Tab tab)
{
    m_tab = tab;
    switch (tab) {
        case WALLPAPER: {
            m_settings.type = BackgroundSettings::IMAGE;
            if (!BackgroundLibrary::isBuiltIn(m_settings.imagePath)) {
                QString path = m_lastWallpaper;
                if (path.isEmpty()) {
                    for (const auto& image : BackgroundLibrary::images()) {
                        if (image.builtIn) {
                            path = image.path;
                            break;
                        }
                    }
                }
                m_settings.imagePath = path;
            }
            break;
        }
        case GRADIENT:
            m_settings.type = BackgroundSettings::GRADIENT;
            break;
        case COLOR:
            if (m_settings.type != BackgroundSettings::SOLID &&
                m_settings.type != BackgroundSettings::AUTO &&
                m_settings.type != BackgroundSettings::TRANSPARENT) {
                m_settings.type = BackgroundSettings::SOLID;
            }
            break;
        case MY_IMAGES: {
            m_settings.type = BackgroundSettings::IMAGE;
            if (m_settings.imagePath.isEmpty() ||
                BackgroundLibrary::isBuiltIn(m_settings.imagePath)) {
                QString path = m_lastMyImage;
                if (path.isEmpty() || !QFileInfo::exists(path)) {
                    path.clear();
                    for (const auto& image : BackgroundLibrary::images()) {
                        if (!image.builtIn) {
                            path = image.path;
                            break;
                        }
                    }
                }
                // No image of the user yet: the renderer falls back to the
                // gradient
                m_settings.imagePath = path;
            }
            break;
        }
    }
    notify();
}

void BackgroundEditor::chooseImages()
{
    QStringList patterns;
    for (const QByteArray& format : QImageReader::supportedImageFormats()) {
        patterns << QStringLiteral("*.") + QString::fromLatin1(format);
    }
    importImages(QFileDialog::getOpenFileNames(this,
                                               tr("Add background images"),
                                               QString(),
                                               tr("Images") + " (" +
                                                 patterns.join(' ') + ")"));
}

void BackgroundEditor::importImages(const QStringList& files)
{
    QString imported;
    for (const QString& file : files) {
        QString copy = BackgroundLibrary::importImage(file);
        if (copy.isEmpty()) {
            QMessageBox::warning(
              this,
              tr("Background"),
              tr("Unable to add %1 to the image library").arg(file));
        } else {
            imported = copy;
        }
    }
    if (!imported.isEmpty()) {
        m_tab = MY_IMAGES;
        m_settings.type = BackgroundSettings::IMAGE;
        m_settings.imagePath = imported;
        refreshGalleries();
        notify();
    }
}

void BackgroundEditor::dragEnterEvent(QDragEnterEvent* event)
{
    if (!imageFiles(event->mimeData()).isEmpty()) {
        event->acceptProposedAction();
    }
}

void BackgroundEditor::dropEvent(QDropEvent* event)
{
    importImages(imageFiles(event->mimeData()));
    event->acceptProposedAction();
}

bool BackgroundEditor::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() != QEvent::MouseButtonPress ||
        watched != m_myImages->viewport()) {
        return QWidget::eventFilter(watched, event);
    }
    const QPoint pos = static_cast<QMouseEvent*>(event)->position().toPoint();
    const QModelIndex index = m_myImages->indexAt(pos);
    if (!index.isValid()) {
        return false;
    }
    if (index.data(KIND_ROLE).toInt() == KIND_ADD) {
        chooseImages();
        return true;
    }
    if (badgeRect(m_myImages->visualRect(index)).contains(pos)) {
        const QString path = index.data(Qt::UserRole).toString();
        if (BackgroundLibrary::removeImage(path)) {
            if (m_settings.imagePath == path) {
                m_settings.imagePath.clear();
            }
            if (m_lastMyImage == path) {
                m_lastMyImage.clear();
            }
            refreshGalleries();
            selectTab(MY_IMAGES);
        }
        return true;
    }
    return false;
}

void BackgroundEditor::refreshGalleries()
{
    m_galleriesLoaded = true;
    const bool updating = m_updating;
    m_updating = true;
    m_wallpapers->clear();
    m_myImages->clear();
    for (const auto& image : BackgroundLibrary::images()) {
        auto* item = new QListWidgetItem(
          BackgroundLibrary::thumbnail(image.path, THUMBNAIL_SIZE), QString());
        item->setData(Qt::UserRole, image.path);
        item->setData(KIND_ROLE, KIND_IMAGE);
        item->setData(REMOVABLE_ROLE, !image.builtIn);
        item->setToolTip(image.name);
        (image.builtIn ? m_wallpapers : m_myImages)->addItem(item);
    }
    auto* add = new QListWidgetItem(tr("Add"));
    add->setData(KIND_ROLE, KIND_ADD);
    add->setToolTip(tr("Add images from the computer"));
    add->setFlags(Qt::ItemIsEnabled);
    m_myImages->addItem(add);
    m_updating = updating;
}

void BackgroundEditor::updateComponents()
{
    m_updating = true;
    const auto& s = m_settings;

    const qsizetype ratio = RATIOS.indexOf(s.aspectRatio);
    if (QAbstractButton* button =
          m_ratioGroup->button(static_cast<int>(ratio))) {
        button->setChecked(true);
    }
    m_ratioValue->setText(s.aspectRatio == QLatin1String("auto")
                            ? tr("Auto", "output ratio")
                            : s.aspectRatio);

    m_tabGroup->button(m_tab)->setChecked(true);
    m_pages->setCurrentIndex(m_tab);
    for (int i = 0; i < m_pages->count(); ++i) {
        m_pages->widget(i)->setSizePolicy(QSizePolicy::Preferred,
                                          i == m_tab ? QSizePolicy::Maximum
                                                     : QSizePolicy::Ignored);
    }
    m_pages->updateGeometry();
    if ((m_tab == WALLPAPER || m_tab == MY_IMAGES) && !m_galleriesLoaded) {
        // Thumbnails are only decoded once a gallery is shown
        refreshGalleries();
    }

    // Wallpapers / my images selection
    QListWidgetItem* selectedWallpaper = nullptr;
    QListWidgetItem* selectedMine = nullptr;
    const bool image = s.type == BackgroundSettings::IMAGE;
    for (QListWidget* gallery : { m_wallpapers, m_myImages }) {
        for (int i = 0; i < gallery->count(); ++i) {
            QListWidgetItem* item = gallery->item(i);
            if (image && item->data(Qt::UserRole).toString() == s.imagePath) {
                (gallery == m_wallpapers ? selectedWallpaper : selectedMine) =
                  item;
            }
        }
    }
    m_wallpapers->setCurrentItem(selectedWallpaper);
    if (selectedWallpaper == nullptr) {
        m_wallpapers->clearSelection();
    } else {
        m_lastWallpaper = s.imagePath;
        m_wallpaperCredit->setText(tr("%1 wallpapers · %2")
                                     .arg(m_wallpapers->count())
                                     .arg(selectedWallpaper->toolTip()));
    }
    if (selectedWallpaper == nullptr) {
        m_wallpaperCredit->setText(
          tr("%1 wallpapers").arg(m_wallpapers->count()));
    }
    m_myImages->setCurrentItem(selectedMine);
    if (selectedMine == nullptr) {
        m_myImages->clearSelection();
    } else {
        m_lastMyImage = s.imagePath;
    }

    // Gradients
    const auto& presets = BackgroundRenderer::gradientPresets();
    const GradientPreset& gradient =
      BackgroundRenderer::gradientPreset(s.gradientId);
    for (qsizetype i = 0; i < presets.size(); ++i) {
        m_gradientGroup->button(static_cast<int>(i))
          ->setChecked(s.type == BackgroundSettings::GRADIENT &&
                       presets[i].id == gradient.id);
    }
    m_gradientLabel->setText(
      QStringLiteral("%1 · %2°").arg(gradient.name).arg(gradient.angle));

    // Colors
    m_autoColor->setChecked(s.type == BackgroundSettings::AUTO);
    m_transparent->setChecked(s.type == BackgroundSettings::TRANSPARENT);
    for (const auto& [button, color] : m_swatches) {
        button->setChecked(s.type == BackgroundSettings::SOLID &&
                           color == s.color);
    }
    m_colorChip->setStyleSheet(
      QStringLiteral("background: %1; border: 1px solid rgba(20,26,36,30); "
                     "border-radius: 5px;")
        .arg(s.color.name()));
    m_colorHex->setText(s.color.name().toUpper());

    // Adjustments
    for (const auto& [slider, member] : m_sliders) {
        slider->setValue(s.*member);
    }
    for (const auto& [button, member] : m_colorButtons) {
        button->setIcon(swatchIcon(s.*member, SWATCH - 4));
    }
    m_blurRow->setVisible(m_tab == WALLPAPER || m_tab == MY_IMAGES);
    m_updating = false;
}

void BackgroundEditor::notify()
{
    if (m_updating) {
        return;
    }
    updateComponents();
    emit settingsChanged(m_settings);
}
