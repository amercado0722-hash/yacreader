#include "comic_management_coordinator.h"

#include "comic_files_manager.h"
#include "comic_model.h"
#include "comics_remover.h"
#include "db_helper.h"
#include "folder_model.h"
#include "properties_dialog.h"
#include "reading_list_model.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QThread>
#include <QWidget>
#include <QsLog.h>

#include <algorithm>

namespace {
template<class Remover>
void moveAndConnectRemoverToThread(Remover *remover, QThread *thread)
{
    Q_ASSERT(remover);
    Q_ASSERT(thread);
    remover->moveToThread(thread);
    QObject::connect(thread, &QThread::started, remover, &Remover::process);
    QObject::connect(remover, &Remover::finished, remover, &QObject::deleteLater);
    QObject::connect(remover, &Remover::finished, thread, &QThread::quit);
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
}
}

ComicManagementCoordinator::ComicManagementCoordinator(QWidget *window,
                                                       ComicModel *comicsModel,
                                                       FolderModel *foldersModel,
                                                       FolderModelProxy *foldersModelProxy,
                                                       PropertiesDialog *propertiesDialog,
                                                       SelectionProvider selectionProvider,
                                                       CurrentListProvider currentListProvider,
                                                       CurrentFolderProvider currentFolderProvider,
                                                       LibraryPathProvider libraryPathProvider)
    : QObject(window), window(window), comicsModel(comicsModel), foldersModel(foldersModel), foldersModelProxy(foldersModelProxy), propertiesDialog(propertiesDialog), selectionProvider(std::move(selectionProvider)), currentListProvider(std::move(currentListProvider)), currentFolderProvider(std::move(currentFolderProvider)), libraryPathProvider(std::move(libraryPathProvider))
{
    connect(propertiesDialog, &PropertiesDialog::coverChangedSignal, comicsModel, &ComicModel::notifyCoverChange);
    connect(propertiesDialog, &QDialog::accepted, this, &ComicManagementCoordinator::currentSourceRefreshAccepted);
    connect(propertiesDialog, &QDialog::rejected, this, &ComicManagementCoordinator::currentSourceRefreshCancelled);
}

void ComicManagementCoordinator::copyAndImportComicsToCurrentFolder(const QList<QPair<QString, QString>> &comics)
{
    copyAndImportComics(comics, currentFolderProvider(), libraryPathProvider());
}

void ComicManagementCoordinator::moveAndImportComicsToCurrentFolder(const QList<QPair<QString, QString>> &comics)
{
    moveAndImportComics(comics, currentFolderProvider(), libraryPathProvider());
}

void ComicManagementCoordinator::copyAndImportComicsToFolder(const QList<QPair<QString, QString>> &comics, const QModelIndex &folder)
{
    const auto destinationFolder = foldersModelProxy->mapToSource(folder);
    if (destinationFolder.isValid())
        copyAndImportComics(comics, destinationFolder, libraryPathProvider());
}

void ComicManagementCoordinator::moveAndImportComicsToFolder(const QList<QPair<QString, QString>> &comics, const QModelIndex &folder)
{
    const auto destinationFolder = foldersModelProxy->mapToSource(folder);
    if (destinationFolder.isValid())
        moveAndImportComics(comics, destinationFolder, libraryPathProvider());
}

void ComicManagementCoordinator::addSelectedComicsToFavorites()
{
    comicsModel->addComicsToFavorites(selectionProvider());
}

void ComicManagementCoordinator::addSelectedComicsToLabel(qulonglong labelId)
{
    comicsModel->addComicsToLabel(selectionProvider(), labelId);
}

