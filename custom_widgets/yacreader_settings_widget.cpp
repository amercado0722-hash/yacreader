#include "yacreader_settings_widget.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QListWidget>
#include <QSplitter>
#include <QStackedWidget>

namespace {
constexpr int navigationMinimumWidth = 140;
constexpr int navigationHorizontalPadding = 32;
constexpr int minimumContentWidth = 400;
constexpr int defaultContentWidth = 530;
}

YACReaderSettingsWidget::YACReaderSettingsWidget(QWidget *parent)
    : QWidget(parent), navigation(new QListWidget(this)), pages(new QStackedWidget(this)), splitter(new QSplitter(Qt::Horizontal, this))
{
    navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navigation->setMinimumWidth(navigationMinimumWidth);
    navigation->setSelectionMode(QAbstractItemView::SingleSelection);
    navigation->setUniformItemSizes(true);

    navigation->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    pages->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

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
    updateGeometry();

    if (navigation->currentRow() == -1)
        navigation->setCurrentRow(0);

    return index;
}

QSize YACReaderSettingsWidget::sizeHint() const
{
    const int width = preferredNavigationWidth() + splitter->handleWidth() + preferredContentWidth();
    const int height = qMax(pages->sizeHint().height(), navigation->sizeHint().height());

    return QSize(width, height);
}

int YACReaderSettingsWidget::preferredNavigationWidth() const
{
    return qMax(navigationMinimumWidth, navigation->sizeHintForColumn(0) + navigationHorizontalPadding);
}

int YACReaderSettingsWidget::preferredContentWidth() const
{
    return qBound(minimumContentWidth, pages->sizeHint().width(), defaultContentWidth);
}

void YACReaderSettingsWidget::updateNavigationSize()
{
    splitter->setSizes({ preferredNavigationWidth(), preferredContentWidth() });
}
