#include "library_search_coordinator.h"

#include "bookcase_view.h"
#include "comic_item.h"
#include "comic_model.h"
#include "comics_view.h"
#include "folder_item.h"
#include "folder_model.h"
#include "yacreader_content_views_manager.h"
#include "yacreader_folders_view.h"

LibrarySearchCoordinator::LibrarySearchCoordinator(FolderModel *foldersModel,
                                                   FolderModelProxy *foldersModelProxy,
                                                   ComicModel *comicsModel,
                                                   YACReaderFoldersView *foldersView,
                                                   YACReaderContentViewsManager *contentViewsManager,
                                                   ClearSearchInput clearSearchInput,
                                                   QObject *parent)
    : QObject(parent), foldersModel(foldersModel), foldersModelProxy(foldersModelProxy), comicsModel(comicsModel), foldersView(foldersView), contentViewsManager(contentViewsManager), clearSearchInput(std::move(clearSearchInput)), folderQueryResultProcessor(std::make_unique<YACReader::FolderQueryResultProcessor>(foldersModel))
{
    qRegisterMetaType<FolderItem *>("FolderItem *");
    qRegisterMetaType<QMap<unsigned long long, FolderItem *> *>("QMap<unsigned long long int, FolderItem *> *");

    connect(&comicQueryResultProcessor, &YACReader::ComicQueryResultProcessor::newData, this, &LibrarySearchCoordinator::applyComicResults);
    connect(folderQueryResultProcessor.get(), &YACReader::FolderQueryResultProcessor::newData, this, &LibrarySearchCoordinator::applyFolderResults);
}

bool LibrarySearchCoordinator::isSearching() const
{
    return searching;
}

bool LibrarySearchCoordinator::exitSearchMode()
{
    if (!searching)
        return false;

    clearSearchInput();
    clearResults();
    return true;
}

void LibrarySearchCoordinator::search(const QString &filter)
{
    // In the bookcase, searching means "narrow the wall to these series" rather than "find
    // these comics". Running the comic query there would answer by leaving the bookcase for
    // the comics view, which is why wiring the search box to the wall was not enough on its
    // own: the box worked and the view it filtered was no longer the one on screen.
    if (contentViewsManager->isBookcaseVisible()) {
        contentViewsManager->bookcase()->setFilter(filter);
        // Still a search as far as the rest of the window is concerned. Without this, Escape
        // has nothing to leave and the wall stays narrowed with no way back but emptying the
        // box by hand.
        searching = !filter.isEmpty();
        return;
    }

    if (!filter.isEmpty()) {
        folderQueryResultProcessor->createModelData(filter);
        comicQueryResultProcessor.createModelData(filter, foldersModel->getDatabase());
    } else if (searching) {
        clearResults();
        emit previousNavigationStateRequested();
    }
}

void LibrarySearchCoordinator::applyComicResults(QList<ComicItem *> *data, const QString &databasePath)
{
    searching = true;

    comicsModel->setModelData(data, databasePath);
    contentViewsManager->comicsView->enableFilterMode(true);
    contentViewsManager->comicsView->setModel(comicsModel); // TODO, columns are messed up after ResetModel some times, this shouldn't be necesary

    const bool noResults = comicsModel->rowCount() == 0;
    if (noResults)
        contentViewsManager->showNoSearchResults();
    else
        contentViewsManager->showComicsView();

    emit comicActionsDisabledChanged(noResults);
}

void LibrarySearchCoordinator::applyFolderResults(QMap<unsigned long long, FolderItem *> *filteredItems, FolderItem *root)
{
    foldersModelProxy->setFilterData(filteredItems, root);
    foldersView->expandAll();
}

void LibrarySearchCoordinator::clearResults()
{
    // The wall has its own filter rather than a model proxy, so clearing the search has to
    // reach it explicitly.
    if (contentViewsManager->isBookcaseVisible()) {
        contentViewsManager->bookcase()->setFilter(QString());
    }

    foldersModelProxy->clear();
    contentViewsManager->comicsView->enableFilterMode(false);
    foldersView->collapseAll();
    searching = false;
}
