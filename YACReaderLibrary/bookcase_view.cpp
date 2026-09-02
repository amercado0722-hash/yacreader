#include "bookcase_view.h"

#include "folder_model.h"
#include "series_name_utils.h"

#include <QColor>
#include <QQmlContext>

BookcaseView::BookcaseView(QWidget *parent)
    : QQuickWidget(parent)
{
    setResizeMode(QQuickWidget::SizeRootObjectToView);

    // Every context property the scene reads has to exist before the QML is loaded. A
    // property added afterwards does not re-run the bindings that referred to it, so the
    // wall would come up with the fallback colours and stay that way.
    rootContext()->setContextProperty("bookcase", this);
    rootContext()->setContextProperty("bookcaseBackgroundColor", QColor(16, 16, 18));
    rootContext()->setContextProperty("bookcaseTextColor", QColor(235, 235, 235));

    setSource(QUrl("qrc:/qml/Bookcase.qml"));

    initTheme(this);
}

void BookcaseView::applyTheme(const Theme &theme)
{
    rootContext()->setContextProperty("bookcaseBackgroundColor", theme.comicFlow.backgroundColor);
    rootContext()->setContextProperty("bookcaseTextColor", theme.comicFlow.textColor);
}

void BookcaseView::setFolderModel(FolderModel *model, const QModelIndex &parentIndex)
{
    folderModel = model;
    parentFolder = parentIndex;
    reload();
}

void BookcaseView::reload()
{
    series.clear();
    titles.clear();
    covers.clear();
    counts.clear();

    if (folderModel != nullptr) {
        const QModelIndex parent = parentFolder;
        const auto rows = folderModel->rowCount(parent);
        for (auto row = 0; row < rows; ++row) {
            const auto index = folderModel->index(row, 0, parent);
            if (!index.isValid()) {
                continue;
            }

            series.append(QPersistentModelIndex(index));
            titles.append(YACReader::cleanSeriesDisplayName(index.data(FolderModel::FolderNameRole).toString()));
            covers.append(index.data(FolderModel::CoverPathRole).toUrl());
            counts.append(index.data(FolderModel::NumChildrenRole).toInt());
        }
    }

    emit seriesChanged();
}

int BookcaseView::seriesCount() const
{
    return static_cast<int>(series.size());
}

QString BookcaseView::titleAt(int index) const
{
    return (index >= 0 && index < titles.size()) ? titles.at(index) : QString();
}

QUrl BookcaseView::coverAt(int index) const
{
    return (index >= 0 && index < covers.size()) ? covers.at(index) : QUrl();
}

int BookcaseView::volumesAt(int index) const
{
    return (index >= 0 && index < counts.size()) ? counts.at(index) : 0;
}

QColor BookcaseView::spineColorAt(int index) const
{
    if (index < 0 || index >= titles.size()) {
        return QColor(90, 90, 96);
    }

    // Hue from the title, saturation and lightness kept in a narrow band. Free hue with
    // fixed saturation is what gives a shelf of cloth bindings rather than a paint chart:
    // the colours differ from each other without any of them shouting.
    const auto name = titles.at(index);
    quint32 hash = 2166136261u;
    for (const auto ch : name) {
        hash = (hash ^ ch.unicode()) * 16777619u;
    }

    // A real shelf is mostly muted and mostly dark, with a few bright ones, rather than
    // every hue at the same strength - which is what made the first attempt look like a
    // paint chart and the second like a bag of sweets.
    const auto hue = static_cast<int>(hash % 360);
    auto saturation = 26 + static_cast<int>((hash >> 9) % 96);
    // Squared, so the spread runs dark with occasional light rather than sitting in a
    // uniform pastel band.
    const auto level = static_cast<double>((hash >> 17) % 256) / 255.0;
    auto lightness = static_cast<int>(34 + 104 * level * level);

    // Roughly one book in five is plain cloth or board with no colour to speak of, which is
    // what stops a shelf reading as a swatch card.
    if ((hash >> 26) % 5 == 0) {
        saturation /= 5;
        lightness = 34 + lightness / 3;
    }

    return QColor::fromHsl(hue, saturation, lightness);
}

void BookcaseView::openSeries(int index)
{
    if (index < 0 || index >= series.size()) {
        return;
    }

    const auto folder = series.at(index);
    if (folder.isValid()) {
        emit folderSelected(folder);
    }
}
