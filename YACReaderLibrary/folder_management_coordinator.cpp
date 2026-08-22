#include "folder_management_coordinator.h"

#include "comics_remover.h"
#include "folder_model.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QThread>

namespace {
bool containsInvalidFolderNameCharacters(const QString &folderName)
{
    static const QRegularExpression invalidCharacters(QStringLiteral("[\\/\\\\:*?\"<>|]"));
    return folderName.contains(invalidCharacters);
}
}

FolderManagementCoordinator::FolderManagementCoordinator(FolderModel *foldersModel, QObject *parent)
    : QObject(parent), foldersModel(foldersModel)
{
}

QModelIndex FolderManagementCoordinator::createFolder(const QModelIndex &parent, const QString &parentPath, const QString &folderName)
{
    if (folderName.isEmpty() || containsInvalidFolderNameCharacters(folderName))
        return { };

    QDir parentDirectory(parentPath);
    const QDir newFolder(parentDirectory.filePath(folderName));
    if (!parentDirectory.mkdir(folderName) && !newFolder.exists())
        return { };

    return foldersModel->addFolderAtParent(folderName, parent);
}

FolderManagementCoordinator::RenameResult FolderManagementCoordinator::renameFolder(const QModelIndex &folder, const QString &libraryPath, const QString &newName)
{
    const auto oldName = folder.data(FolderModel::FolderNameRole).toString();
    if (newName.isEmpty() || newName == "." || newName == ".." || containsInvalidFolderNameCharacters(newName))
        return { RenameError::InvalidName };

    const auto oldPath = QDir::cleanPath(libraryPath + foldersModel->getFolderPath(folder));
    const QFileInfo oldFolder(oldPath);
    QDir parentDirectory(oldFolder.absolutePath());
    const auto newPath = QDir::cleanPath(parentDirectory.filePath(newName));

    if (QFileInfo::exists(newPath) && QString::compare(oldPath, newPath, Qt::CaseInsensitive) != 0)
        return { RenameError::TargetAlreadyExists };

    if (!parentDirectory.rename(oldName, newName))
        return { RenameError::FileSystemRenameFailed, oldPath };

    QString databaseError;
    if (foldersModel->renameFolder(folder, newName, &databaseError))
        return { };

    if (!parentDirectory.rename(newName, oldName))
        return { RenameError::DatabaseUpdateAndRollbackFailed, oldPath, databaseError };

    return { RenameError::DatabaseUpdateFailed, oldPath, databaseError };
}

void FolderManagementCoordinator::deleteFolder(const QModelIndex &folder, const QString &folderPath)
{
    QModelIndexList folders { folder };
    QList<QString> paths { folderPath };

    auto remover = new FoldersRemover(folders, paths);
    auto thread = new QThread(this);
    remover->moveToThread(thread);

    connect(thread, &QThread::started, remover, &FoldersRemover::process);
    connect(remover, &FoldersRemover::remove, foldersModel, &FolderModel::deleteFolder);
    connect(remover, &FoldersRemover::removeError, this, &FolderManagementCoordinator::folderDeletionFailed);
    connect(remover, &FoldersRemover::finished, this, &FolderManagementCoordinator::folderDeletionFinished);
    connect(remover, &FoldersRemover::finished, remover, &QObject::deleteLater);
    connect(remover, &FoldersRemover::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
}
