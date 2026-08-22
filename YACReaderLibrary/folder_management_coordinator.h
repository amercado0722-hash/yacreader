#ifndef FOLDER_MANAGEMENT_COORDINATOR_H
#define FOLDER_MANAGEMENT_COORDINATOR_H

#include <QModelIndex>
#include <QObject>
#include <QString>

class FolderModel;
class QWidget;

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

    explicit FolderManagementCoordinator(FolderModel *foldersModel, QWidget *dialogParent);

    QModelIndex createFolder(const QModelIndex &parent, const QString &parentPath, const QString &folderName);
    RenameResult renameFolder(const QModelIndex &folder, const QString &libraryPath, const QString &newName);
    void deleteFolder(const QModelIndex &folder, const QString &folderPath);
    void selectAndSetCustomCover(qulonglong folderId, const QString &libraryPath);
    void resetCustomCover(qulonglong folderId, const QString &libraryPath);

signals:
    void folderDeletionFailed();
    void folderDeletionFinished();

private:
    QModelIndex folderIndex(qulonglong folderId, const QString &libraryPath) const;

    FolderModel *foldersModel;
    QWidget *dialogParent;
};

#endif // FOLDER_MANAGEMENT_COORDINATOR_H
