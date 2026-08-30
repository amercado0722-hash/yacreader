#include "grid_content_model.h"

#include "comic_model.h"
#include "data_base_management.h"
#include "db_helper.h"
#include "folder_model.h"
#include "library_item.h"

#include <QSqlDatabase>

#include <utility>

GridContentModel::GridContentModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int GridContentModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return layout.size();
}

QVariant GridContentModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= layout.size())
        return { };

    const auto &entry = layout.at(index.row());

    if (entry.kind == FolderItem) {
        const auto sourceIndex = sourceFolderIndex(index.row());
        switch (role) {
        case ItemKindRole:
            return FolderItem;
        case SourceIndexRole:
            return entry.sourceRow;
        case TitleRole:
        case FileNameRole:
            return sourceIndex.data(FolderModel::FolderNameRole);
        case IdRole:
            return sourceIndex.data(FolderModel::IdRole);
        case CoverPathRole:
            return sourceIndex.data(FolderModel::CoverPathRole);
        case AddedRole:
            return sourceIndex.data(FolderModel::AddedRole);
        case TypeRole:
            return sourceIndex.data(FolderModel::TypeRole);
        case ShowRecentRole:
            return sourceIndex.data(FolderModel::ShowRecentRole);
        case RecentRangeRole:
            return sourceIndex.data(FolderModel::RecentRangeRole);
        case UpdatedRole:
            return sourceIndex.data(FolderModel::UpdatedRole);
        case FinishedRole:
            return sourceIndex.data(FolderModel::FinishedRole);
        case ExpandableRole:
            return true;
        case ExpandedRole:
            return expandedFolders.contains(sourceIndex.data(FolderModel::IdRole).toULongLong());
        default:
            return { };
        }
    }

    if (entry.kind == SpacerItem) {
        if (role == ItemKindRole)
            return SpacerItem;
        if (role == SourceIndexRole)
            return -1;
        return { };
    }

    if (entry.kind == ExpandedComicItem) {
        const auto comics = expandedComics.value(entry.ownerFolderId);
        if (entry.expandedIndex < 0 || entry.expandedIndex >= comics.size())
            return { };

        const ComicDB &comic = comics.at(entry.expandedIndex);
        switch (role) {
        case ItemKindRole:
            return ExpandedComicItem;
        case SourceIndexRole:
            return -1;
        case NumberRole:
            return comic.info.number;
        case TitleRole:
            // Both branches must be QVariant; info.title is a QVariant and name a QString
            return comic.info.title.toString().isEmpty() ? QVariant(comic.name) : comic.info.title;
        case FileNameRole:
            return comic.name;
        case NumPagesRole:
            return comic.info.numPages;
        case IdRole:
            return comic.id;
        case ReadRole:
            return comic.info.read;
        case CurrentPageRole:
            return comic.info.currentPage;
        case RatingRole:
            return comic.info.rating;
        case HasBeenOpenedRole:
            return comic.info.hasBeenOpened;
        case CoverPathRole:
            return comicCoverUrlForHash(comic.info.hash);
        case VolumeLabelRole:
            return comic.info.number.toString().isEmpty()
                    ? QVariant(QString::number(entry.expandedIndex + 1))
                    : comic.info.number;
        case ExpandableRole:
            return false;
        case ExpandedRole:
            return false;
        default:
            return { };
        }
    }

    if (!comicModel)
        return { };

    const auto sourceRow = entry.sourceRow;
    const auto sourceIndex = comicModel->index(sourceRow, 0);
    switch (role) {
    case ItemKindRole:
        return ComicItem;
    case SourceIndexRole:
        return sourceRow;
    case NumberRole:
        return sourceIndex.data(ComicModel::NumberRole);
    case TitleRole:
        return sourceIndex.data(ComicModel::TitleRole);
    case FileNameRole:
        return sourceIndex.data(ComicModel::FileNameRole);
    case NumPagesRole:
        return sourceIndex.data(ComicModel::NumPagesRole);
    case IdRole:
        return sourceIndex.data(ComicModel::IdRole);
    case ReadRole:
        return sourceIndex.data(ComicModel::ReadColumnRole);
    case CurrentPageRole:
        return sourceIndex.data(ComicModel::CurrentPageRole);
    case RatingRole:
        return sourceIndex.data(ComicModel::RatingRole);
    case HasBeenOpenedRole:
        return sourceIndex.data(ComicModel::HasBeenOpenedRole);
    case CoverPathRole:
        return sourceIndex.data(ComicModel::CoverPathRole);
    case AddedRole:
        return sourceIndex.data(ComicModel::AddedRole);
    case TypeRole:
        return sourceIndex.data(ComicModel::TypeRole);
    case ShowRecentRole:
        return sourceIndex.data(ComicModel::ShowRecentRole);
    case RecentRangeRole:
        return sourceIndex.data(ComicModel::RecentRangeRole);
    case ExpandableRole:
        return false;
    case ExpandedRole:
        return false;
    default:
        return { };
    }
}

