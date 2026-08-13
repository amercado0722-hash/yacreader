#include "yacreader_navigation_controller.h"

#include "QsLog.h"
#include "comic_model.h"
#include "comics_view.h"
#include "db_helper.h"
#include "empty_label_widget.h"
#include "empty_special_list.h"
#include "folder_item.h"
#include "folder_model.h"
#include "grid_comics_view.h"
#include "library_window.h"
#include "reading_list_model.h"
#include "yacreader_content_views_manager.h"
#include "yacreader_folders_view.h"
#include "yacreader_global.h"
#include "yacreader_history_controller.h"
#include "yacreader_library_list_widget.h"
#include "yacreader_reading_lists_view.h"

#include <QModelIndex>

#include <memory>

YACReaderNavigationController::YACReaderNavigationController(LibraryWindow *parent, YACReaderContentViewsManager *contentViewsManager)
    : QObject(parent), libraryWindow(parent), contentViewsManager(contentViewsManager)
{
    setupConnections();
}

void YACReaderNavigationController::selectedFolder(const QModelIndex &proxyIndex)
{
    const QModelIndex folderIndex = libraryWindow->foldersModelProxy->mapToSource(proxyIndex);

    if (!restoringHistorySelection)
        libraryWindow->historyController->updateHistory(YACReaderLibrarySourceContainer(folderIndex, YACReaderLibrarySourceContainer::Folder));

    // when a folder is selected the search mode has to be reset
    if (libraryWindow->exitSearchMode()) {
        libraryWindow->foldersView->scrollTo(folderIndex, QAbstractItemView::PositionAtTop);
        libraryWindow->foldersView->setCurrentIndex(folderIndex);
    }

    loadFolderContent(folderIndex);

    libraryWindow->setToolbarTitle(folderIndex);
}

void YACReaderNavigationController::reselectCurrentFolder()
{
    selectedFolder(libraryWindow->foldersView->currentIndex());
}

void YACReaderNavigationController::loadFolderContent(const QModelIndex &folderIndex)
{
    const qulonglong folderId = folderIdForIndex(folderIndex);
    const bool isRoot = folderId == FolderModel::RootFolderId;

    libraryWindow->comicsModel->setupFolderModelData(folderId, libraryWindow->foldersModel->getDatabase());

    if (isRoot) {
        loadRootContinueReading();
    } else {
        contentViewsManager->gridView()->clearRootContinueReadingModel();
    }

    const auto libraryName = libraryWindow->selectedLibrary->currentText();
    const auto libraryInfo = isRoot ? DBHelper::getLibraryInfoData(libraryWindow->libraries.getUuid(libraryName)) : QVariantMap();
    contentViewsManager->gridView()->setFolderModel(libraryWindow->foldersModel, folderIndex, libraryName, libraryInfo);

    if (libraryWindow->comicsModel->rowCount() > 0) {
        contentViewsManager->comicsView->setModel(libraryWindow->comicsModel);
        contentViewsManager->showComicsView();
        libraryWindow->setComicActionsDisabled(false);
    } else if (libraryWindow->foldersModel->rowCount(folderIndex) > 0) {
        // Folder has subfolders, so show the unified content grid.
        contentViewsManager->gridView()->setModel(libraryWindow->comicsModel);
        contentViewsManager->showFoldersOnlyGrid();
        libraryWindow->setComicActionsDisabled(true);
    } else {
        contentViewsManager->showEmptyFolder();
        libraryWindow->setComicActionsDisabled(true);
    }
    // if a folder is selected, listsView selection must be cleared
    libraryWindow->listsView->clearSelection();
}

void YACReaderNavigationController::loadListContent(const QModelIndex &listIndex)
{
    contentViewsManager->gridView()->clearFolderModel();
    switch (listIndex.data(ReadingListModel::TypeListsRole).toInt()) {
    case ReadingListModel::SpecialList:
        loadSpecialListContent(listIndex);
        break;

    case ReadingListModel::Label:
        loadLabelContent(listIndex);
        break;

    case ReadingListModel::ReadingList:
        loadReadingListContent(listIndex);
        break;
    }
    contentViewsManager->gridView()->setCurrentList(listIndex);
    // if a list is selected, foldersView selection must be cleared
    libraryWindow->foldersView->clearSelection();
}

