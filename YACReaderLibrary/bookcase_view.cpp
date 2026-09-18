#include "bookcase_view.h"

#include "QsLog.h"
#include "bookcase_sections.h"
#include "comic_model.h"
#include "data_base_management.h"
#include "folder_model.h"
#include "series_name_utils.h"

#include <QColor>
#include <QQmlContext>
#include <QQuickWidget>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVBoxLayout>

#include <algorithm>

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

void BookcaseView::setFilter(const QString &text)
{
    const auto trimmed = text.trimmed();
    if (trimmed == filter) {
        return;
    }

    filter = trimmed;
    rebuild();
}

QString BookcaseView::filterText() const
{
    return filter;
}

// How far through each series the reader is, and whether anything is known about it, for
// every folder in the library at once.
//
// Grouped in SQL rather than counted here: eighteen thousand rows returned and summed in C++
// is the same answer for a great deal more work. Its result is kept between rebuilds, because
// narrowing the wall to a search does not change how far through anything you are, and the
// wall is rebuilt on every keystroke in the search box.
QHash<qulonglong, BookcaseView::SeriesState> BookcaseView::loadSeriesState() const
{
    QHash<qulonglong, SeriesState> result;

    if (folderModel == nullptr) {
        return result;
    }

    const auto databasePath = folderModel->getDatabase();
    if (databasePath.isEmpty()) {
        return result;
    }

    QString connectionName;
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(databasePath);
        QSqlQuery query(db);
        // The genres come back as the distinct genre strings of the folder's volumes joined
        // together. Each one is already a comma separated list and the join uses a comma
        // too, so splitting the result on commas gives every genre the series carries
        // without the query having to know how many that is.
        query.prepare("SELECT c.parentId, COUNT(*), "
                      "SUM(CASE WHEN ci.read = 1 THEN 1 ELSE 0 END), "
                      "SUM(CASE WHEN ci.synopsis IS NOT NULL AND ci.synopsis <> '' THEN 1 ELSE 0 END), "
                      "GROUP_CONCAT(DISTINCT ci.genere) "
                      "FROM comic c INNER JOIN comic_info ci ON (c.comicInfoId = ci.id) "
                      "GROUP BY c.parentId");
        query.exec();

        while (query.next()) {
            SeriesState state;
            state.volumes = query.value(1).toInt();
            state.read = query.value(2).toInt();
            state.identified = query.value(3).toInt() > 0;

            const auto genres = query.value(4).toString();
            if (!genres.isEmpty()) {
                state.genres = genres.split(u',', Qt::SkipEmptyParts);
                for (auto &genre : state.genres) {
                    genre = genre.trimmed();
                }
            }

            result.insert(query.value(0).toULongLong(), state);
        }

        connectionName = db.connectionName();
    }
    QSqlDatabase::removeDatabase(connectionName);

    return result;
}

void BookcaseView::reload()
{
    states = loadSeriesState();
    rebuild();
}

BookcaseView::SeriesState BookcaseView::aggregate(const QModelIndex &folder) const
{
    auto total = states.value(folder.data(FolderModel::IdRole).toULongLong());

    const auto rows = folderModel->rowCount(folder);
    for (auto row = 0; row < rows; ++row) {
        const auto child = folderModel->index(row, 0, folder);
        if (!child.isValid()) {
            continue;
        }

        const auto part = aggregate(child);
        total.volumes += part.volumes;
        total.read += part.read;
        total.identified = total.identified || part.identified;
        for (const auto &genre : part.genres) {
            if (!total.genres.contains(genre)) {
                total.genres.append(genre);
            }
        }
    }

    return total;
}