QHash<int, QByteArray> GridContentModel::roleNames() const
{
    return {
        { ItemKindRole, "item_kind" },
        { SourceIndexRole, "source_index" },
        { NumberRole, "number" },
        { TitleRole, "title" },
        { FileNameRole, "file_name" },
        { NumPagesRole, "num_pages" },
        { IdRole, "id" },
        { ReadRole, "read_column" },
        { CurrentPageRole, "current_page" },
        { RatingRole, "rating" },
        { HasBeenOpenedRole, "has_been_opened" },
        { CoverPathRole, "cover_path" },
        { AddedRole, "added_date" },
        { TypeRole, "type" },
        { ShowRecentRole, "show_recent" },
        { RecentRangeRole, "recent_range" },
        { UpdatedRole, "updated" },
        { FinishedRole, "is_finished" },
        { ExpandedRole, "is_expanded" },
        { ExpandableRole, "is_expandable" },
        { VolumeLabelRole, "volume_label" },
    };
}

void GridContentModel::setComicModel(ComicModel *model)
{
    if (comicModel == model)
        return;

    beginResetModel();
    comicModel = model;
    expandedFolders.clear();
    expandedComics.clear();
    rebuildLayout();
    endResetModel();
    reconnectModels();
}

void GridContentModel::setFolderModel(FolderModel *model, const QModelIndex &folderIndex)
{
    beginResetModel();
    folderModel = model;
    selectedFolderIndex = folderIndex;
    selectedFolderIsRoot = model && !folderIndex.isValid();
    // Navigating elsewhere invalidates every expansion: the folder rows are different now
    expandedFolders.clear();
    expandedComics.clear();
    rebuildLayout();
    endResetModel();
    reconnectModels();
}

void GridContentModel::clearFolderModel()
{
    setFolderModel(nullptr, { });
}

void GridContentModel::setMixFoldersAndComics(bool enabled)
{
    if (mixFoldersAndComics == enabled)
        return;

    beginResetModel();
    mixFoldersAndComics = enabled;
    rebuildLayout();
    endResetModel();
}

void GridContentModel::setStartComicsOnNewRow(bool enabled)
{
    if (startComicsOnNewRow == enabled)
        return;

    beginResetModel();
    startComicsOnNewRow = enabled;
    rebuildLayout();
    endResetModel();
}

void GridContentModel::setGridColumnCount(int columns)
{
    columns = qMax(1, columns);
    if (gridColumnCount == columns)
        return;

    const auto hadPadding = hasExpandedFolders() || startComicsOnNewRow;
    gridColumnCount = columns;
    // Only padding depends on the column count, and this runs on every resize step, so
    // with nothing to pad there is no reason to rebuild a layout of thousands of rows.
    if (hadPadding)
        resetFromSource();
}

bool GridContentModel::isFolderRow(int viewRow) const
{
    return viewRow >= 0 && viewRow < layout.size() && layout.at(viewRow).kind == FolderItem;
}

bool GridContentModel::isSpacerRow(int viewRow) const
{
    return viewRow >= 0 && viewRow < layout.size() && layout.at(viewRow).kind == SpacerItem;
}

bool GridContentModel::isExpandedComicRow(int viewRow) const
{
    return viewRow >= 0 && viewRow < layout.size() && layout.at(viewRow).kind == ExpandedComicItem;
}

int GridContentModel::visibleFolderCount() const
{
    const auto folders = sourceFolderCount();
    if (!mixFoldersAndComics && comicModel && comicModel->rowCount() > 0)
        return 0;
    return folders;
}

int GridContentModel::sourceComicRow(int viewRow) const
{
    if (viewRow < 0 || viewRow >= layout.size())
        return -1;

    const auto &entry = layout.at(viewRow);
    return entry.kind == ComicItem ? entry.sourceRow : -1;
}

int GridContentModel::viewRowForComicRow(int sourceRow) const
{
    if (sourceRow < 0)
        return -1;

    for (auto row = 0; row < layout.size(); ++row) {
        const auto &entry = layout.at(row);
        if (entry.kind == ComicItem && entry.sourceRow == sourceRow)
            return row;
    }
    return -1;
}

