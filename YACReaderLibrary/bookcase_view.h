#ifndef BOOKCASE_VIEW_H
#define BOOKCASE_VIEW_H

#include "themable.h"

#include <QColor>
#include <QHash>
#include <QList>
#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QStringList>
#include <QUrl>
#include <QVariant>
#include <QWidget>

class ComicModel;
class FolderModel;
class QQuickWidget;

// The library as a wall of shelves curving away around you, books standing spine out.
//
// The scene is QML rather than a 3D engine, so there is no new Qt module and no shaders:
// the curve is per-column geometry, which is enough because a cylinder seen from its axis
// only does two things to what is on it - squeezes it horizontally as the surface turns
// away, and splays the shelf lines apart towards the edges.
//
// This class is deliberately thin. It holds the series list and answers questions about
// it one index at a time, so the scene can build only the columns it can actually see;
// binding the model directly would mean instantiating nineteen hundred books to show
// ninety of them.
class BookcaseView : public QWidget, protected Themable
{
    Q_OBJECT

public:
    explicit BookcaseView(QWidget *parent = nullptr);

    void setFolderModel(FolderModel *model, const QModelIndex &parentIndex = QModelIndex());
    void reload();

    // Narrows the wall to the series whose name contains this. Plain substring rather than
    // the library's query language: the wall is a way of looking for a series by its name,
    // and nineteen hundred of them is far too many to reach by turning.
    void setFilter(const QString &text);

    // How the wall is arranged. By folder is what a library of series wants; by magazine is
    // for a library of one-shots, where the folders are artists and the thing worth reading
    // in order is the magazine an issue came out of.
    enum class Arrangement {
        ByFolder,
        ByMagazine,
    };
    Q_ENUM(Arrangement)
    Q_INVOKABLE int arrangement() const;
    Q_INVOKABLE void setArrangement(int arrangement);
    // Whether there is anything to arrange by. A library whose comics name no magazine gets
    // no toggle, rather than a toggle that empties the wall.
    Q_INVOKABLE bool canArrangeByMagazine() const;

    Q_INVOKABLE int seriesCount() const;
    Q_INVOKABLE QString titleAt(int index) const;
    Q_INVOKABLE QUrl coverAt(int index) const;
    Q_INVOKABLE int volumesAt(int index) const;
    // A stable colour for a series' spine. There is no spine artwork anywhere in a comic
    // library - only covers - so the band of colour that makes a shelf look like a shelf
    // has to be invented, and inventing it from the title means it never changes.
    Q_INVOKABLE QColor spineColorAt(int index) const;
    // What state a series is in, counted from its volumes rather than taken from the two
    // flags the folder tree keeps.
    //
    // Those flags are hand-set and default to "not finished, complete", so in a library
    // nobody has hand-marked they say the same thing about all nineteen hundred series and
    // the marks drawn from them never appear. Counting how many volumes have been read
    // costs one query and is true the moment a book is read.
    enum class ReadState {
        Untouched,
        Started,
        Read,
    };
    Q_ENUM(ReadState)
    Q_INVOKABLE int readStateAt(int index) const;
    // Whether anything is known about the series beyond its file names. Roughly a quarter of
    // this library is still unidentified after a scrape, and those are the ones worth being
    // able to pick out of a wall.
    Q_INVOKABLE bool isIdentifiedAt(int index) const;

    // The wall is sorted into sections the way a bookshop is, rather than being one
    // alphabetical run of nineteen hundred. These say which section a book is in, and which
    // book is the first of one - that book carries the sign.
    Q_INVOKABLE QString sectionNameAt(int index) const;
    Q_INVOKABLE bool startsSectionAt(int index) const;
    // What the wall is currently narrowed to, so the scene can say so when nothing matches.
    Q_INVOKABLE QString filterText() const;

    // Taking a series off the wall. The volumes are loaded here rather than in the scene,
    // because a series is a folder in the library database and the scene has no business
    // knowing that.
    Q_INVOKABLE void openSeries(int index);
    Q_INVOKABLE void closeSeries();
    bool hasOpenedSeries() const;
    Q_INVOKABLE QString openedSeriesTitle() const;