void YACReaderNavigationController::loadSpecialListContent(const QModelIndex &listIndex)
{
    const auto type = static_cast<ReadingListModel::TypeSpecialList>(listIndex.data(ReadingListModel::SpecialListTypeRole).toInt());

    switch (type) {
    case ReadingListModel::TypeSpecialList::Favorites:
        libraryWindow->comicsModel->setupFavoritesModelData(libraryWindow->foldersModel->getDatabase());
        break;
    case ReadingListModel::TypeSpecialList::Reading:
        libraryWindow->comicsModel->setupReadingModelData(libraryWindow->foldersModel->getDatabase());
        break;
    case ReadingListModel::TypeSpecialList::Recent:
        libraryWindow->comicsModel->setupRecentModelData(libraryWindow->foldersModel->getDatabase());
        break;
    }

    contentViewsManager->comicsView->setModel(libraryWindow->comicsModel);

    if (libraryWindow->comicsModel->rowCount() > 0) {
        contentViewsManager->showComicsView();
        libraryWindow->setComicActionsDisabled(false);
    } else {
        contentViewsManager->showEmptySpecialList(type);
        libraryWindow->setComicActionsDisabled(true);
    }
}

void YACReaderNavigationController::loadLabelContent(const QModelIndex &listIndex)
{
    const qulonglong id = listIndex.data(ReadingListModel::IDRole).toULongLong();
    // check comics in label with id = id
    libraryWindow->comicsModel->setupLabelModelData(id, libraryWindow->foldersModel->getDatabase());
    contentViewsManager->comicsView->setModel(libraryWindow->comicsModel);

    // configure views
    if (libraryWindow->comicsModel->rowCount() > 0) {
        // updateView
        contentViewsManager->showComicsView();
        libraryWindow->setComicActionsDisabled(false);
    } else {
        // showEmptyFolder
        // loadEmptyLabelInfo(); //there is no info in an empty label by now, TODO design something
        contentViewsManager->showEmptyLabel(static_cast<YACReader::LabelColors>(listIndex.data(ReadingListModel::LabelColorRole).toInt()));
        libraryWindow->setComicActionsDisabled(true);
    }
}

void YACReaderNavigationController::loadReadingListContent(const QModelIndex &listIndex)
{
    const qulonglong id = listIndex.data(ReadingListModel::IDRole).toULongLong();
    // check comics in label with id = id
    libraryWindow->comicsModel->setupReadingListModelData(id, libraryWindow->foldersModel->getDatabase());
    contentViewsManager->comicsView->setModel(libraryWindow->comicsModel);

    // configure views
    if (libraryWindow->comicsModel->rowCount() > 0) {
        // updateView
        contentViewsManager->showComicsView();
        libraryWindow->setComicActionsDisabled(false);
    } else {
        contentViewsManager->showEmptyReadingList();
        libraryWindow->setComicActionsDisabled(true);
    }
}

void YACReaderNavigationController::selectedList(const QModelIndex &proxyIndex)
{
    const QModelIndex listIndex = libraryWindow->listsModelProxy->mapToSource(proxyIndex);

    libraryWindow->historyController->updateHistory(YACReaderLibrarySourceContainer(listIndex, YACReaderLibrarySourceContainer::List));

    // when a list is selected the search mode has to be reset
    if (libraryWindow->exitSearchMode()) {

        libraryWindow->listsView->scrollTo(proxyIndex, QAbstractItemView::PositionAtTop);
        libraryWindow->listsView->setCurrentIndex(proxyIndex);
    }

    loadListContent(listIndex);

    libraryWindow->setToolbarTitle(listIndex);
}

void YACReaderNavigationController::reselectCurrentList()
{
    selectedList(libraryWindow->listsView->currentIndex());
}

void YACReaderNavigationController::reselectCurrentSource()
{
    if (!libraryWindow->hasLoadedLibraryModels())
        return;

    if (!libraryWindow->listsView->selectionModel()->selectedRows().isEmpty()) {
        reselectCurrentList();
    } else {
        reselectCurrentFolder();
    }
}

void YACReaderNavigationController::refreshCurrentSource()
{
    if (!libraryWindow->hasLoadedLibraryModels())
        return;

    if (libraryWindow->status == LibraryWindow::Searching) {
        libraryWindow->comicsModel->reload();

        if (contentViewsManager->isComicsViewVisible())
            contentViewsManager->comicsView->reloadContent();
        return;
    }

    if (!libraryWindow->listsView->selectionModel()->selectedRows().isEmpty()) {
        auto currentListIndex = libraryWindow->listsModelProxy->mapToSource(libraryWindow->listsView->currentIndex());
        if (currentListIndex.isValid()) {
            loadListContent(currentListIndex);
            return;
        }
    }

    loadFolderContent(libraryWindow->getCurrentFolderIndex());
}

void YACReaderNavigationController::selectedIndexFromHistory(const YACReaderLibrarySourceContainer &sourceContainer)
{
    // TODO NO searching allowed, just disable backward/forward actions in searching mode
    // when a folder or a list is selected the search mode has to be reset
    libraryWindow->exitSearchMode();
    restoringHistorySelection = true;
    loadIndexFromHistory(sourceContainer);
    restoringHistorySelection = false;
    libraryWindow->setToolbarTitle(sourceContainer.getSourceModelIndex());
}

