#include "yacreader_settings_widget.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QListWidget>
#include <QSplitter>
#include <QStackedWidget>

YACReaderSettingsWidget::YACReaderSettingsWidget(QWidget *parent)
    : QWidget(parent), navigation(new QListWidget(this)), pages(new QStackedWidget(this)), splitter(new QSplitter(Qt::Horizontal, this))
{
    navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navigation->setMinimumWidth(140);
    navigation->setSelectionMode(QAbstractItemView::SingleSelection);
    navigation->setUniformItemSizes(true);

    splitter->addWidget(navigation);
    splitter->addWidget(pages);
    splitter->setCollapsible(0, false);
    splitter->setCollapsible(1, false);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(splitter);

    connect(navigation, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
}

int YACReaderSettingsWidget::addPage(QWidget *page, const QString &title, const QIcon &icon)
{
    const int index = pages->addWidget(page);
    navigation->addItem(new QListWidgetItem(icon, title));
    updateNavigationSize();

    if (navigation->currentRow() == -1)
        navigation->setCurrentRow(0);

    return index;
}

void YACReaderSettingsWidget::updateNavigationSize()
{
    constexpr int minimumWidth = 140;
    constexpr int horizontalPadding = 32;

    const int navigationWidth = qMax(minimumWidth, navigation->sizeHintForColumn(0) + horizontalPadding);
    const int contentWidth = qMax(400, pages->sizeHint().width());
    splitter->setSizes({ navigationWidth, contentWidth });
}
