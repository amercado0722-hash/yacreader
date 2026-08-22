#ifndef __LIBRARYWINDOW_H
#define __LIBRARYWINDOW_H

#include "comic_db.h"
#include "comic_model.h"
#include "comic_query_result_processor.h"
#include "folder.h"
#include "folder_query_result_processor.h"
#include "libraries_update_coordinator.h"
#include "library_window_actions.h"
#include "themable.h"
#include "yacreader_global.h"
#include "yacreader_libraries.h"
#include "yacreader_navigation_controller.h"

#include <QFileInfo>
#include <QMainWindow>
#include <QMap>
#include <QModelIndex>

#include <memory>

#ifdef Y_MAC_UI
#include "yacreader_macosx_toolbar.h"
#endif

class QTreeView;
class QDirModel;
class QAction;
class QMenu;
class QToolBar;
class QComboBox;
class QThread;
class QStackedWidget;
class YACReaderSearchLineEdit;
class CreateLibraryDialog;
class ExportLibraryDialog;
class ImportLibraryDialog;
class ExportComicsInfoDialog;
class ImportComicsInfoDialog;
class AddLibraryDialog;
class HelpAboutDialog;
class RenameLibraryDialog;
class PropertiesDialog;
class PackageManager;
class QCheckBox;
class QPushButton;
class ComicModel;
class QSplitter;
class FolderModel;
class FolderModelProxy;
class QItemSelectionModel;
class QString;
class QLabel;
class NoLibrariesWidget;
class OptionsDialog;
class ServerConfigDialog;
class QCloseEvent;
class ImportWidget;
class QSettings;
class LibraryItem;
class QShowEvent;
class YACReaderTableView;
class YACReaderSideBar;
class YACReaderLibraryListWidget;
class YACReaderFoldersView;
class YACReaderMainToolBar;
class ComicVineDialog;
class ComicsView;
class ClassicComicsView;
class GridComicsView;
class ComicsViewTransition;
class NoSearchResultsWidget;
class EditShortcutsDialog;
class ReadingListModel;
class ReadingListModelProxy;
class YACReaderReadingListsView;
class YACReaderHistoryController;
class EmptyLabelWidget;
class EmptySpecialListWidget;
class EmptyReadingListWidget;
class RecentVisibilityCoordinator;
class OrganizeFilesCoordinator;
class ComicManagementCoordinator;
class FolderManagementCoordinator;
class LibraryDatabaseMaintenanceCoordinator;
class LibraryRepairCoordinator;
class LibraryManagementCoordinator;

namespace YACReader {
class TrayIconController;
class XMLInfoLibraryScanner;
}

#include "comic_db.h"

using namespace YACReader;

class LibraryWindow : public QMainWindow, protected Themable
{
    friend class YACReaderNavigationController;

    Q_OBJECT
public:
    YACReaderSideBar *sideBar;
    QSplitter *mainSplitter;

    CreateLibraryDialog *createLibraryDialog;
    ExportLibraryDialog *exportLibraryDialog;
    ImportLibraryDialog *importLibraryDialog;
    ExportComicsInfoDialog *exportComicsInfoDialog;
    ImportComicsInfoDialog *importComicsInfoDialog;
    AddLibraryDialog *addLibraryDialog;
    XMLInfoLibraryScanner *xmlInfoLibraryScanner;
    HelpAboutDialog *had;
    RenameLibraryDialog *renameLibraryDialog;
    PropertiesDialog *propertiesDialog;
    ComicVineDialog *comicVineDialog;
    EditShortcutsDialog *editShortcutsDialog;
    bool fullscreen;
    bool importedCovers; // if true, the library is read only (not updates,open comic or properties)
    bool fromMaximized;

    PackageManager *packageManager;

    QSize slideSizeW;
    QSize slideSizeF;
    // search filter
#ifdef Y_MAC_UI
    YACReaderMacOSXSearchLineEdit *searchEdit;
#else
    YACReaderSearchLineEdit *searchEdit;
#endif

    QString previousFilter;
    QCheckBox *includeComicsCheckBox;
    //-------------

    YACReaderNavigationController *navigationController;
    YACReaderContentViewsManager *contentViewsManager;

    YACReaderFoldersView *foldersView;
    YACReaderReadingListsView *listsView;
    YACReaderLibraryListWidget *selectedLibrary;
    FolderModel *foldersModel;
    FolderModelProxy *foldersModelProxy;
    ComicModel *comicsModel;
    ReadingListModel *listsModel;
    ReadingListModelProxy *listsModelProxy;

    YACReaderLibraries libraries;
    LibrariesUpdateCoordinator *librariesUpdateCoordinator;

    QStackedWidget *mainWidget;
    NoLibrariesWidget *noLibrariesWidget;
    ImportWidget *importWidget;

    bool fetching;

    int i;

    LibraryWindowActions actions;

#ifdef Y_MAC_UI
    YACReaderMacOSXToolbar *libraryToolBar;
#else
    YACReaderMainToolBar *libraryToolBar;
#endif
    QToolBar *treeActions;
    QToolBar *comicsToolBar;
    QToolBar *editInfoToolBar;
    QList<QAction *> comicToolbarEntries;
    QAction *comicToolbarEndAnchor = nullptr;

    OptionsDialog *optionsDialog;
    ServerConfigDialog *serverConfigDialog;

    QString libraryPath;
    QString comicsPath;

    enum NavigationStatus {
        Normal, //
        Searching
    };

    NavigationStatus status;