// The sections are shelves; everything under them is a series.
//
// The library folder can be arranged into one folder per section with the series inside them,
// and then the top level holds no comics at all - so a wall built from the immediate children
// of the top is empty spines and whatever has not been sorted yet, which is what it was.
//
// A section used to have to be named after one of the genres. That was too narrow. A library
// does not have to be arranged by genre and one here is not: seventeen thousand one-shots
// filed by artist under A to Z, which no metadata provider will ever put a genre on. But the
// shelf the reader put them on is in the path. So any top level folder holding no comics of
// its own is a section, named for itself.
//
// Only the top level. Below it the first folder is a series, and it is a series even when its
// volumes sit in a subfolder rather than loose in its own: three series here keep them in a
// "Chapters", a "Replaced" or a "- Decensored", and taking the subfolder for the series put
// those three words on the wall instead of the titles.
void BookcaseView::collect(const QModelIndex &parent, const QString &shelf)
{
    const auto rows = folderModel->rowCount(parent);

    for (auto row = 0; row < rows; ++row) {
        const auto index = folderModel->index(row, 0, parent);
        if (!index.isValid()) {
            continue;
        }

        const auto name = index.data(FolderModel::FolderNameRole).toString();
        // The library's own housekeeping - what the drop folder set aside, what the
        // duplicate finder pulled out. Real folders holding real comics, and not shelves.
        if (name.startsWith(QLatin1Char('_')) || name.startsWith(QLatin1Char('.'))) {
            continue;
        }

        auto state = states.value(index.data(FolderModel::IdRole).toULongLong());

        // A shelf: at the top of the library, holding no comics of its own, and holding
        // something that does. Both halves matter - a series that keeps its volumes in a
        // subfolder also has no comics of its own, and is not a shelf.
        if (shelf.isEmpty() && state.volumes == 0) {
            const auto beneath = aggregate(index);
            if (beneath.volumes > 0) {
                collect(index, name);
                continue;
            }
        }

        if (state.volumes == 0) {
            state = aggregate(index);
        }

        if (state.volumes == 0) {
            continue;
        }

        // The filter narrows which series are shown, never which folders are looked inside:
        // searching has to reach a series wherever it is filed.
        const auto title = YACReader::cleanSeriesDisplayName(name);
        if (!filter.isEmpty() && !title.contains(filter, Qt::CaseInsensitive)) {
            continue;
        }

        Series entry;
        entry.folder = QPersistentModelIndex(index);
        entry.title = title;
        entry.cover = index.data(FolderModel::CoverPathRole).toUrl();
        entry.volumes = state.volumes;
        entry.identified = state.identified;
        // The shelf it is actually on wins over the genre it claims. Somebody who filed a
        // series under Horror by hand meant it, and a wall that argued with the folder would
        // be telling them their own library is wrong.
        //
        // With one exception, which is the folder the unidentified series sit in. That is a
        // real folder and it would otherwise become a section like any other, and be handed a
        // colour off the wheel - so the shelf that means "nothing is known about these" would
        // come out looking like a genre somebody chose. It stays plain undyed board.
        const auto unsorted = YACReader::bookcaseSectionName(YACReader::kUnsortedSection);
        if (!shelf.isEmpty()) {
            if (shelf != unsorted) {
                entry.section = shelf;
            }
        } else {
            // Left empty rather than named when the genres say nothing, so that it falls
            // through to plain board instead of being handed a colour as though the wall
            // knew where it belonged.
            const auto genreSection = YACReader::bookcaseSectionFor(state.genres);
            if (genreSection != YACReader::kUnsortedSection) {
                entry.section = YACReader::bookcaseSectionName(genreSection);
            }
        }

        // The folder's own finished flag still counts, for anyone who does set it by hand,
        // but it is no longer the only way a series can be marked as read.
        if (state.read >= state.volumes || index.data(FolderModel::FinishedRole).toBool()) {
            entry.readState = ReadState::Read;
        } else if (state.read > 0) {
            entry.readState = ReadState::Started;
        }

        entries.append(entry);
    }
}

void BookcaseView::rebuild()
{
    // Whatever was pulled off the wall belongs to the old list of series and its index means
    // nothing against the new one.
    openedSeries = -1;

    entries.clear();
    sectionHues.clear();

    if (folderModel != nullptr) {
        collect(parentFolder, QString());
    }

    // Genres keep the hues chosen for them; anything the folders named gets one worked out
    // from how many of them there turned out to be, which is the only way to colour a set
    // of sections nobody knew about until the library was read.
    QStringList named;
    for (const auto &entry : std::as_const(entries)) {
        if (!entry.section.isEmpty() && YACReader::bookcaseSectionRank(entry.section) >= YACReader::bookcaseSections().size()) {
            named.append(entry.section);
        }
    }
    sectionHues = YACReader::bookcaseHuesFor(named);
    for (const auto &section : YACReader::bookcaseSections()) {
        sectionHues.insert(QString::fromLatin1(section.genre), section.hue);
    }

    // Sections in the order they stand on the wall, and alphabetically within one. Whatever
    // has no section at all goes last: the end of the wall is where you put the things you
    // have not dealt with yet.
    std::sort(entries.begin(), entries.end(), [](const Series &a, const Series &b) {
        const auto rankA = YACReader::bookcaseSectionRank(a.section);
        const auto rankB = YACReader::bookcaseSectionRank(b.section);
        if (rankA != rankB) {
            return rankA < rankB;
        }
        // Two sections the folders named sort against each other by name, so the wall runs
        // A, B, C rather than in whatever order the library happened to hand them over.
        const auto byName = a.section.localeAwareCompare(b.section);
        if (byName != 0) {
            return byName < 0;
        }
        return a.title.localeAwareCompare(b.title) < 0;
    });

    emit seriesChanged();
}

int BookcaseView::readStateAt(int index) const
{
    return static_cast<int>((index >= 0 && index < entries.size()) ? entries.at(index).readState : ReadState::Untouched);
}