int GridContentModel::viewRowForComicId(qulonglong id) const
{
    if (!comicModel)
        return -1;

    const auto sourceIndex = comicModel->getIndexFromId(id);
    return sourceIndex.isValid() ? viewRowForComicRow(sourceIndex.row()) : -1;
}

int GridContentModel::viewRowForFolderId(qulonglong id) const
{
    for (auto row = 0; row < layout.size(); ++row) {
        if (layout.at(row).kind != FolderItem)
            continue;
        if (data(index(row, 0), IdRole).toULongLong() == id)
            return row;
    }
    return -1;
}

QModelIndex GridContentModel::sourceFolderIndex(int viewRow) const
{
    if (!folderModel || !isFolderRow(viewRow))
        return { };

    const QModelIndex parent = selectedFolderIsRoot ? QModelIndex() : QModelIndex(selectedFolderIndex);
    return folderModel->index(layout.at(viewRow).sourceRow, 0, parent);
}

Folder GridContentModel::folderAt(int viewRow) const
{
    if (!folderModel)
        return { };

    return folderModel->getFolder(sourceFolderIndex(viewRow));
}

QUrl GridContentModel::comicCoverUrlForHash(const QString &hash) const
{
    return comicModel ? comicModel->getCoverUrlPathForComicHash(hash) : QUrl();
}

ComicDB GridContentModel::expandedComicAt(int viewRow) const
{
    if (!isExpandedComicRow(viewRow))
        return { };

    const auto &entry = layout.at(viewRow);
    const auto comics = expandedComics.value(entry.ownerFolderId);
    if (entry.expandedIndex < 0 || entry.expandedIndex >= comics.size())
        return { };

    return comics.at(entry.expandedIndex);
}

void GridContentModel::toggleFolderExpansion(int viewRow)
{
    if (!isFolderRow(viewRow))
        return;

    const auto folderId = data(index(viewRow, 0), IdRole).toULongLong();
    if (folderId == 0)
        return;

    beginResetModel();
    if (expandedFolders.contains(folderId)) {
        expandedFolders.remove(folderId);
        expandedComics.remove(folderId);
    } else {
        loadExpandedComics(folderId);
        // Only mark it open if it actually has something to show, otherwise the user
        // clicks and nothing happens with no explanation.
        if (!expandedComics.value(folderId).isEmpty())
            expandedFolders.insert(folderId);
        else
            expandedComics.remove(folderId);
    }
    rebuildLayout();
    endResetModel();
}

void GridContentModel::collapseAllFolders()
{
    if (expandedFolders.isEmpty())
        return;

    beginResetModel();
    expandedFolders.clear();
    expandedComics.clear();
    rebuildLayout();
    endResetModel();
}

QString GridContentModel::databasePath() const
{
    return comicModel ? comicModel->getDatabasePath() : QString();
}

void GridContentModel::loadExpandedComics(qulonglong folderId)
{
    const auto path = databasePath();
    if (path.isEmpty())
        return;

    QList<ComicDB> comics;
    QString connectionName;
    {
        auto db = DataBaseManagement::loadDatabase(path);
        if (!db.isOpen())
            return;

        connectionName = db.connectionName();
        const auto items = DBHelper::getComicsFromParent(folderId, db, true);
        for (auto *item : items) {
            if (auto *comic = dynamic_cast<ComicDB *>(item))
                comics.append(*comic);
        }
        qDeleteAll(items);
    }
    if (!connectionName.isEmpty())
        QSqlDatabase::removeDatabase(connectionName);

    expandedComics.insert(folderId, comics);
}

void GridContentModel::appendRowPadding(int itemsInRow)
{
    const auto columns = qMax(1, gridColumnCount);
    const auto remainder = itemsInRow % columns;
    if (remainder == 0)
        return;

    Entry spacer;
    spacer.kind = SpacerItem;
    for (auto i = remainder; i < columns; ++i)
        layout.append(spacer);
}

