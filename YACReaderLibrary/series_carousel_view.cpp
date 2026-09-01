#include "series_carousel_view.h"

#include "comic_flow_widget.h"
#include "folder_model.h"
#include "series_name_utils.h"
#include "yacreader_global.h"
#include "yacreader_global_gui.h"

#include <QLabel>
#include <QSettings>
#include <QShowEvent>
#include <QUrl>
#include <QVBoxLayout>

SeriesCarouselView::SeriesCarouselView(QWidget *parent)
    : QWidget(parent)
{
    settings = new QSettings(YACReader::getSettingsPath() + "/YACReaderLibrary.ini", QSettings::IniFormat);
    settings->beginGroup("libraryConfig");

    flow = new ComicFlowWidget(this);
    // Not optional. This is what hands the engine its performance mode and vsync setting;
    // without it the flow is constructed but never animates, which looks exactly like a
    // carousel that will not turn.
    flow->updateConfig(settings);
    flow->setShowMarks(false);
    // After updateConfig, which chooses a preset of its own from the saved settings.
    // Roulette is the engine's own name for the presentation where the covers turn about
    // an axis rather than sliding past one another.
    flow->setFlowType(YACReader::Roulette);
    // The engine reads the wheel and the arrow keys itself, but only once it can be
    // focused and actually holds focus.
    flow->setFocusPolicy(Qt::StrongFocus);
    setFocusProxy(flow);

    titleLabel = new QLabel(this);
    titleLabel->setAlignment(Qt::AlignHCenter);
    titleLabel->setWordWrap(false);
    auto titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleLabel->setFont(titleFont);

    countLabel = new QLabel(this);
    countLabel->setAlignment(Qt::AlignHCenter);

    auto *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 18);
    layout->setSpacing(2);
    layout->addWidget(flow, 1);
    layout->addWidget(titleLabel);
    layout->addWidget(countLabel);
    setLayout(layout);

    connect(flow, &ComicFlowWidget::centerIndexChanged, this, &SeriesCarouselView::showCaptionFor);
    connect(flow, &ComicFlowWidget::selected, this, [this](unsigned int index) {
        const auto row = static_cast<int>(index);
        if (row < 0 || row >= series.size())
            return;
        const auto folder = series.at(row);
        if (folder.isValid())
            emit folderSelected(folder);
    });

    initTheme(this);
}

void SeriesCarouselView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    flow->setFocus(Qt::OtherFocusReason);
}

void SeriesCarouselView::applyTheme(const Theme &theme)
{
    // The flow widget themes itself; these are the captions underneath it.
    const auto background = theme.comicFlow.backgroundColor;
    setStyleSheet(QStringLiteral("SeriesCarouselView { background-color: %1; }").arg(background.name()));
    titleLabel->setStyleSheet(QStringLiteral("color: %1;").arg(theme.comicFlow.textColor.name()));

    auto subtle = theme.comicFlow.textColor;
    subtle.setAlpha(150);
    countLabel->setStyleSheet(QStringLiteral("color: rgba(%1,%2,%3,%4);").arg(subtle.red()).arg(subtle.green()).arg(subtle.blue()).arg(subtle.alpha()));
}

void SeriesCarouselView::setFolderModel(FolderModel *model, const QModelIndex &parentIndex)
{
    folderModel = model;
    parentFolder = parentIndex;
    reload();
}

int SeriesCarouselView::seriesCount() const
{
    return static_cast<int>(series.size());
}

void SeriesCarouselView::reload()
{
    series.clear();
    titles.clear();
    counts.clear();

    QStringList coverPaths;

    if (folderModel != nullptr) {
        const QModelIndex parent = parentFolder;
        const auto rows = folderModel->rowCount(parent);
        for (auto row = 0; row < rows; ++row) {
            const auto index = folderModel->index(row, 0, parent);
            if (!index.isValid())
                continue;

            // The engine takes filesystem paths; the model answers with file URLs.
            const auto coverUrl = index.data(FolderModel::CoverPathRole).toUrl();
            const auto coverPath = coverUrl.isLocalFile() ? coverUrl.toLocalFile() : coverUrl.toString();

            series.append(QPersistentModelIndex(index));
            titles.append(YACReader::cleanSeriesDisplayName(index.data(FolderModel::FolderNameRole).toString()));
            counts.append(index.data(FolderModel::NumChildrenRole).toInt());
            coverPaths.append(coverPath);
        }
    }

    // setImagePaths replaces the list on its own; the classic view does not clear first
    // and that path is the one known to work.
    flow->setImagePaths(coverPaths);

    if (!coverPaths.isEmpty()) {
        flow->setCenterIndexWithoutAnimation(0);
        showCaptionFor(0);
    } else {
        titleLabel->setText(tr("Nothing here yet"));
        countLabel->clear();
    }
}

void SeriesCarouselView::showCaptionFor(int index)
{
    if (index < 0 || index >= titles.size()) {
        titleLabel->clear();
        countLabel->clear();
        return;
    }

    titleLabel->setText(titles.at(index));

    const auto count = counts.at(index);
    countLabel->setText(count > 0
                                ? tr("%n volume(s)", "", count) + QStringLiteral("   ·   ") + tr("%1 of %2").arg(index + 1).arg(series.size())
                                : tr("%1 of %2").arg(index + 1).arg(series.size()));
}