void ComicManagementCoordinator::copyAndImportComics(const QList<QPair<QString, QString>> &comics,
                                                     const QModelIndex &destinationFolder,
                                                     const QString &libraryPath)
{
    if (comics.isEmpty())
        return;

    const auto destinationPath = QDir::cleanPath(libraryPath + foldersModel->getFolderPath(destinationFolder));
    const auto destinationFolderId = destinationFolder.data(FolderModel::IdRole).toULongLong();
    QLOG_DEBUG() << "Copying comics to" << destinationPath;
    auto progressDialog = newProgressDialog(QCoreApplication::translate("LibraryWindow", "Copying comics..."), comics.size());
    auto comicFilesManager = new ComicFilesManager;
    comicFilesManager->copyComicsTo(comics, destinationPath, destinationFolderId);
    processComicFiles(comicFilesManager, progressDialog);
}

void ComicManagementCoordinator::moveAndImportComics(const QList<QPair<QString, QString>> &comics,
                                                     const QModelIndex &destinationFolder,
                                                     const QString &libraryPath)
{
    if (comics.isEmpty())
        return;

    const auto destinationPath = QDir::cleanPath(libraryPath + foldersModel->getFolderPath(destinationFolder));
    const auto destinationFolderId = destinationFolder.data(FolderModel::IdRole).toULongLong();
    QLOG_DEBUG() << "Moving comics to" << destinationPath;
    auto progressDialog = newProgressDialog(QCoreApplication::translate("LibraryWindow", "Moving comics..."), comics.size());
    auto comicFilesManager = new ComicFilesManager;
    comicFilesManager->moveComicsTo(comics, destinationPath, destinationFolderId);
    processComicFiles(comicFilesManager, progressDialog);
}

void ComicManagementCoordinator::showProperties()
{
    const auto indexList = selectionProvider();
    const auto comics = comicsModel->getComics(indexList);
    if (comics.isEmpty())
        return;

    propertiesDialog->databasePath = foldersModel->getDatabase();
    propertiesDialog->basePath = libraryPathProvider();

    if (indexList.length() > 1) {
        propertiesDialog->setComics(comics);
    } else {
        const auto allComics = comicsModel->getAllComics();
        propertiesDialog->setComicsForSequentialEditing(allComics.indexOf(comics.constFirst()), allComics);
    }

    emit currentSourceRefreshStarted();
    propertiesDialog->show();
}

void ComicManagementCoordinator::setSelectedComicsRead()
{
    comicsModel->setComicsRead(selectionProvider(), YACReader::Read);
    emit currentComicViewUpdateRequested();
}

void ComicManagementCoordinator::setSelectedComicsUnread()
{
    comicsModel->setComicsRead(selectionProvider(), YACReader::Unread);
    emit currentComicViewUpdateRequested();
}

void ComicManagementCoordinator::setSelectedComicsType(YACReader::FileType type)
{
    comicsModel->setComicsType(selectionProvider(), type);
}

void ComicManagementCoordinator::resetSelectedComicRatings()
{
    const auto indexList = selectionProvider();
    comicsModel->startTransaction();
    for (const auto &index : indexList)
        comicsModel->resetComicRating(index);
    comicsModel->finishTransaction();
}

void ComicManagementCoordinator::assignNumbers()
{
    const auto selectedIds = selectedComicIds();
    if (selectedIds.isEmpty())
        return;

    const auto source = currentSource();
    const auto initialIndexes = indexesForComicIds(selectedIds, source);
    if (initialIndexes.isEmpty())
        return;

    int startingNumber = initialIndexes.constFirst().row() + 1;
    if (initialIndexes.count() > 1) {
        bool accepted;
        startingNumber = QInputDialog::getInt(window,
                                              QCoreApplication::translate("LibraryWindow", "Assign comics numbers"),
                                              QCoreApplication::translate("LibraryWindow", "Assign numbers starting in:"),
                                              startingNumber,
                                              0,
                                              2147483647,
                                              1,
                                              &accepted);
        if (!accepted)
            return;
    }

    const auto indexList = indexesForComicIds(selectedIds, source);
    if (indexList.isEmpty())
        return;

    emit comicNumbersAssigned(comicsModel->asignNumbers(indexList, startingNumber));
}

