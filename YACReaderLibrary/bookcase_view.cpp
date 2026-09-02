#include "bookcase_view.h"

#include "QsLog.h"
#include "comic_model.h"
#include "folder_model.h"
#include "series_name_utils.h"

#include <QColor>
#include <QQmlContext>
#include <QQuickWidget>
#include <QVBoxLayout>

BookcaseView::BookcaseView(QWidget *parent)
    : QWidget(parent)
{
    // A QQuickWidget held inside a plain widget, rather than this class being one.
    //
    // That is not a style choice. As a QQuickWidget in the view stack, the scene received
    // mouse presses and releases and nothing else: no hover, no wheel, no keys. Every other
    // QML view in this application is a plain widget with a QQuickWidget laid out inside it
    // and all three work there, so the difference is worth having even without an
    // explanation for it.
    view = new QQuickWidget(this);
    view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    view->setFocusPolicy(Qt::StrongFocus);
    setFocusPolicy(Qt::StrongFocus);
    setFocusProxy(view);

    // A QML error is otherwise completely silent: the scene loads as far as it got, the
    // rest of it simply is not there, and nothing says so.
    connect(view, &QQuickWidget::statusChanged, this, [this](QQuickWidget::Status status) {
        if (status == QQuickWidget::Error) {
            QLOG_ERROR() << view->errors();
        }
    });

    volumes = new ComicModel(this);

    // Every context property the scene reads has to exist before the QML is loaded. A
    // property added afterwards does not re-run the bindings that referred to it, so the
    // wall would come up with the fallback colours and stay that way.
    view->rootContext()->setContextProperty("bookcase", this);
    view->rootContext()->setContextProperty("bookcaseBackgroundColor", QColor(16, 16, 18));
    view->rootContext()->setContextProperty("bookcaseTextColor", QColor(235, 235, 235));

    view->setSource(QUrl("qrc:/qml/Bookcase.qml"));

    auto *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(view);
    setLayout(layout);
    setContentsMargins(0, 0, 0, 0);

    initTheme(this);
}

void BookcaseView::applyTheme(const Theme &theme)
{
    view->rootContext()->setContextProperty("bookcaseBackgroundColor", theme.comicFlow.backgroundColor);
    view->rootContext()->setContextProperty("bookcaseTextColor", theme.comicFlow.textColor);
}

void BookcaseView::setFolderModel(FolderModel *model, const QModelIndex &parentIndex)
{
    folderModel = model;
    parentFolder = parentIndex;
    reload();
}

void BookcaseView::reload()
{
    // Whatever was pulled off the wall belongs to the old list of series and its index means
    // nothing against the new one.
    openedSeries = -1;

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
    if (index < 0 || index >= series.size() || folderModel == nullptr) {
        return;
    }

    const auto folder = series.at(index);
    if (!folder.isValid()) {
        return;
    }

    openedSeries = index;
    volumes->setupFolderModelData(folder.data(FolderModel::IdRole).toULongLong(), folderModel->getDatabase());

    emit volumesChanged();
}

void BookcaseView::closeSeries()
{
    openedSeries = -1;
    emit volumesChanged();
    emit seriesClosed();
}

bool BookcaseView::hasOpenedSeries() const
{
    return openedSeries >= 0;
}

QString BookcaseView::openedSeriesTitle() const
{
    return titleAt(openedSeries);
}

int BookcaseView::volumeCount() const
{
    return openedSeries >= 0 ? volumes->rowCount() : 0;
}

// One index at a time, like the wall above it. A series of two hundred and twenty one
// volumes is not unheard of in this library, and the shelf only ever shows the handful of
// rows that are actually on screen.
QVariant BookcaseView::volumeData(int index, int role) const
{
    if (openedSeries < 0 || index < 0 || index >= volumes->rowCount()) {
        return { };
    }
    return volumes->index(index, 0).data(role);
}

QString BookcaseView::volumeTitleAt(int index) const
{
    return volumeData(index, ComicModel::ReadableTitle).toString();
}

QString BookcaseView::volumeNumberAt(int index) const
{
    return volumeData(index, ComicModel::NumberRole).toString();
}

QUrl BookcaseView::volumeCoverAt(int index) const
{
    return volumeData(index, ComicModel::CoverPathRole).toUrl();
}

bool BookcaseView::volumeReadAt(int index) const
{
    return volumeData(index, ComicModel::ReadColumnRole).toBool();
}

void BookcaseView::openVolume(int index)
{
    if (openedSeries < 0 || openedSeries >= series.size()) {
        return;
    }

    const auto id = volumeData(index, ComicModel::IdRole).toULongLong();
    if (id == 0) {
        return;
    }

    const auto folder = series.at(openedSeries);
    if (folder.isValid()) {
        emit volumeActivated(folder, id);
    }
}

void BookcaseView::showOpenedSeriesInLibrary()
{
    if (openedSeries < 0 || openedSeries >= series.size()) {
        return;
    }

    const auto folder = series.at(openedSeries);
    if (folder.isValid()) {
        emit folderSelected(folder);
    }
}