void YACReaderNavigationController::loadIndexFromHistory(const YACReaderLibrarySourceContainer &sourceContainer)
{
    QModelIndex sourceMI = sourceContainer.getSourceModelIndex();
    switch (sourceContainer.getType()) {
    case YACReaderLibrarySourceContainer::Folder: {
        if (!sourceMI.isValid()) {
            libraryWindow->setRootIndex(); // TODO: we do a double update, without it the continue reading list height comes later and causes a small flash
            break;
        }

        QModelIndex mi = libraryWindow->foldersModelProxy->mapFromSource(sourceMI);
        libraryWindow->foldersView->scrollTo(mi, QAbstractItemView::PositionAtTop);
        // currentIndexChanged is about to be emited, but we don't want it to end in YACReaderHistoryController::updateHistory
        disconnect(libraryWindow->foldersView, &YACReaderTreeView::currentIndexChanged, this, &YACReaderNavigationController::selectedFolder);
        libraryWindow->foldersView->setCurrentIndex(mi);
        connect(libraryWindow->foldersView, &YACReaderTreeView::currentIndexChanged, this, &YACReaderNavigationController::selectedFolder);
        loadFolderContent(sourceMI);
        break;
    }
    case YACReaderLibrarySourceContainer::List: {
        QModelIndex mi = libraryWindow->listsModelProxy->mapFromSource(sourceMI);
        libraryWindow->listsView->scrollTo(mi, QAbstractItemView::PositionAtTop);
        libraryWindow->listsView->setCurrentIndex(mi);
        loadListContent(sourceMI);
        break;
    }
    case YACReaderLibrarySourceContainer::None:
        QLOG_ERROR() << "Cannot load a source container of type None";
        break;
    }
}

void YACReaderNavigationController::loadRootContinueReading()
{
    auto readingComicsModel = std::make_unique<ComicModel>();

    readingComicsModel->setupReadingModelData(libraryWindow->foldersModel->getDatabase());

    contentViewsManager->gridView()->setRootContinueReadingModel(std::move(readingComicsModel));
}

void YACReaderNavigationController::reloadRootContinueReading()
{
    contentViewsManager->gridView()->reloadRootContinueReadingModel();
}

void YACReaderNavigationController::loadPreviousStatus()
{
    YACReaderLibrarySourceContainer sourceContainer = libraryWindow->historyController->currentSourceContainer();
    loadIndexFromHistory(sourceContainer);
}

void YACReaderNavigationController::setupConnections()
{
    auto *gridView = contentViewsManager->gridView();

    // we need YACReaderTreeView::currentIndexChanged to be able to navigate the folders tree using the keyboard cursors
    connect(libraryWindow->foldersView, &YACReaderTreeView::currentIndexChanged, this, &YACReaderNavigationController::selectedFolder);
    connect(libraryWindow->foldersView, &YACReaderTreeView::clicked, this, &YACReaderNavigationController::selectedFolder);
    connect(libraryWindow->listsView, &QAbstractItemView::clicked, this, &YACReaderNavigationController::selectedList);
    connect(libraryWindow->historyController, &YACReaderHistoryController::modelIndexSelected, this, &YACReaderNavigationController::selectedIndexFromHistory);
    connect(gridView, &GridComicsView::folderSelected, this, [this](const QModelIndex &index) {
        libraryWindow->foldersView->setCurrentIndex(libraryWindow->foldersModelProxy->mapFromSource(index));
    });
    connect(gridView, &GridComicsView::openFolderContextMenu, libraryWindow, [this, gridView](const QPoint &point, const Folder &folder) {
        libraryWindow->showGridFoldersContextMenu(gridView->mapToGlobal(point), folder);
    });
    connect(gridView, &GridComicsView::openContinueReadingComicContextMenu, libraryWindow, [this, gridView](const QPoint &point, const ComicDB &comic) {
        libraryWindow->showContinueReadingContextMenu(gridView->mapToGlobal(point), comic);
    });
    connect(gridView, &GridComicsView::openLibraryFolderRequested, libraryWindow, &LibraryWindow::openLibraryFolder);
    connect(libraryWindow->comicsModel, &ComicModel::isEmpty, this, &YACReaderNavigationController::reselectCurrentSource);
}

qulonglong YACReaderNavigationController::folderIdForIndex(const QModelIndex &folderIndex) const
{
    if (!folderIndex.isValid())
        return FolderModel::RootFolderId;

    auto folderItem = static_cast<FolderItem *>(folderIndex.internalPointer());
    if (folderItem != nullptr)
        return folderItem->id;

    return FolderModel::RootFolderId;
}
