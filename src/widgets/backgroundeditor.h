// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#pragma once

#include "utils/backgroundrenderer.h"

#include <QList>
#include <QPair>
#include <QWidget>

class QButtonGroup;
class QHBoxLayout;
class QLabel;
class QListWidget;
class QPushButton;
class QSlider;
class QStackedWidget;
class QToolButton;
class QVBoxLayout;

/**
 * @brief Controls to edit BackgroundSettings, laid out as the Zshot cards:
 * output ratio, background (wallpaper / gradient / color / my images) and
 * adjustments. Emits settingsChanged on every modification, it doesn't write
 * the config itself.
 */
class BackgroundEditor : public QWidget
{
    Q_OBJECT
public:
    explicit BackgroundEditor(QWidget* parent = nullptr);

    BackgroundSettings settings() const;
    void setSettings(const BackgroundSettings& settings);

signals:
    void settingsChanged(const BackgroundSettings& settings);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    enum Tab
    {
        WALLPAPER = 0,
        GRADIENT = 1,
        COLOR = 2,
        MY_IMAGES = 3,
    };

    QWidget* buildRatioCard();
    QWidget* buildBackgroundCard();
    QWidget* buildAdjustCard();
    QWidget* buildWallpaperPage();
    QWidget* buildGradientPage();
    QWidget* buildColorPage();
    QWidget* buildImagesPage();
    QListWidget* createGallery(QWidget* parent);
    QWidget* addSlider(QVBoxLayout* layout,
                       const QString& label,
                       int max,
                       const QString& suffix,
                       int BackgroundSettings::*member,
                       QHBoxLayout** rowLayout = nullptr);
    QToolButton* createColorButton(QColor BackgroundSettings::*member);
    void importImages(const QStringList& files);
    void chooseImages();
    void selectTab(Tab tab);
    Tab tabFor(const BackgroundSettings& settings) const;
    void refreshGalleries();
    void updateComponents();
    void notify();

    BackgroundSettings m_settings;
    bool m_updating = false;
    bool m_galleriesLoaded = false;
    bool m_wallpapersExpanded = false;
    Tab m_tab = GRADIENT;
    QString m_lastWallpaper;
    QString m_lastMyImage;

    QButtonGroup* m_ratioGroup;
    QLabel* m_ratioValue;
    QButtonGroup* m_tabGroup;
    QStackedWidget* m_pages;
    QListWidget* m_wallpapers;
    QLabel* m_wallpaperCredit;
    QPushButton* m_showAllWallpapers;
    QButtonGroup* m_gradientGroup;
    QLabel* m_gradientLabel;
    QPushButton* m_autoColor;
    QPushButton* m_transparent;
    QList<QPair<QToolButton*, QColor>> m_swatches;
    QLabel* m_colorChip;
    QLabel* m_colorHex;
    QListWidget* m_myImages;
    QWidget* m_blurRow;
    QList<QPair<QSlider*, int BackgroundSettings::*>> m_sliders;
    QList<QPair<QToolButton*, QColor BackgroundSettings::*>> m_colorButtons;
};
