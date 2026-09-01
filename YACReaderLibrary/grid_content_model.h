#ifndef GRID_CONTENT_MODEL_H
#define GRID_CONTENT_MODEL_H

#include "comic_db.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QPersistentModelIndex>
#include <QSet>
#include <QUrl>
#include <QVector>

class ComicModel;
class FolderModel;
class Folder;

class GridContentModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum ItemKind {
        FolderItem = 0,
        ComicItem,
        SpacerItem,
        // A comic belonging to a folder the user expanded in place. It is deliberately a
        // distinct kind: these rows have no row in the comic model, so they must never
        // reach the code paths that map a view row to a comic model row.
        ExpandedComicItem
    };
    Q_ENUM(ItemKind)

    enum Roles {
        ItemKindRole = Qt::UserRole + 1,
        SourceIndexRole,
        NumberRole,
        TitleRole,
        FileNameRole,
        NumPagesRole,
        IdRole,
        ReadRole,
        CurrentPageRole,
        RatingRole,
        HasBeenOpenedRole,
        CoverPathRole,
        AddedRole,
        TypeRole,
        ShowRecentRole,
        RecentRangeRole,
        UpdatedRole,
        FinishedRole,
        ExpandedRole,
        ExpandableRole,
        VolumeLabelRole,
        ChildCountRole
    };

    explicit GridContentModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setComicModel(ComicModel *model);
    void setFolderModel(FolderModel *model, const QModelIndex &selectedFolderIndex);
    void clearFolderModel();
    void setMixFoldersAndComics(bool enabled);
    void setStartComicsOnNewRow(bool enabled);
    void setGridColumnCount(int columns);

    bool isFolderRow(int viewRow) const;
    bool isSpacerRow(int viewRow) const;
    bool isExpandedComicRow(int viewRow) const;
    int visibleFolderCount() const;
    int sourceComicRow(int viewRow) const;
    int viewRowForComicRow(int sourceRow) const;
    int viewRowForComicId(qulonglong id) const;
    int viewRowForFolderId(qulonglong id) const;
    QModelIndex sourceFolderIndex(int viewRow) const;
    Folder folderAt(int viewRow) const;
    Q_INVOKABLE QUrl comicCoverUrlForHash(const QString &hash) const;

    // Inline expansion: an expanded folder's comics are spliced into the grid directly
    // below it, so several series can be open at once without leaving the view.
    void toggleFolderExpansion(int viewRow);
    void collapseAllFolders();
    bool hasExpandedFolders() const { return !expandedFolders.isEmpty(); }
    ComicDB expandedComicAt(int viewRow) const;

private:
    struct Entry {
        ItemKind kind = SpacerItem;
        // Row in the source model, for FolderItem and ComicItem
        int sourceRow = -1;
        // For ExpandedComicItem: the folder it belongs to and its position in that
        // folder's loaded list
        qulonglong ownerFolderId = 0;
        int expandedIndex = -1;

        bool operator==(const Entry &other) const
        {
            return kind == other.kind && sourceRow == other.sourceRow && ownerFolderId == other.ownerFolderId && expandedIndex == other.expandedIndex;
        }
    };

    void reconnectModels();
    void resetFromSource();
    void rebuildLayout();
    QVector<Entry> buildLayout() const;
    void applyLayout(QVector<Entry> next);
    int sourceFolderCount() const;
    void loadExpandedComics(qulonglong folderId);
    static void appendRowPadding(QVector<Entry> &target, int itemsInRow, int columns);
    QString databasePath() const;

    ComicModel *comicModel = nullptr;
    FolderModel *folderModel = nullptr;
    QPersistentModelIndex selectedFolderIndex;
    bool selectedFolderIsRoot = false;
    bool mixFoldersAndComics = true;
    bool startComicsOnNewRow = false;
    int gridColumnCount = 1;
    QList<QMetaObject::Connection> sourceConnections;

    QVector<Entry> layout;
    QSet<qulonglong> expandedFolders;
    QHash<qulonglong, QList<ComicDB>> expandedComics;
};

#endif // GRID_CONTENT_MODEL_H
