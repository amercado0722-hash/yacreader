#include "yacreader_settings_widget.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QHBoxLayout>
#include <QListWidget>
#include <QStackedWidget>

YACReaderSettingsWidget::YACReaderSettingsWidget(QWidget *parent)
    : QWidget(parent), navigation(new QListWidget(this)), pages(new QStackedWidget(this))
{
    navigation->setFrameShape(QFrame::NoFrame);
    navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navigation->setSelectionMode(QAbstractItemView::SingleSelection);
    navigation->setUniformItemSizes(true);

    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(navigation);
    layout->addWidget(separator);
    layout->addWidget(pages, 1);

    connect(navigation, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
}

int YACReaderSettingsWidget::addPage(QWidget *page, const QString &title, const QIcon &icon)
{
    const int index = pages->addWidget(page);
    navigation->addItem(new QListWidgetItem(icon, title));
    updateNavigationWidth();

    if (navigation->currentRow() == -1)
        navigation->setCurrentRow(0);

    return index;
}

void YACReaderSettingsWidget::updateNavigationWidth()
{
    constexpr int minimumWidth = 140;
    constexpr int maximumWidth = 240;
    constexpr int horizontalPadding = 32;

    navigation->setFixedWidth(qBound(minimumWidth, navigation->sizeHintForColumn(0) + horizontalPadding, maximumWidth));
}
