#ifndef YACREADER_NAVIGATION_CONTROLLER_H
#define YACREADER_NAVIGATION_CONTROLLER_H

#include <QObject>
class LibraryWindow;
class YACReaderLibrarySourceContainer;
class YACReaderContentViewsManager;

class YACReaderNavigationController : public QObject
{
    Q_OBJECT
public:
    explicit YACReaderNavigationController(LibraryWindow *parent, YACReaderContentViewsManager *contentViewsManager);

public slots:
    void selectedFolder(const QModelIndex &proxyIndex);
    void reselectCurrentFolder();
    void selectedList(const QModelIndex &proxyIndex);
    void reselectCurrentList();

    void reselectCurrentSource();
    void refreshCurrentSource();

    // history navigation
    void selectedIndexFromHistory(const YACReaderLibrarySourceContainer &sourceContainer);
    void loadIndexFromHistory(const YACReaderLibrarySourceContainer &sourceContainer);

    void loadFolderContent(const QModelIndex &folderIndex);
    void loadListContent(const QModelIndex &listIndex);
    void loadSpecialListContent(const QModelIndex &listIndex);
    void loadLabelContent(const QModelIndex &listIndex);
    void loadReadingListContent(const QModelIndex &listIndex);

    void loadPreviousStatus();
    void reloadRootContinueReading();

private:
    void setupConnections();
    void loadRootContinueReading();

    LibraryWindow *libraryWindow;
    YACReaderContentViewsManager *contentViewsManager;
    bool restoringHistorySelection = false;

    qulonglong folderIdForIndex(const QModelIndex &folderIndex) const;
};

#endif // YACREADER_NAVIGATION_CONTROLLER_H