void GridContentModel::rebuildLayout()
{
    layout.clear();

    const auto folders = visibleFolderCount();
    const auto comics = comicModel ? comicModel->rowCount() : 0;
    const QModelIndex parent = selectedFolderIsRoot ? QModelIndex() : QModelIndex(selectedFolderIndex);

    auto itemsInCurrentRow = 0;

    for (auto folderRow = 0; folderRow < folders; ++folderRow) {
        Entry folderEntry;
        folderEntry.kind = FolderItem;
        folderEntry.sourceRow = folderRow;
        layout.append(folderEntry);
        itemsInCurrentRow++;

        if (!folderModel)
            continue;

        const auto folderId = folderModel->index(folderRow, 0, parent).data(FolderModel::IdRole).toULongLong();
        if (!expandedFolders.contains(folderId))
            continue;

        const auto expanded = expandedComics.value(folderId);
        if (expanded.isEmpty())
            continue;

        // Start the group on a fresh row so it reads as belonging to the folder above it,
        // and close it off the same way so the following folders line up again.
        appendRowPadding(itemsInCurrentRow);
        itemsInCurrentRow = 0;

        for (auto i = 0; i < expanded.size(); ++i) {
            Entry comicEntry;
            comicEntry.kind = ExpandedComicItem;
            comicEntry.ownerFolderId = folderId;
            comicEntry.expandedIndex = i;
            layout.append(comicEntry);
            itemsInCurrentRow++;
        }

        appendRowPadding(itemsInCurrentRow);
        itemsInCurrentRow = 0;
    }

    if (comics == 0)
        return;

    if (folders > 0 && startComicsOnNewRow) {
        appendRowPadding(itemsInCurrentRow);
        itemsInCurrentRow = 0;
    }

    for (auto comicRow = 0; comicRow < comics; ++comicRow) {
        Entry comicEntry;
        comicEntry.kind = ComicItem;
        comicEntry.sourceRow = comicRow;
        layout.append(comicEntry);
    }
}

void GridContentModel::reconnectModels()
{
    for (const auto &connection : std::as_const(sourceConnections))
        disconnect(connection);
    sourceConnections.clear();

    // Structural changes rebuild the whole layout. Forwarding them incrementally is not
    // possible now that expanded groups make the mapping between source rows and view
    // rows non-linear, and these happen on library updates rather than while browsing.
    if (folderModel) {
        sourceConnections << connect(folderModel, &QAbstractItemModel::modelReset, this, &GridContentModel::resetFromSource);
        sourceConnections << connect(folderModel, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex &parent) {
            if (parent == selectedFolderIndex)
                resetFromSource();
        });
        sourceConnections << connect(folderModel, &QAbstractItemModel::rowsRemoved, this, [this](const QModelIndex &parent) {
            if (parent == selectedFolderIndex)
                resetFromSource();
        });
        sourceConnections << connect(folderModel, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &topLeft, const QModelIndex &bottomRight) {
            if (topLeft.parent() != selectedFolderIndex || bottomRight.parent() != selectedFolderIndex)
                return;
            for (auto sourceRow = topLeft.row(); sourceRow <= bottomRight.row(); ++sourceRow) {
                for (auto row = 0; row < layout.size(); ++row) {
                    if (layout.at(row).kind == FolderItem && layout.at(row).sourceRow == sourceRow)
                        emit dataChanged(index(row), index(row));
                }
            }
        });
    }

    if (comicModel) {
        sourceConnections << connect(comicModel, &QAbstractItemModel::modelReset, this, &GridContentModel::resetFromSource);
        sourceConnections << connect(comicModel, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex &parent) {
            if (!parent.isValid())
                resetFromSource();
        });
        sourceConnections << connect(comicModel, &QAbstractItemModel::rowsRemoved, this, [this](const QModelIndex &parent) {
            if (!parent.isValid())
                resetFromSource();
        });
        sourceConnections << connect(comicModel, &QAbstractItemModel::rowsMoved, this, [this](const QModelIndex &sourceParent, int, int, const QModelIndex &destinationParent) {
            if (!sourceParent.isValid() && !destinationParent.isValid())
                resetFromSource();
        });
        sourceConnections << connect(comicModel, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &topLeft, const QModelIndex &bottomRight) {
            if (topLeft.parent().isValid() || bottomRight.parent().isValid())
                return;
            const auto first = viewRowForComicRow(topLeft.row());
            const auto last = viewRowForComicRow(bottomRight.row());
            if (first >= 0 && last >= 0)
                emit dataChanged(index(qMin(first, last)), index(qMax(first, last)));
        });
    }
}

void GridContentModel::resetFromSource()
{
    beginResetModel();
    rebuildLayout();
    endResetModel();
}

int GridContentModel::sourceFolderCount() const
{
    if (!folderModel)
        return 0;
    if (selectedFolderIsRoot)
        return folderModel->rowCount();
    return selectedFolderIndex.isValid() ? folderModel->rowCount(selectedFolderIndex) : 0;
}
