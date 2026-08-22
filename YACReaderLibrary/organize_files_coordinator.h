#ifndef ORGANIZE_FILES_COORDINATOR_H
#define ORGANIZE_FILES_COORDINATOR_H

#include "comic_db.h"

#include <QModelIndex>
#include <QObject>

#include <functional>

class ComicModel;
class FolderModel;
class QSettings;
class QWidget;

class OrganizeFilesCoordinator : public QObject
{
    Q_OBJECT
public:
    struct LibraryContext {
        qulonglong id;
        QString rootPath;
    };

    using SelectionProvider = std::function<QModelIndexList()>;
    using CurrentFolderProvider = std::function<QModelIndex()>;
    using CurrentLibraryProvider = std::function<LibraryContext()>;

    explicit OrganizeFilesCoordinator(QSettings *settings,
                                      QWidget *window,
                                      ComicModel *comicsModel,
                                      FolderModel *foldersModel,
                                      SelectionProvider selectionProvider,
                                      CurrentFolderProvider currentFolderProvider,
                                      CurrentLibraryProvider currentLibraryProvider);

public slots:
    void organizeCurrentFolder();
    void organizeSelectedComics();

signals:
    void folderRefreshRequested(const QModelIndex &folder);
    void currentSourceReloadRequested();

private:
    bool organizeFolder(qulonglong libraryId,
                        qulonglong folderId,
                        const QString &libraryRoot,
                        const QString &folderPath);
    bool organizeComics(const QList<ComicDB> &comics, const QString &libraryRoot, const QString &cleanupPath);

    QSettings *settings;
    QWidget *window;
    ComicModel *comicsModel;
    FolderModel *foldersModel;
    SelectionProvider selectionProvider;
    CurrentFolderProvider currentFolderProvider;
    CurrentLibraryProvider currentLibraryProvider;
};

#endif // ORGANIZE_FILES_COORDINATOR_H