    void createSettings();
    void setupUI();
    void createToolBars();
    void createMenus();
    void createConnections();
    void doLayout();
    void doDialogs();
    void setUpShortcutsManagement();
    void doModels();
    void setupCoordinators();
    bool hasLoadedLibraryModels() const;
    QMenu *createSearchMenu();
    void applySearchQuery(const QString &query);
    void setSearchInputEnabled(bool enabled);
    void clearSearchInput(bool notify);
    void focusSearchInput();
    void showSearchSyntax();

    QString currentPath();

    // settings
    QSettings *settings;

    // navigation backward and forward
    YACReaderHistoryController *historyController;

    // QTBUG-41883
    QSize _size;
    QPoint _pos;

protected:
    virtual void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void applyTheme(const Theme &theme) override;

public:
    LibraryWindow();
    QString searchText() const;

public slots:
    void loadLibrary(const QString &path);
    void checkEmptyFolder();
    void openComic();
    void openComic(const ComicDB &comic, const ComicModel::Mode mode);
    void createLibrary();
    void showAddLibrary();
    void loadLibraries();
    void reloadCurrentLibrary();
    void updateLibrary();
    void backupLibrary();
    void restoreLibrary();
    void offerDatabaseRecovery(const QString &libraryName);
    void repairLibrary();
    // void deleteLibrary();
    void openContainingFolder();
    void organizeFiles();
    void organizeComicsFiles();
    void openContainingFolderComic();
    void deleteCurrentLibrary();
    void removeLibrary();
    void renameLibrary();
    void rescanLibraryForXMLInfo();
    void showLibraryInfo();
    void openLibraryFolder();
    void rescanCurrentFolderForXMLInfo();
    void rescanFolderForXMLInfo(QModelIndex modelIndex);
    void rename(QString newName);
    void stopXMLScanning();
    void setRootIndex();
    void toggleFullScreen();
    void toNormal();
    void toFullScreen();
    void setSearchFilter(QString filter);
    void setComicSearchFilterData(QList<ComicItem *> *, const QString &);
    void setFolderSearchFilterData(QMap<unsigned long long int, FolderItem *> *filteredItems, FolderItem *root);
    void clearSearchFilter();
    void exportLibrary(QString destPath);
    void importLibrary(QString clc, QString destPath, QString name);
    void reloadOptions();
    void showExportComicsInfo();
    void showImportComicsInfo();
    void showNoLibrariesWidget();
    void showRootWidget();
    void showImportingWidget();
    void manageCreatingError(const QString &error);
    void manageUpdatingError(const QString &error);
    void manageOpeningLibraryError(const QString &error);
    QModelIndexList getSelectedComics();
    void showFoldersContextMenu(const QPoint &point);
    void showGridFoldersContextMenu(QPoint point, Folder folder);
    void showContinueReadingContextMenu(QPoint point, ComicDB comic);
    void importLibraryPackage();
    void updateViewsOnClientSync();
    void updateViewsOnComicUpdateWithId(quint64 libraryId, quint64 comicId);
    void updateViewsOnComicUpdate(quint64 libraryId, const ComicDB &comic);
    void showComicVineScraper();
    void checkSearchNumResults(int numResults);
    void loadCoversFromCurrentModel();
    void updateCurrentFolder();
    void updateFolder(const QModelIndex &miFolder);
    void reloadCurrentFolderComicsContent();
    void reloadAfterCopyMove(const QModelIndex &mi);
    QModelIndex getCurrentFolderIndex();
    void enableNeededActions();
    void setComicActionsDisabled(bool disabled);
    void setComicToolbarEntriesVisible(bool visible);
    void addFolderToCurrentIndex();
    void renameSelectedFolder();
    void renameFolder(const QModelIndex &folder);
    void deleteSelectedFolder();
    void errorDeletingFolder();
    void addNewReadingList();
    void deleteSelectedReadingList();
    void showAddNewLabelDialog();
    void showRenameCurrentList();
    void showComicsViewContextMenu(const QPoint &point);
    void showComicsItemContextMenu(const QPoint &point);
    void showComicsContextMenu(const QPoint &point, bool showFullScreenAction);
    void setupAddToSubmenu(QMenu &menu);
    void setToolbarTitle(const QModelIndex &modelIndex);
    void setCurrentLibraryAs(FileType fileType);

    void prepareToCloseApp();
    void closeApp();

    void afterLaunchTasks();

    bool eventFilter(QObject *object, QEvent *event) override;

private:
    //! @brief Exits search mode if it is active.
    //! @return true If the search mode was active when this function was called.
    bool exitSearchMode();
    bool startsHiddenInTray() const;

    void applyLoadedLibrary(const QString &libraryDataPath, bool readOnly);
    void showLibraryManagementOnly();
    void addLibraryToSelector(const QString &libraryName, const QString &libraryPath);
    void handleLibraryRemoved(const QString &libraryName, bool librariesEmpty);

    TrayIconController *trayIconController;
    ComicQueryResultProcessor comicQueryResultProcessor;
    std::unique_ptr<FolderQueryResultProcessor> folderQueryResultProcessor;

    RecentVisibilityCoordinator *recentVisibilityCoordinator;
    OrganizeFilesCoordinator *organizeFilesCoordinator;
    ComicManagementCoordinator *comicManagementCoordinator;
    FolderManagementCoordinator *folderManagementCoordinator;
    LibraryDatabaseMaintenanceCoordinator *libraryDatabaseMaintenanceCoordinator;
    LibraryRepairCoordinator *libraryRepairCoordinator;
    LibraryManagementCoordinator *libraryManagementCoordinator;
    bool pendingAfterLaunchTasks;
};

#endif