void ComicManagementCoordinator::deleteMetadataFromSelectedComics()
{
    auto comics = comicsModel->getComics(selectionProvider());
    if (comics.isEmpty())
        return;

    for (auto &comic : comics)
        comic.info.deleteMetadata();

    DBHelper::updateComicsInfo(comics, foldersModel->getDatabase());
    comicsModel->reload();
}

void ComicManagementCoordinator::deleteSelectedComics()
{
    const auto comicIds = selectedComicIds();
    if (comicIds.isEmpty())
        return;

    const auto source = currentSource();
    const auto listIndex = currentListProvider();
    if (listIndex.isValid()) {
        deleteComicsFromList(comicIds,
                             source,
                             listIndex.data(ReadingListModel::TypeListsRole).toInt(),
                             listIndex.data(ReadingListModel::IDRole).toULongLong());
    } else {
        deleteComicsFromDisk(comicIds, source);
    }
}

void ComicManagementCoordinator::saveSelectedCoversTo()
{
    const auto comicIds = selectedComicIds();
    if (comicIds.isEmpty())
        return;

    const auto source = currentSource();
    const auto destinationFolder = QFileDialog::getExistingDirectory(window,
                                                                     QCoreApplication::translate("LibraryWindow", "Save covers"),
                                                                     QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
    if (destinationFolder.isEmpty())
        return;

    const auto indexList = indexesForComicIds(comicIds, source);
    for (const auto &comic : indexList) {
        QString origin = comic.data(ComicModel::CoverPathRole).toString().remove("file:///").remove("file:");
        const auto destination = QDir(destinationFolder).filePath(comic.data(ComicModel::FileNameRole).toString() + ".jpg");

        QLOG_DEBUG() << "From : " << origin;
        QLOG_DEBUG() << "To : " << destination;

        QFile::copy(origin, destination);
    }
}

QProgressDialog *ComicManagementCoordinator::newProgressDialog(const QString &label, int maximum)
{
    auto progressDialog = new QProgressDialog(label, QStringLiteral("Cancel"), 0, maximum, window);
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->setMinimumWidth(350);
    progressDialog->show();
    return progressDialog;
}

void ComicManagementCoordinator::processComicFiles(ComicFilesManager *comicFilesManager, QProgressDialog *progressDialog)
{
    connect(comicFilesManager, &ComicFilesManager::progress, progressDialog, &QProgressDialog::setValue);

    auto thread = new QThread;
    comicFilesManager->moveToThread(thread);

    connect(progressDialog, &QProgressDialog::canceled, comicFilesManager, &ComicFilesManager::cancel, Qt::DirectConnection);
    connect(thread, &QThread::started, comicFilesManager, &ComicFilesManager::process);
    connect(comicFilesManager, &ComicFilesManager::success, this, &ComicManagementCoordinator::importRequested);
    connect(comicFilesManager, &ComicFilesManager::finished, thread, &QThread::quit);
    connect(comicFilesManager, &ComicFilesManager::finished, comicFilesManager, &QObject::deleteLater);
    connect(comicFilesManager, &ComicFilesManager::finished, progressDialog, &QWidget::close);
    connect(comicFilesManager, &ComicFilesManager::finished, progressDialog, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
}

QList<qulonglong> ComicManagementCoordinator::selectedComicIds() const
{
    QList<qulonglong> comicIds;
    const auto selection = selectionProvider();
    comicIds.reserve(selection.size());
    for (const auto &index : selection)
        comicIds.append(index.data(ComicModel::IdRole).toULongLong());
    return comicIds;
}

ComicManagementCoordinator::SourceContext ComicManagementCoordinator::currentSource() const
{
    return { libraryPathProvider(), static_cast<int>(comicsModel->getMode()), comicsModel->getSourceId() };
}

QModelIndexList ComicManagementCoordinator::indexesForComicIds(const QList<qulonglong> &comicIds, const SourceContext &source) const
{
    if (!isCurrentSource(source))
        return { };

    auto indexes = comicsModel->getIndexesFromIds(comicIds);
    if (std::any_of(indexes.cbegin(), indexes.cend(), [](const QModelIndex &index) { return !index.isValid(); }))
        return { };

    std::sort(indexes.begin(), indexes.end(), [](const QModelIndex &left, const QModelIndex &right) {
        return left.row() < right.row();
    });
    return indexes;
}

bool ComicManagementCoordinator::isCurrentSource(const SourceContext &source) const
{
    return QDir::cleanPath(foldersModel->getDatabase()) == QDir::cleanPath(YACReader::LibraryPaths::libraryDataPath(source.libraryPath)) && static_cast<int>(comicsModel->getMode()) == source.mode && comicsModel->getSourceId() == source.sourceId;
}

void ComicManagementCoordinator::deleteComicsFromDisk(const QList<qulonglong> &comicIds, const SourceContext &source)
{
    const auto answer = QMessageBox::question(window,
                                              QCoreApplication::translate("LibraryWindow", "Delete comics"),
                                              QCoreApplication::translate("LibraryWindow", "All the selected comics will be deleted from your disk. Are you sure?"),
                                              QMessageBox::Yes,
                                              QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    auto indexList = indexesForComicIds(comicIds, source);
    auto comics = comicsModel->getComics(indexList);
    if (comics.isEmpty())
        return;

    QList<QString> paths;
    paths.reserve(comics.size());
    for (const auto &comic : comics) {
        paths.append(source.libraryPath + comic.path);
        QLOG_TRACE() << comic.path;
        QLOG_TRACE() << comic.id;
        QLOG_TRACE() << comic.parentId;
    }

    auto remover = new ComicsRemover(indexList, paths, comics.constFirst().parentId);
    auto thread = new QThread(this);
    moveAndConnectRemoverToThread(remover, thread);

    comicDeletionFailed = false;
    comicsModel->startTransaction();

    connect(remover, &ComicsRemover::remove, comicsModel, &ComicModel::remove);
    connect(remover, &ComicsRemover::removeError, this, [this] { comicDeletionFailed = true; });
    connect(remover, &ComicsRemover::finished, comicsModel, &ComicModel::finishTransaction);
    connect(remover, &ComicsRemover::removedItemsFromFolder, foldersModel, &FolderModel::updateFolderChildrenInfo);
    connect(remover, &ComicsRemover::finished, this, &ComicManagementCoordinator::finishComicDeletion);

    thread->start();
}

void ComicManagementCoordinator::deleteComicsFromList(const QList<qulonglong> &comicIds, const SourceContext &source, int listType, qulonglong listId)
{
    const auto answer = QMessageBox::question(window,
                                              QCoreApplication::translate("LibraryWindow", "Remove comics"),
                                              QCoreApplication::translate("LibraryWindow", "Comics will only be deleted from the current label/list. Are you sure?"),
                                              QMessageBox::Yes,
                                              QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    const auto currentList = currentListProvider();
    if (!currentList.isValid() || currentList.data(ReadingListModel::TypeListsRole).toInt() != listType || currentList.data(ReadingListModel::IDRole).toULongLong() != listId)
        return;

    const auto indexList = indexesForComicIds(comicIds, source);
    if (indexList.isEmpty())
        return;

    switch (static_cast<ReadingListModel::TypeList>(listType)) {
    case ReadingListModel::SpecialList:
        comicsModel->deleteComicsFromSpecialList(indexList, listId);
        break;
    case ReadingListModel::Label:
        comicsModel->deleteComicsFromLabel(indexList, listId);
        break;
    case ReadingListModel::ReadingList:
        comicsModel->deleteComicsFromReadingList(indexList, listId);
        break;
    case ReadingListModel::Separator:
        break;
    }
}

void ComicManagementCoordinator::finishComicDeletion()
{
    emit comicDeletionFinished();
    if (comicDeletionFailed) {
        QMessageBox::critical(window,
                              QCoreApplication::translate("LibraryWindow", "Unable to delete"),
                              QCoreApplication::translate("LibraryWindow", "There was an issue trying to delete the selected comics. Please, check for write permissions in the selected files or containing folder."));
    }
    comicDeletionFailed = false;
}