    Q_INVOKABLE int volumeCount() const;
    Q_INVOKABLE QString volumeTitleAt(int index) const;
    Q_INVOKABLE QString volumeNumberAt(int index) const;
    Q_INVOKABLE QUrl volumeCoverAt(int index) const;
    Q_INVOKABLE bool volumeReadAt(int index) const;
    Q_INVOKABLE void openVolume(int index);
    // The way back to the ordinary folder view, for the things the shelf deliberately does
    // not do: selecting several volumes, the context menu, editing metadata.
    Q_INVOKABLE void showOpenedSeriesInLibrary();
    // Whether there is anywhere to show it. A magazine issue is not a folder, so the way back
    // into the ordinary view does not exist for one and the button should not either.
    Q_INVOKABLE bool openedSeriesIsAFolder() const;

signals:
    void folderSelected(const QModelIndex &sourceIndex);
    void volumeActivated(const QModelIndex &sourceIndex, qulonglong comicId);
    // The scene rebuilds itself on these rather than being poked by name through the
    // metaobject, which is both type checked and one less string to get wrong.
    void seriesChanged();
    void volumesChanged();
    // So that a series can be put back from outside the scene - the window handles Escape,
    // because whether the QML has keyboard focus depends on where the user last clicked and
    // the way out of a view should not.
    void seriesClosed();

protected:
    void applyTheme(const Theme &theme) override;

private:
    QVariant volumeData(int index, int role) const;

    QQuickWidget *view = nullptr;

    FolderModel *folderModel = nullptr;
    QPersistentModelIndex parentFolder;

    QString filter;

    // One entry per book on the wall, in the order they stand on it - which is by section
    // and then alphabetically, not the order the folder model hands them over in.
    struct Series {
        QPersistentModelIndex folder;
        // Kept alongside the index because a magazine issue is not a folder and still has to
        // be openable; for an issue this is the folder its "no magazine" spine stands for.
        qulonglong folderId = 0;
        // The story arc that identifies this issue, empty for a spine that is a folder.
        QString issue;
        // What this sorts by inside its section: the issue in publication order, so a run of
        // a magazine reads left to right the way it was published.
        QString sortKey;
        QString title;
        QUrl cover;
        int volumes = 0;
        ReadState readState = ReadState::Untouched;
        bool identified = true;
        // What the wall shelves this under. A folder name when the library is arranged into
        // folders, the genre otherwise, and empty when nothing is known either way.
        QString section;
    };
    QList<Series> entries;

    // The hue for each section on this wall. Fixed for the genres, worked out from however
    // many folder-named sections there turn out to be for everything else.
    QHash<QString, int> sectionHues;

    // One query for the whole wall, keyed by folder id. Asking per series would be nineteen
    // hundred round trips to the database every time the view is rebuilt.
    struct SeriesState {
        int volumes = 0;
        int read = 0;
        bool identified = false;
        QStringList genres;
    };
    QHash<qulonglong, SeriesState> loadSeriesState() const;
    bool hasMagazines() const;
    // Held between rebuilds, because narrowing the wall to a search does not change how far
    // through anything you are - and rebuilding now happens on every keystroke.
    QHash<qulonglong, SeriesState> states;
    void rebuild();
    // Walks down to the series, wherever they are. The library folder can be arranged into
    // section folders, and a wall that showed only the immediate children of the top would
    // then be nineteen empty sections and whatever had not been sorted yet.
    // shelf is the name of the folder this level sits under, empty at the top of a library
    // that is not arranged into folders.
    void collect(const QModelIndex &parent, const QString &shelf);
    // The wall as magazines and their issues, read from the comics' tags rather than from
    // the folder tree.
    void collectMagazines();
    // Everything below a folder added together, for a series that keeps its volumes in a
    // subfolder rather than loose in its own.
    SeriesState aggregate(const QModelIndex &folder) const;

    // The series currently pulled off the wall, and its volumes. Its own model rather than
    // the window's, so that opening a series on the shelf does not disturb whatever the
    // ordinary comics view is showing.
    ComicModel *volumes = nullptr;
    int openedSeries = -1;

    Arrangement wallArrangement = Arrangement::ByFolder;
    // Worked out once per reload rather than per repaint: it is a query over every comic.
    bool magazinesPresent = false;
};

#endif // BOOKCASE_VIEW_H
