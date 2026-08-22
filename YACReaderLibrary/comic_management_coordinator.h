#ifndef COMIC_MANAGEMENT_COORDINATOR_H
#define COMIC_MANAGEMENT_COORDINATOR_H

#include "yacreader_global.h"

#include <QList>
#include <QModelIndex>
#include <QObject>
#include <QPair>
#include <QString>

#include <functional>

class ComicFilesManager;
class ComicModel;
class FolderModel;
class FolderModelProxy;
class PropertiesDialog;
class QProgressDialog;
class QWidget;

class ComicManagementCoordinator : public QObject
{
    Q_OBJECT

public:
    using SelectionProvider = std::function<QModelIndexList()>;
    using CurrentListProvider = std::function<QModelIndex()>;
    using CurrentFolderProvider = std::function<QModelIndex()>;
    using LibraryPathProvider = std::function<QString()>;

    explicit ComicManagementCoordinator(QWidget *window,
                                        ComicModel *comicsModel,
                                        FolderModel *foldersModel,
                                        FolderModelProxy *foldersModelProxy,
                                        PropertiesDialog *propertiesDialog,
                                        SelectionProvider selectionProvider,
                                        CurrentListProvider currentListProvider,
                                        CurrentFolderProvider currentFolderProvider,
                                        LibraryPathProvider libraryPathProvider);

public slots:
    void copyAndImportComicsToCurrentFolder(const QList<QPair<QString, QString>> &comics);
    void moveAndImportComicsToCurrentFolder(const QList<QPair<QString, QString>> &comics);
    void copyAndImportComicsToFolder(const QList<QPair<QString, QString>> &comics, const QModelIndex &folder);
    void moveAndImportComicsToFolder(const QList<QPair<QString, QString>> &comics, const QModelIndex &folder);
    void addSelectedComicsToFavorites();
    void addSelectedComicsToLabel(qulonglong labelId);
    void showProperties();
    void setSelectedComicsRead();
    void setSelectedComicsUnread();
    void setSelectedComicsType(YACReader::FileType type);
    void resetSelectedComicRatings();
    void assignNumbers();
    void deleteMetadataFromSelectedComics();
    void deleteSelectedComics();
    void saveSelectedCoversTo();

signals:
    void importRequested(qulonglong destinationFolderId);
    void currentComicViewUpdateRequested();
    void currentSourceRefreshStarted();
    void currentSourceRefreshAccepted();
    void currentSourceRefreshCancelled();
    void comicNumbersAssigned(qint64 editedComicId);
    void comicDeletionFinished();

private:
    struct SourceContext {
        QString libraryPath;
        int mode;
        qulonglong sourceId;
    };

    QProgressDialog *newProgressDialog(const QString &label, int maximum);
    void copyAndImportComics(const QList<QPair<QString, QString>> &comics,
                             const QModelIndex &destinationFolder,
                             const QString &libraryPath);
    void moveAndImportComics(const QList<QPair<QString, QString>> &comics,
                             const QModelIndex &destinationFolder,
                             const QString &libraryPath);
    void processComicFiles(ComicFilesManager *comicFilesManager, QProgressDialog *progressDialog);
    QList<qulonglong> selectedComicIds() const;
    SourceContext currentSource() const;
    QModelIndexList indexesForComicIds(const QList<qulonglong> &comicIds, const SourceContext &source) const;
    bool isCurrentSource(const SourceContext &source) const;
    void deleteComicsFromDisk(const QList<qulonglong> &comicIds, const SourceContext &source);
    void deleteComicsFromList(const QList<qulonglong> &comicIds, const SourceContext &source, int listType, qulonglong listId);
    void finishComicDeletion();

    QWidget *window;
    ComicModel *comicsModel;
    FolderModel *foldersModel;
    FolderModelProxy *foldersModelProxy;
    PropertiesDialog *propertiesDialog;
    SelectionProvider selectionProvider;
    CurrentListProvider currentListProvider;
    CurrentFolderProvider currentFolderProvider;
    LibraryPathProvider libraryPathProvider;
    bool comicDeletionFailed { false };
};

#endif // COMIC_MANAGEMENT_COORDINATOR_H
