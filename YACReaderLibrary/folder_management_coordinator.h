#ifndef FOLDER_MANAGEMENT_COORDINATOR_H
#define FOLDER_MANAGEMENT_COORDINATOR_H

#include "yacreader_global.h"

#include <QModelIndex>
#include <QObject>
#include <QString>

#include <functional>

class FolderModel;
class QWidget;

class FolderManagementCoordinator : public QObject
{
    Q_OBJECT

public:
    using CurrentFolderProvider = std::function<QModelIndex()>;
    using LibraryPathProvider = std::function<QString()>;

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

    explicit FolderManagementCoordinator(FolderModel *foldersModel,
                                         QWidget *dialogParent,
                                         CurrentFolderProvider currentFolderProvider,
                                         LibraryPathProvider libraryPathProvider);

    QModelIndex createFolder(const QModelIndex &parent, const QString &parentPath, const QString &folderName);
    RenameResult renameFolder(const QModelIndex &folder, const QString &libraryPath, const QString &newName);
    void deleteFolder(const QModelIndex &folder, const QString &folderPath);
    void setFolderCompleted(qulonglong folderId, const QString &libraryPath, bool completed);
    void setFolderRead(qulonglong folderId, const QString &libraryPath, bool read);
    void setFolderType(qulonglong folderId, const QString &libraryPath, YACReader::FileType type);
    void selectAndSetCustomCover(qulonglong folderId, const QString &libraryPath);
    void resetCustomCover(qulonglong folderId, const QString &libraryPath);

public slots:
    void setCurrentFolderCompleted(bool completed);
    void setCurrentFolderRead(bool read);
    void setCurrentFolderType(YACReader::FileType type);
    void selectAndSetCurrentFolderCover();
    void resetCurrentFolderCover();

signals:
    void folderDeletionFailed();
    void folderDeletionFinished();

private:
    QModelIndex folderIndex(qulonglong folderId, const QString &libraryPath) const;

    FolderModel *foldersModel;
    QWidget *dialogParent;
    CurrentFolderProvider currentFolderProvider;
    LibraryPathProvider libraryPathProvider;
};

#endif // FOLDER_MANAGEMENT_COORDINATOR_H