// Defaults to identified when the index is out of range, so a series the wall does not know
// about is not accused of missing metadata it may well have.
bool BookcaseView::isIdentifiedAt(int index) const
{
    return (index >= 0 && index < entries.size()) ? entries.at(index).identified : true;
}

QString BookcaseView::sectionNameAt(int index) const
{
    if (index < 0 || index >= entries.size()) {
        return { };
    }
    const auto &section = entries.at(index).section;
    return section.isEmpty() ? YACReader::bookcaseSectionName(YACReader::kUnsortedSection) : section;
}

// The first book of a section carries its sign. Index zero always does, so the wall opens
// with one rather than with an unlabelled run.
bool BookcaseView::startsSectionAt(int index) const
{
    if (index < 0 || index >= entries.size()) {
        return false;
    }
    return index == 0 || entries.at(index - 1).section != entries.at(index).section;
}

int BookcaseView::seriesCount() const
{
    return static_cast<int>(entries.size());
}

QString BookcaseView::titleAt(int index) const
{
    return (index >= 0 && index < entries.size()) ? entries.at(index).title : QString();
}

QUrl BookcaseView::coverAt(int index) const
{
    return (index >= 0 && index < entries.size()) ? entries.at(index).cover : QUrl();
}

int BookcaseView::volumesAt(int index) const
{
    return (index >= 0 && index < entries.size()) ? entries.at(index).volumes : 0;
}

QColor BookcaseView::spineColorAt(int index) const
{
    if (index < 0 || index >= entries.size()) {
        return QColor(90, 90, 96);
    }

    // Everything but the hue comes from the title, so that no two books are quite the same
    // and a series always looks the way it looked yesterday.
    const auto &entry = entries.at(index);
    quint32 hash = 2166136261u;
    for (const auto ch : entry.title) {
        hash = (hash ^ ch.unicode()) * 16777619u;
    }

    // A real shelf is mostly muted and mostly dark, with a few bright ones, rather than
    // every hue at the same strength - which is what made the first attempt look like a
    // paint chart and the second like a bag of sweets.
    auto saturation = 26 + static_cast<int>((hash >> 9) % 96);
    // Squared, so the spread runs dark with occasional light rather than sitting in a
    // uniform pastel band.
    const auto level = static_cast<double>((hash >> 17) % 256) / 255.0;
    auto lightness = static_cast<int>(34 + 104 * level * level);

    // Roughly one book in four is plain cloth or board with no colour to speak of, which is
    // what stops a shelf reading as a swatch card. Raised from one in five once the wall was
    // sorted: a section is one family of colours now, and an undyed book every few inches is
    // most of what breaks that family up into individual books.
    if ((hash >> 26) % 4 == 0) {
        saturation /= 5;
        lightness = 34 + lightness / 3;
    }

    // The hue is the section's, give or take. Taking it from the title instead - which is what
    // this did before the wall was sorted - meant the colours carried no information at all;
    // pinning it exactly to the section would turn each one into a single block, which is the
    // bar chart this view started life as and had to be talked out of being.
    //
    // The band was thirty degrees wide, and that was too timid. Seen against a real library
    // it was fine where a shelf crossed two or three sections and flat wherever it did not:
    // Romance is four hundred and seventy four series, and four hundred and seventy four
    // books within fifteen degrees of each other is one long stripe of pink rather than a
    // shelf. Fifty six degrees still reads as one family from across the room - the sections
    // either side of it are seventy five degrees away at the very closest - and reads as
    // different books when you are standing at it.
    if (entry.section.isEmpty() || !sectionHues.contains(entry.section)) {
        // Nothing known about it, so nothing to say: plain board, no dye.
        return QColor::fromHsl(28, 12, 34 + lightness / 3);
    }

    const auto base = sectionHues.value(entry.section);
    const auto hue = (base + static_cast<int>((hash >> 3) % 57) - 28 + 360) % 360;

    return QColor::fromHsl(hue, saturation, lightness);
}

void BookcaseView::openSeries(int index)
{
    if (index < 0 || index >= entries.size() || folderModel == nullptr) {
        return;
    }

    const auto folder = entries.at(index).folder;
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
    if (openedSeries < 0 || openedSeries >= entries.size()) {
        return;
    }

    const auto id = volumeData(index, ComicModel::IdRole).toULongLong();
    if (id == 0) {
        return;
    }

    const auto folder = entries.at(openedSeries).folder;
    if (folder.isValid()) {
        emit volumeActivated(folder, id);
    }
}

void BookcaseView::showOpenedSeriesInLibrary()
{
    if (openedSeries < 0 || openedSeries >= entries.size()) {
        return;
    }

    const auto folder = entries.at(openedSeries).folder;
    if (folder.isValid()) {
        emit folderSelected(folder);
    }
}
