#ifndef FOLDER_MANAGEMENT_COORDINATOR_H
#define FOLDER_MANAGEMENT_COORDINATOR_H

#include <QModelIndex>
#include <QObject>
#include <QString>

class FolderModel;

class FolderManagementCoordinator : public QObject
{
    Q_OBJECT

public:
    enum class RenameError {
        None,
        InvalidName,
        TargetAlreadyExists,
        FileSystemRenameFailed,
        DatabaseUpdateFailed,
        DatabaseUpdateAndRollbackFailed
    };

    struct RenameResult {
        RenameError error { RenameError::None };
        QString folderPath;
        QString databaseError;
    };

    explicit FolderManagementCoordinator(FolderModel *foldersModel, QObject *parent = nullptr);

    QModelIndex createFolder(const QModelIndex &parent, const QString &parentPath, const QString &folderName);
    RenameResult renameFolder(const QModelIndex &folder, const QString &libraryPath, const QString &newName);
    void deleteFolder(const QModelIndex &folder, const QString &folderPath);

signals:
    void folderDeletionFailed();
    void folderDeletionFinished();

private:
    FolderModel *foldersModel;
};

#endif // FOLDER_MANAGEMENT_COORDINATOR_H
