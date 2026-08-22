#include "library_window.h"

#include "yacreader_global.h"
#include "yacreader_global_gui.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QShowEvent>
#include <QSplitter>
#include <QSqlError>
#include <QStackedWidget>
#include <QToolBar>
#include <QToolButton>
#include <QtCore>

#include <algorithm>

#ifdef Q_OS_WIN
#include <qt_windows.h>

#include <shellapi.h>
#endif

#include "QsLog.h"
#include "add_label_dialog.h"
#include "add_library_dialog.h"
#include "api_key_dialog.h"
#include "comic_db.h"
#include "comic_management_coordinator.h"
#include "comic_model.h"
#include "comic_vine_dialog.h"
#include "comics_view.h"
#include "create_library_dialog.h"
#include "data_base_management.h"
#include "db_helper.h"
#include "edit_shortcuts_dialog.h"
#include "export_comics_info_dialog.h"
#include "export_library_dialog.h"
#include "feature_flags.h"
#include "folder_item.h"
#include "folder_management_coordinator.h"
#include "folder_model.h"
#include "grid_comics_view.h"
#include "help_about_dialog.h"
#include "import_comics_info_dialog.h"
#include "import_library_dialog.h"
#include "import_widget.h"
#include "library_comic_opener.h"
#include "library_database_maintenance_coordinator.h"
#include "library_management_coordinator.h"
#include "library_repair_coordinator.h"
#include "no_libraries_widget.h"
#include "options_dialog.h"
#include "organize_files_coordinator.h"
#include "package_manager.h"
#include "properties_dialog.h"
#include "reading_list_item.h"
#include "reading_list_model.h"
#include "recent_visibility_coordinator.h"
#include "rename_library_dialog.h"
#include "search_syntax_dialog.h"
#include "server_config_dialog.h"
#include "shortcuts_manager.h"
#include "static.h"
#include "trayicon_controller.h"
#include "whats_new_controller.h"
#include "xml_info_library_scanner.h"
#include "yacreader_content_views_manager.h"
#include "yacreader_folders_view.h"
#include "yacreader_history_controller.h"
#include "yacreader_http_server.h"
#include "yacreader_library_list_widget.h"
#include "yacreader_main_toolbar.h"
#include "yacreader_reading_lists_view.h"
#include "yacreader_search_line_edit.h"
#include "yacreader_sidebar.h"
#include "yacreader_titled_toolbar.h"
#include "yacreader_tool_bar_stretch.h"
extern YACReaderHttpServer *httpServer;

#include <KDSignalThrottler.h>

using namespace YACReader;

LibraryWindow::LibraryWindow()
    : QMainWindow(), fullscreen(false), previousFilter(""), fetching(false), status(LibraryWindow::Normal), pendingAfterLaunchTasks(false)
{
    createSettings();

    setupUI();

    loadLibraries();

    if (libraries.isEmpty()) {
        showNoLibrariesWidget();
    } else {
        showRootWidget();
        selectedLibrary->setCurrentIndex(0);
    }

    if (startsHiddenInTray()) {
        pendingAfterLaunchTasks = true;
    } else {
        afterLaunchTasks();
    }
}

void LibraryWindow::afterLaunchTasks()
{
    if (!libraries.isEmpty()) {
        WhatsNewController whatsNewController;
        whatsNewController.showWhatsNewIfNeeded(this);
    }
}

bool LibraryWindow::startsHiddenInTray() const
{
    return settings->value(START_TO_TRAY, false).toBool() && settings->value(CLOSE_TO_TRAY, false).toBool();
}

void LibraryWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);

    if (pendingAfterLaunchTasks) {
        pendingAfterLaunchTasks = false;
        afterLaunchTasks();
    }
}

bool LibraryWindow::eventFilter(QObject *object, QEvent *event)
{
    if (this->isActiveWindow()) {
        if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease) {
            auto mouseEvent = static_cast<QMouseEvent *>(event);

            if (mouseEvent->button() == Qt::ForwardButton) {
                if (event->type() == QEvent::MouseButtonRelease)
                    actions.forwardAction->trigger();
                event->accept();
                return true;
            }

            if (mouseEvent->button() == Qt::BackButton) {
                if (event->type() == QEvent::MouseButtonRelease)
                    actions.backAction->trigger();
                event->accept();
                return true;
            }
        }
    }

    if (this->foldersView->hasFocus() && event->type() == QEvent::Shortcut) {
        auto shortcutEvent = static_cast<QShortcutEvent *>(event);
        auto keySequence = shortcutEvent->key();

        if (keySequence.count() > 1) {
            return QMainWindow::eventFilter(object, event);
        }

        auto keyCombination = keySequence[0];

        if (keyCombination.keyboardModifiers() != Qt::NoModifier) {
            return QMainWindow::eventFilter(object, event);
        }

        auto string = keySequence.toString();

        if (string.size() > 1) {
            return QMainWindow::eventFilter(object, event);
        }

        event->ignore();

        foldersView->keyboardSearch(keySequence.toString());
        return true;
    }

    return QMainWindow::eventFilter(object, event);
}

void LibraryWindow::createSettings()
{
    settings = new QSettings(YACReader::getSettingsPath() + "/YACReaderLibrary.ini", QSettings::IniFormat); // TODO unificar la creación del fichero de config con el servidor
    settings->beginGroup("libraryConfig");
}

void LibraryWindow::setupUI()
{
    setUnifiedTitleAndToolBarOnMac(true);

    packageManager = new PackageManager();
    xmlInfoLibraryScanner = new XMLInfoLibraryScanner();

    historyController = new YACReaderHistoryController(this);

    actions.createActions(this, settings);
    doModels();

    doDialogs();
    doLayout();
    createToolBars();
    createMenus();

    setupCoordinators();

    navigationController = new YACReaderNavigationController(this, contentViewsManager);

    createConnections();

    setWindowTitle(tr("YACReader Library"));

    setMinimumSize(800, 480);

    // restore
    if (settings->contains(MAIN_WINDOW_GEOMETRY)) {
        restoreGeometry(settings->value(MAIN_WINDOW_GEOMETRY).toByteArray());
        restoreState(settings->value(MAIN_WINDOW_STATE).toByteArray());
        // Guard against the window landing off-screen when a monitor is unplugged
        // between sessions. Qt 6 tries to remap the geometry to the primary screen
        // when the saved screen is gone, but the result can still be off-screen.
        const QRect restored = geometry();
        const auto availableScreens = QApplication::screens();
        const bool onScreen = std::any_of(
                availableScreens.cbegin(), availableScreens.cend(),
                [&restored](QScreen *s) { return s->availableGeometry().intersects(restored); });
        if (!onScreen) {
            const QRect avail = QApplication::primaryScreen()->availableGeometry();
            setGeometry(QRect(avail.center() - QPoint(width() / 2, height() / 2), size()));
        }
    } else if (startsHiddenInTray()) {
        setWindowState(windowState() | Qt::WindowMaximized);
    } else {
        // if(settings->value(USE_OPEN_GL).toBool() == false)
        showMaximized();
    }

    trayIconController = new TrayIconController(settings, this);

    initTheme(this);
}

void LibraryWindow::applyTheme(const Theme &theme)
{
    editInfoToolBar->setStyleSheet(theme.comicsViewToolbar.toolbarQSS);
    mainSplitter->setStyleSheet(theme.contentSplitter.horizontalSplitterQSS);

    // Update main toolbar and comics view toolbar icons
    actions.updateTheme(theme);
}

void LibraryWindow::doLayout()
{
    // LAYOUT ELEMENTS------------------------------------------------------------
    mainSplitter = new QSplitter(Qt::Horizontal); // spliter principal
    auto sHorizontal = mainSplitter; // Keep local alias for existing code

    // TOOLBARS-------------------------------------------------------------------
    //---------------------------------------------------------------------------
    editInfoToolBar = new QToolBar();

#ifdef Y_MAC_UI
    libraryToolBar = new YACReaderMacOSXToolbar(this);
#else
    libraryToolBar = new YACReaderMainToolBar(this);
#endif

    // FOLDERS FILTER-------------------------------------------------------------
    //---------------------------------------------------------------------------
#ifndef Y_MAC_UI
    // in MacOSX the searchEdit is created using the toolbar wrapper
    searchEdit = new YACReaderSearchLineEdit();
#endif

    // SIDEBAR--------------------------------------------------------------------
    //---------------------------------------------------------------------------
    sideBar = new YACReaderSideBar;

    foldersView = sideBar->foldersView;
    listsView = sideBar->readingListsView;
    selectedLibrary = sideBar->selectedLibrary;

    YACReaderTitledToolBar *librariesTitle = sideBar->librariesTitle;
    YACReaderTitledToolBar *foldersTitle = sideBar->foldersTitle;
    YACReaderTitledToolBar *readingListsTitle = sideBar->readingListsTitle;

    librariesTitle->addAction(actions.createLibraryAction);
    librariesTitle->addAction(actions.openLibraryAction);
    librariesTitle->addSpacing(3);

    foldersTitle->addAction(actions.addFolderAction);
    foldersTitle->addAction(actions.deleteFolderAction);
    foldersTitle->addSepartor();
    foldersTitle->addAction(actions.setRootIndexAction);
    foldersTitle->addAction(actions.expandAllNodesAction);
    foldersTitle->addAction(actions.colapseAllNodesAction);

    readingListsTitle->addAction(actions.addReadingListAction);
    // readingListsTitle->addSepartor();
    readingListsTitle->addAction(actions.addLabelAction);
    // readingListsTitle->addSepartor();
    readingListsTitle->addAction(actions.renameListAction);
    readingListsTitle->addAction(actions.deleteReadingListAction);
    readingListsTitle->addSpacing(3);

    // FINAL LAYOUT-------------------------------------------------------------

    contentViewsManager = new YACReaderContentViewsManager(settings, this);

    sHorizontal->addWidget(sideBar);
#ifndef Y_MAC_UI
    QVBoxLayout *rightLayout = new QVBoxLayout;
    rightLayout->addWidget(libraryToolBar);
    rightLayout->addWidget(contentViewsManager->containerWidget());

    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    QWidget *rightWidget = new QWidget();
    rightWidget->setLayout(rightLayout);

    sHorizontal->addWidget(rightWidget);
#else
    sHorizontal->addWidget(contentViewsManager->containerWidget());
#endif

    sHorizontal->setStretchFactor(0, 0);
    sHorizontal->setStretchFactor(1, 1);
    mainWidget = new QStackedWidget(this);
    mainWidget->addWidget(sHorizontal);
    setCentralWidget(mainWidget);
    // FINAL LAYOUT-------------------------------------------------------------

    // OTHER----------------------------------------------------------------------
    //---------------------------------------------------------------------------
    noLibrariesWidget = new NoLibrariesWidget();
    mainWidget->addWidget(noLibrariesWidget);

    importWidget = new ImportWidget();
    mainWidget->addWidget(importWidget);

    connect(noLibrariesWidget, &NoLibrariesWidget::createNewLibrary, this, &LibraryWindow::createLibrary);
    connect(noLibrariesWidget, &NoLibrariesWidget::addExistingLibrary, this, &LibraryWindow::showAddLibrary);

    // collapsible disabled in macosx (only temporaly)
#ifdef Y_MAC_UI
    sHorizontal->setCollapsible(0, false);
#endif
}

void LibraryWindow::doDialogs()
{
    createLibraryDialog = new CreateLibraryDialog(this);
    renameLibraryDialog = new RenameLibraryDialog(this);
    propertiesDialog = new PropertiesDialog(this);
    comicVineDialog = new ComicVineDialog(this);
    exportLibraryDialog = new ExportLibraryDialog(this);
    importLibraryDialog = new ImportLibraryDialog(this);
    exportComicsInfoDialog = new ExportComicsInfoDialog(this);
    importComicsInfoDialog = new ImportComicsInfoDialog(this);
    addLibraryDialog = new AddLibraryDialog(this);
    optionsDialog = new OptionsDialog(this);
    optionsDialog->restoreOptions(settings);

    editShortcutsDialog = new EditShortcutsDialog(this);
    actions.setUpShortcutsManagement(editShortcutsDialog);

#ifdef SERVER_RELEASE
    serverConfigDialog = new ServerConfigDialog(this);
#endif

    had = new HelpAboutDialog(this); // TODO load data.
    QString sufix = QLocale::system().name();
    if (QFile(":/files/about_" + sufix + ".html").exists())
        had->loadAboutInformation(":/files/about_" + sufix + ".html");
    else
        had->loadAboutInformation(":/files/about.html");

    if (QFile(":/files/helpYACReaderLibrary_" + sufix + ".html").exists())
        had->loadHelp(":/files/helpYACReaderLibrary_" + sufix + ".html");
    else
        had->loadHelp(":/files/helpYACReaderLibrary.html");
}

void LibraryWindow::doModels()
{
    // folders
    foldersModel = new FolderModel(this);
    foldersModelProxy = new FolderModelProxy(this);
    folderQueryResultProcessor.reset(new FolderQueryResultProcessor(foldersModel));
    // foldersModelProxy->setSourceModel(foldersModel);
    // comics
    comicsModel = new ComicModel(this);
    // lists
    listsModel = new ReadingListModel(this);
    listsModelProxy = new ReadingListModelProxy(this);
}

void LibraryWindow::setupCoordinators()
{
    recentVisibilityCoordinator = new RecentVisibilityCoordinator(settings, foldersModel, comicsModel);
    organizeFilesCoordinator = new OrganizeFilesCoordinator(settings, this);
    comicManagementCoordinator = new ComicManagementCoordinator(
            this,
            comicsModel,
            foldersModel,
            propertiesDialog,
            [this] { return getSelectedComics(); },
            [this] {
                if (listsView->selectionModel() == nullptr || listsView->selectionModel()->selectedRows().isEmpty())
                    return QModelIndex();
                return listsModelProxy->mapToSource(listsView->currentIndex());
            },
            [this] { return currentPath(); });
    connect(comicManagementCoordinator, &ComicManagementCoordinator::importRequested, this, [this](qulonglong folderId) {
        updateFolder(foldersModel->getIndexFromFolderId(folderId));
    });
    connect(comicManagementCoordinator, &ComicManagementCoordinator::currentComicViewUpdateRequested, contentViewsManager, &YACReaderContentViewsManager::updateCurrentComicView);
    connect(comicManagementCoordinator, &ComicManagementCoordinator::currentSourceRefreshStarted, navigationController, &YACReaderNavigationController::beginCurrentSourceRefresh);
    connect(comicManagementCoordinator, &ComicManagementCoordinator::currentSourceRefreshAccepted, navigationController, &YACReaderNavigationController::refreshCurrentSource);
    connect(comicManagementCoordinator, &ComicManagementCoordinator::currentSourceRefreshCancelled, navigationController, &YACReaderNavigationController::cancelCurrentSourceRefresh);
    connect(comicManagementCoordinator, &ComicManagementCoordinator::comicNumbersAssigned, this, [this](qint64 editedComicId) {
        navigationController->loadFolderContent(foldersModelProxy->mapToSource(foldersView->currentIndex()));

        const auto editedComic = comicsModel->getIndexFromId(editedComicId);
        if (editedComic.isValid()) {
            contentViewsManager->comicsView->scrollTo(editedComic, QAbstractItemView::PositionAtCenter);
            contentViewsManager->comicsView->setCurrentIndex(editedComic);
        }
    });
    connect(comicManagementCoordinator, &ComicManagementCoordinator::comicDeletionFinished, this, &LibraryWindow::checkEmptyFolder);
    folderManagementCoordinator = new FolderManagementCoordinator(foldersModel, this);
    connect(folderManagementCoordinator, &FolderManagementCoordinator::folderDeletionFailed, this, &LibraryWindow::errorDeletingFolder);
    connect(folderManagementCoordinator, &FolderManagementCoordinator::folderDeletionFinished, navigationController, &YACReaderNavigationController::reselectCurrentFolder);
    libraryDatabaseMaintenanceCoordinator = new LibraryDatabaseMaintenanceCoordinator(this);
    connect(libraryDatabaseMaintenanceCoordinator, &LibraryDatabaseMaintenanceCoordinator::backupAvailabilityChanged, actions.backupLibraryAction, &QAction::setEnabled);
    connect(libraryDatabaseMaintenanceCoordinator, &LibraryDatabaseMaintenanceCoordinator::maintenanceStarted, this, [this] {
        contentViewsManager->comicsView->setModel(nullptr);
        foldersView->setModel(nullptr);
        listsView->setModel(nullptr);
        actions.disableAllActions();
    });
    connect(libraryDatabaseMaintenanceCoordinator, &LibraryDatabaseMaintenanceCoordinator::libraryReloadRequested, this, &LibraryWindow::loadLibrary);
    connect(libraryDatabaseMaintenanceCoordinator, &LibraryDatabaseMaintenanceCoordinator::libraryUpdateRequested, this, &LibraryWindow::updateLibrary);
    connect(libraryDatabaseMaintenanceCoordinator, &LibraryDatabaseMaintenanceCoordinator::invalidDatabaseRestoreCancelled, this, [this] {
        actions.renameLibraryAction->setEnabled(true);
        actions.removeLibraryAction->setEnabled(true);
        actions.restoreLibraryAction->setEnabled(true);
    });
    connect(libraryDatabaseMaintenanceCoordinator, &LibraryDatabaseMaintenanceCoordinator::databaseUnavailableAfterRestore, this, [this] {
        actions.restoreLibraryAction->setEnabled(true);
        actions.removeLibraryAction->setEnabled(true);
    });
    connect(libraryDatabaseMaintenanceCoordinator, &LibraryDatabaseMaintenanceCoordinator::databaseSalvageFailed, this, [this] {
        actions.restoreLibraryAction->setEnabled(true);
    });
    libraryRepairCoordinator = new LibraryRepairCoordinator(settings, this);
    connect(libraryRepairCoordinator, &LibraryRepairCoordinator::repairStarted, importWidget, &ImportWidget::setRepairLook);
    connect(libraryRepairCoordinator, &LibraryRepairCoordinator::repairStarted, this, &LibraryWindow::showImportingWidget);
    connect(libraryRepairCoordinator, &LibraryRepairCoordinator::repairFinished, this, &LibraryWindow::showRootWidget);
    connect(libraryRepairCoordinator, &LibraryRepairCoordinator::repairFinished, this, &LibraryWindow::reloadCurrentLibrary);
    connect(libraryRepairCoordinator, &LibraryRepairCoordinator::comicProcessed, importWidget, &ImportWidget::newComic);
    connect(libraryRepairCoordinator, &LibraryRepairCoordinator::databaseRecoveryRequested, this, &LibraryWindow::offerDatabaseRecovery);
    libraryManagementCoordinator = new LibraryManagementCoordinator(settings, libraries, this);
    connect(libraryManagementCoordinator, &LibraryManagementCoordinator::loadStarted, this, [this] {
        historyController->clear();
        showRootWidget();
    });
    connect(libraryManagementCoordinator, &LibraryManagementCoordinator::libraryReady, this, &LibraryWindow::applyLoadedLibrary);
    connect(libraryManagementCoordinator, &LibraryManagementCoordinator::libraryManagementOnlyRequested, this, &LibraryWindow::showLibraryManagementOnly);
    connect(libraryManagementCoordinator, &LibraryManagementCoordinator::databaseRecoveryRequested, this, &LibraryWindow::offerDatabaseRecovery);
    connect(libraryManagementCoordinator, &LibraryManagementCoordinator::upgradeStarted, importWidget, &ImportWidget::setUpgradeLook);
    connect(libraryManagementCoordinator, &LibraryManagementCoordinator::upgradeStarted, this, &LibraryWindow::showImportingWidget);
    connect(libraryManagementCoordinator, &LibraryManagementCoordinator::libraryReloadRequested, this, &LibraryWindow::loadLibrary);
    connect(libraryManagementCoordinator, &LibraryManagementCoordinator::libraryRecreationRequested, createLibraryDialog, &CreateLibraryDialog::setDataAndStart);
    connect(libraryManagementCoordinator, &LibraryManagementCoordinator::openingError, this, &LibraryWindow::manageOpeningLibraryError);
    connect(libraryManagementCoordinator, &LibraryManagementCoordinator::creationStarted, importWidget, &ImportWidget::setImportLook);
    connect(libraryManagementCoordinator, &LibraryManagementCoordinator::creationStarted, this, &LibraryWindow::showImportingWidget);
    connect(libraryManagementCoordinator, &LibraryManagementCoordinator::updateStarted, importWidget, &ImportWidget::setUpdateLook);
    connect(libraryManagementCoordinator, &LibraryManagementCoordinator::updateStarted, this, &LibraryWindow::showImportingWidget);
    connect(libraryManagementCoordinator, &LibraryManagementCoordinator::operationUiResetRequested, this, &LibraryWindow::showRootWidget);
    connect(libraryManagementCoordinator, &LibraryManagementCoordinator::operationFinished, this, &LibraryWindow::showRootWidget);
    connect(libraryManagementCoordinator, &LibraryManagementCoordinator::currentLibraryReloadRequested, this, &LibraryWindow::reloadCurrentLibrary);
    connect(libraryManagementCoordinator, &LibraryManagementCoordinator::libraryAdded, this, &LibraryWindow::addLibraryToSelector);
    connect(libraryManagementCoordinator, &LibraryManagementCoordinator::libraryRemoved, this, &LibraryWindow::handleLibraryRemoved);
    connect(libraryManagementCoordinator, &LibraryManagementCoordinator::folderUpdateFinished, this, [this](qulonglong folderId) {
        reloadAfterCopyMove(foldersModel->getIndexFromFolderId(folderId));
    });
    connect(libraryManagementCoordinator, &LibraryManagementCoordinator::comicAdded, importWidget, &ImportWidget::newComic);
    connect(libraryManagementCoordinator, &LibraryManagementCoordinator::creationFailed, this, &LibraryWindow::manageCreatingError);
    connect(libraryManagementCoordinator, &LibraryManagementCoordinator::updateFailed, this, &LibraryWindow::manageUpdatingError);

    auto canStartUpdateProvider = [this]() {
        return comicVineDialog->isVisible() == false &&
                propertiesDialog->isVisible() == false;
    };
    librariesUpdateCoordinator = new LibrariesUpdateCoordinator(settings, libraries, canStartUpdateProvider, this);

    // Allow HTTP requests (e.g. the WebUI "Update now" button) to trigger updates.
    Static::librariesUpdateCoordinator = librariesUpdateCoordinator;

    connect(librariesUpdateCoordinator, &LibrariesUpdateCoordinator::updateStarted, sideBar->librariesTitle, &YACReaderTitledToolBar::showBusyIndicator);
    connect(librariesUpdateCoordinator, &LibrariesUpdateCoordinator::updateEnded, sideBar->librariesTitle, &YACReaderTitledToolBar::hideBusyIndicator);

    connect(librariesUpdateCoordinator, &LibrariesUpdateCoordinator::updateStarted, this, [=, this]() {
        actions.disableAllActions();
    });
    connect(librariesUpdateCoordinator, &LibrariesUpdateCoordinator::updateEnded, this, &LibraryWindow::reloadCurrentLibrary);

    librariesUpdateCoordinator->init();

    connect(sideBar->librariesTitle, &YACReaderTitledToolBar::cancelOperationRequested, librariesUpdateCoordinator, &LibrariesUpdateCoordinator::cancel);
}

bool LibraryWindow::hasLoadedLibraryModels() const
{
    return foldersView->model() == foldersModelProxy &&
            listsView->model() == listsModelProxy &&
            foldersModelProxy->sourceModel() == foldersModel &&
            listsModelProxy->sourceModel() == listsModel;
}

void LibraryWindow::createToolBars()
{

#ifdef Y_MAC_UI
    // libraryToolBar->setIconSize(QSize(16,16)); //TODO make icon size dynamic

    libraryToolBar->addAction(actions.backAction);
    libraryToolBar->addAction(actions.forwardAction);

    libraryToolBar->addSpace(10);

#ifdef SERVER_RELEASE
    libraryToolBar->addAction(actions.serverConfigAction);
#endif
    libraryToolBar->addAction(actions.optionsAction);
    libraryToolBar->addAction(actions.helpAboutAction);

    libraryToolBar->addSpace(10);

    libraryToolBar->addAction(actions.toggleComicsViewAction);

    libraryToolBar->addStretch();

    // Native toolbar search edit
    // libraryToolBar->addWidget(searchEdit);
    searchEdit = libraryToolBar->addSearchEdit();
    // connect(libraryToolBar,SIGNAL(searchTextChanged(YACReader::SearchModifiers,QString)),this,SLOT(setSearchFilter(YACReader::SearchModifiers, QString)));

    // libraryToolBar->setMovable(false);

    libraryToolBar->attachToWindow(this);

#else
    libraryToolBar->backButton->setDefaultAction(actions.backAction);
    libraryToolBar->forwardButton->setDefaultAction(actions.forwardAction);
    libraryToolBar->settingsButton->setDefaultAction(actions.optionsAction);
    libraryToolBar->serverButton->setDefaultAction(actions.serverConfigAction);
    libraryToolBar->helpButton->setDefaultAction(actions.helpAboutAction);
    libraryToolBar->toggleComicsViewButton->setDefaultAction(actions.toggleComicsViewAction);
#ifndef Q_OS_MACOS
    libraryToolBar->fullscreenButton->setDefaultAction(actions.toggleFullScreenAction);
#endif
    libraryToolBar->setSearchWidget(searchEdit);
#endif

    auto *searchMenu = createSearchMenu();
#ifdef Y_MAC_UI
    libraryToolBar->setSearchMenu(searchMenu);
#else
    searchEdit->setSearchMenu(searchMenu);
#endif

    editInfoToolBar->setIconSize(QSize(18, 18));
    editInfoToolBar->addAction(actions.openComicAction);
    editInfoToolBar->addSeparator();
    editInfoToolBar->addAction(actions.editSelectedComicsAction);
    editInfoToolBar->addAction(actions.getInfoAction);
    editInfoToolBar->addAction(actions.asignOrderAction);

    editInfoToolBar->addSeparator();

    editInfoToolBar->addAction(actions.selectAllComicsAction);

    editInfoToolBar->addSeparator();

    editInfoToolBar->addAction(actions.setAsReadAction);
    editInfoToolBar->addAction(actions.setAsNonReadAction);

    editInfoToolBar->addAction(actions.showHideMarksAction);

    editInfoToolBar->addSeparator();

    auto setTypeToolButton = new QToolButton();
    setTypeToolButton->addAction(actions.setNormalAction);
    setTypeToolButton->addAction(actions.setMangaAction);
    setTypeToolButton->addAction(actions.setWesternMangaAction);
    setTypeToolButton->addAction(actions.setWebComicAction);
    setTypeToolButton->addAction(actions.setYonkomaAction);
    setTypeToolButton->setPopupMode(QToolButton::InstantPopup);
    setTypeToolButton->setDefaultAction(actions.setNormalAction);
    editInfoToolBar->addWidget(setTypeToolButton);

    editInfoToolBar->addSeparator();

    editInfoToolBar->addAction(actions.deleteComicsAction);

    comicToolbarEntries = editInfoToolBar->actions();

    auto toolBarStretch = new YACReaderToolBarStretch(this);
    comicToolbarEndAnchor = editInfoToolBar->addWidget(toolBarStretch);

    editInfoToolBar->addAction(actions.toogleShowRecentIndicatorAction);

    contentViewsManager->comicsView->setToolBar(editInfoToolBar);
}

QMenu *LibraryWindow::createSearchMenu()
{
    auto *menu = new QMenu(tr("Search filters"), this);
    menu->setMinimumWidth(190);

    auto addFilter = [this, menu](const QString &label, const QString &query) {
        auto *action = menu->addAction(label);
        connect(action, &QAction::triggered, this, [this, query] {
            applySearchQuery(query);
        });
    };

    addFilter(tr("Unread"), QStringLiteral("read:false"));
    addFilter(
            tr("In progress"),
            QStringLiteral("hasBeenOpened:true AND read:false"));
    addFilter(tr("Highly rated"), QStringLiteral("rating>=4"));

    auto *recentlyAdded = menu->addAction(tr("Recently added"));
    connect(recentlyAdded, &QAction::triggered, this, [this] {
        const int days = settings->value(NUM_DAYS_TO_CONSIDER_RECENT, 1).toInt();
        applySearchQuery(QStringLiteral("added>%1").arg(days));
    });

    menu->addSeparator();
    auto *syntaxAction = menu->addAction(tr("Search syntax…"));
    connect(syntaxAction, &QAction::triggered, this, &LibraryWindow::showSearchSyntax);

    return menu;
}

void LibraryWindow::applySearchQuery(const QString &query)
{
#ifdef Y_MAC_UI
    libraryToolBar->setSearchText(query);
    libraryToolBar->focusSearch();
#else
    searchEdit->setText(query);
    searchEdit->setFocus(Qt::ShortcutFocusReason);
#endif
}

void LibraryWindow::setSearchInputEnabled(bool enabled)
{
#ifdef Y_MAC_UI
    libraryToolBar->setSearchEnabled(enabled);
#else
    searchEdit->setEnabled(enabled);
#endif
}

void LibraryWindow::clearSearchInput(bool notify)
{
#ifdef Y_MAC_UI
    libraryToolBar->clearSearchText(notify);
#else
    if (notify)
        searchEdit->clear();
    else
        searchEdit->clearText();
#endif
}

void LibraryWindow::focusSearchInput()
{
#ifdef Y_MAC_UI
    libraryToolBar->focusSearch();
#else
    searchEdit->setFocus(Qt::ShortcutFocusReason);
#endif
}

QString LibraryWindow::searchText() const
{
#ifdef Y_MAC_UI
    return libraryToolBar->searchText();
#else
    return searchEdit->text();
#endif
}

void LibraryWindow::showSearchSyntax()
{
    auto *dialog = new SearchSyntaxDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
}

void LibraryWindow::createMenus()
{
    foldersView->addAction(actions.addFolderAction);
    foldersView->addAction(actions.renameFolderAction);
    foldersView->addAction(actions.deleteFolderAction);
    YACReader::addSperator(foldersView);

    foldersView->addAction(actions.openContainingFolderAction);
    foldersView->addAction(actions.updateFolderAction);
    YACReader::addSperator(foldersView);

    foldersView->addAction(actions.setFolderAsNotCompletedAction);
    foldersView->addAction(actions.setFolderAsCompletedAction);
    YACReader::addSperator(foldersView);

    foldersView->addAction(actions.setFolderAsReadAction);
    foldersView->addAction(actions.setFolderAsUnreadAction);
    YACReader::addSperator(foldersView);

    foldersView->addAction(actions.setFolderAsNormalAction);
    foldersView->addAction(actions.setFolderAsMangaAction);
    foldersView->addAction(actions.setFolderAsWesternMangaAction);
    foldersView->addAction(actions.setFolderAsWebComicAction);
    foldersView->addAction(actions.setFolderAsYonkomaAction);
    YACReader::addSperator(foldersView);

    foldersView->addAction(actions.setFolderCoverAction);
    foldersView->addAction(actions.deleteCustomFolderCoverAction);

    selectedLibrary->addAction(actions.updateLibraryAction);
    selectedLibrary->addAction(actions.renameLibraryAction);
    selectedLibrary->addAction(actions.removeLibraryAction);
    YACReader::addSperator(selectedLibrary);

    auto setNormalAction = new QAction();
    setNormalAction->setText(tr("comic"));

    auto setMangaAction = new QAction();
    setMangaAction->setText(tr("manga"));

    auto setWesternMangaAction = new QAction();
    setWesternMangaAction->setText(tr("western manga (left to right)"));

    auto setWebComicAction = new QAction();
    setWebComicAction->setText(tr("web comic"));

    auto setYonkomaAction = new QAction();
    setYonkomaAction->setText(tr("4koma (top to botom)"));

    setNormalAction->setCheckable(true);
    setMangaAction->setCheckable(true);
    setWesternMangaAction->setCheckable(true);
    setWebComicAction->setCheckable(true);
    setYonkomaAction->setCheckable(true);

    auto setupActions = [=](FileType type) {
        setNormalAction->setChecked(false);
        setMangaAction->setChecked(false);
        setWesternMangaAction->setChecked(false);
        setWebComicAction->setChecked(false);
        setYonkomaAction->setChecked(false);

        switch (type) {
        case YACReader::FileType::Comic:
            setNormalAction->setChecked(true);
            break;
        case YACReader::FileType::Manga:
            setMangaAction->setChecked(true);
            break;
        case YACReader::FileType::WesternManga:
            setWesternMangaAction->setChecked(true);
            break;
        case YACReader::FileType::WebComic:
            setWebComicAction->setChecked(true);
            break;
        case YACReader::FileType::Yonkoma:
            setYonkomaAction->setChecked(true);
            break;
        }
    };

    connect(setNormalAction, &QAction::triggered, this, [=]() { setCurrentLibraryAs(FileType::Comic); });
    connect(setMangaAction, &QAction::triggered, this, [=]() { setCurrentLibraryAs(FileType::Manga); });
    connect(setWesternMangaAction, &QAction::triggered, this, [=]() { setCurrentLibraryAs(FileType::WesternManga); });
    connect(setWebComicAction, &QAction::triggered, this, [=]() { setCurrentLibraryAs(FileType::WebComic); });
    connect(setYonkomaAction, &QAction::triggered, this, [=]() { setCurrentLibraryAs(FileType::Yonkoma); });

    auto typeMenu = new QMenu(tr("Set type"), selectedLibrary);

    connect(typeMenu, &QMenu::aboutToShow, this, [=]() {
        auto folder = foldersModel->getRootFolder();
        setupActions(folder.type);
    });

    selectedLibrary->addAction(typeMenu->menuAction());
    YACReader::addSperator(selectedLibrary);
    typeMenu->addAction(setNormalAction);
    typeMenu->addAction(setMangaAction);
    typeMenu->addAction(setWesternMangaAction);
    typeMenu->addAction(setWebComicAction);
    typeMenu->addAction(setYonkomaAction);

    selectedLibrary->addAction(actions.rescanLibraryForXMLInfoAction);
    selectedLibrary->addAction(actions.repairLibraryAction);
    YACReader::addSperator(selectedLibrary);

    selectedLibrary->addAction(actions.backupLibraryAction);
    selectedLibrary->addAction(actions.restoreLibraryAction);
    YACReader::addSperator(selectedLibrary);

    selectedLibrary->addAction(actions.exportComicsInfoAction);
    selectedLibrary->addAction(actions.importComicsInfoAction);
    YACReader::addSperator(selectedLibrary);

    selectedLibrary->addAction(actions.exportLibraryAction);
    selectedLibrary->addAction(actions.importLibraryAction);
    YACReader::addSperator(selectedLibrary);

    selectedLibrary->addAction(actions.openLibraryFolderAction);
    selectedLibrary->addAction(actions.showLibraryInfo);

// MacOSX app menus
#ifdef Q_OS_MACOS
    QMenuBar *menu = this->menuBar();
    // about / preferences
    // TODO

    // library
    QMenu *libraryMenu = new QMenu(tr("Library"));

    libraryMenu->addAction(actions.updateLibraryAction);
    libraryMenu->addAction(actions.renameLibraryAction);
    libraryMenu->addAction(actions.removeLibraryAction);
    libraryMenu->addSeparator();

    libraryMenu->addMenu(typeMenu);
    libraryMenu->addSeparator();

    libraryMenu->addAction(actions.rescanLibraryForXMLInfoAction);
    libraryMenu->addAction(actions.repairLibraryAction);
    libraryMenu->addSeparator();

    libraryMenu->addAction(actions.backupLibraryAction);
    libraryMenu->addAction(actions.restoreLibraryAction);
    libraryMenu->addSeparator();

    libraryMenu->addAction(actions.exportComicsInfoAction);
    libraryMenu->addAction(actions.importComicsInfoAction);

    libraryMenu->addSeparator();

    libraryMenu->addAction(actions.exportLibraryAction);
    libraryMenu->addAction(actions.importLibraryAction);

    libraryMenu->addSeparator();

    libraryMenu->addAction(actions.openLibraryFolderAction);
    libraryMenu->addAction(actions.showLibraryInfo);

    // folder
    QMenu *folderMenu = new QMenu(tr("Folder"));
    folderMenu->addAction(actions.openContainingFolderAction);
    folderMenu->addAction(actions.renameFolderAction);
    folderMenu->addAction(actions.updateFolderAction);
    folderMenu->addSeparator();
    folderMenu->addAction(actions.rescanXMLFromCurrentFolderAction);
    folderMenu->addSeparator();
    folderMenu->addAction(actions.setFolderAsNotCompletedAction);
    folderMenu->addAction(actions.setFolderAsCompletedAction);
    folderMenu->addSeparator();
    folderMenu->addAction(actions.setFolderAsReadAction);
    folderMenu->addAction(actions.setFolderAsUnreadAction);
    folderMenu->addSeparator();
    folderMenu->addAction(actions.setFolderAsNormalAction);
    folderMenu->addAction(actions.setFolderAsMangaAction);
    folderMenu->addAction(actions.setFolderAsWesternMangaAction);
    folderMenu->addAction(actions.setFolderAsWebComicAction);
    folderMenu->addAction(actions.setFolderAsYonkomaAction);
    folderMenu->addSeparator();
    folderMenu->addAction(actions.setFolderCoverAction);
    folderMenu->addAction(actions.deleteCustomFolderCoverAction);

    // comic
    QMenu *comicMenu = new QMenu(tr("Comic"));
    comicMenu->addAction(actions.openContainingFolderComicAction);
    comicMenu->addSeparator();
    comicMenu->addAction(actions.resetComicRatingAction);

    menu->addMenu(libraryMenu);
    menu->addMenu(folderMenu);
    menu->addMenu(comicMenu);
#endif
}

void LibraryWindow::createConnections()
{
    actions.createConnections(
            historyController,
            navigationController,
            this,
            had,
            exportLibraryDialog,
            contentViewsManager,
            editShortcutsDialog,
            foldersView,
            optionsDialog,
            serverConfigDialog,
            recentVisibilityCoordinator,
            comicManagementCoordinator);
    connect(actions.focusSearchLineAction, &QAction::triggered, this, &LibraryWindow::focusSearchInput);

    connect(createLibraryDialog, &CreateLibraryDialog::createLibrary, libraryManagementCoordinator, &LibraryManagementCoordinator::createLibrary);
    connect(createLibraryDialog, &CreateLibraryDialog::libraryExists, libraryManagementCoordinator, &LibraryManagementCoordinator::showLibraryAlreadyExists);
    connect(importComicsInfoDialog, &QDialog::finished, this, &LibraryWindow::reloadCurrentLibrary);

    connect(xmlInfoLibraryScanner, &QThread::finished, this, &LibraryWindow::showRootWidget);
    connect(xmlInfoLibraryScanner, &QThread::finished, this, &LibraryWindow::reloadCurrentFolderComicsContent);
    connect(xmlInfoLibraryScanner, &XMLInfoLibraryScanner::comicScanned, importWidget, &ImportWidget::newComic);

    // new import widget
    connect(importWidget, &ImportWidget::stop, libraryManagementCoordinator, &LibraryManagementCoordinator::stop);
    connect(importWidget, &ImportWidget::stop, this, &LibraryWindow::stopXMLScanning);
    connect(importWidget, &ImportWidget::stop, libraryRepairCoordinator, &LibraryRepairCoordinator::stop);

    // packageManager connections
    connect(exportLibraryDialog, &ExportLibraryDialog::exportPath, this, &LibraryWindow::exportLibrary);
    connect(exportLibraryDialog, &QDialog::rejected, packageManager, &PackageManager::cancel);
    connect(packageManager, &PackageManager::exported, exportLibraryDialog, &ExportLibraryDialog::close);
    connect(importLibraryDialog, &ImportLibraryDialog::unpackCLC, this, &LibraryWindow::importLibrary);
    connect(importLibraryDialog, &QDialog::rejected, packageManager, &PackageManager::cancel);
    connect(importLibraryDialog, &QDialog::rejected, this, &LibraryWindow::deleteCurrentLibrary);
    connect(importLibraryDialog, &ImportLibraryDialog::libraryExists, libraryManagementCoordinator, &LibraryManagementCoordinator::showLibraryAlreadyExists);
    connect(packageManager, &PackageManager::imported, importLibraryDialog, &QWidget::hide);
    connect(packageManager, &PackageManager::imported, libraryManagementCoordinator, &LibraryManagementCoordinator::finishAddingLibrary);
    connect(packageManager, &PackageManager::failed, this, [this](const QString &error) {
        QMessageBox::critical(this, tr("Package operation failed"), error.isEmpty() ? tr("The covers package operation could not be completed.") : error);
    });

    // create and update dialogs
    connect(createLibraryDialog, &CreateLibraryDialog::cancelCreate, libraryManagementCoordinator, &LibraryManagementCoordinator::stop);

    // open existing library from dialog.
    connect(addLibraryDialog, &AddLibraryDialog::addLibrary, libraryManagementCoordinator, &LibraryManagementCoordinator::addExistingLibrary);

    // load library when selected library changes
    connect(selectedLibrary, &YACReaderLibraryListWidget::currentIndexChanged, this, &LibraryWindow::loadLibrary);

    // rename library dialog
    connect(renameLibraryDialog, &RenameLibraryDialog::renameLibrary, this, &LibraryWindow::rename);

    // navigations between view modes (tree,list and flow)
    // TODO connect(foldersView, SIGNAL(pressed(QModelIndex)), this, SLOT(updateFoldersViewConextMenu(QModelIndex)));
    // connect(foldersView, SIGNAL(clicked(QModelIndex)), this, SLOT(loadCovers(QModelIndex)));

    // drops in folders view
    connect(foldersView, QOverload<QList<QPair<QString, QString>>, QModelIndex>::of(&YACReaderFoldersView::copyComicsToFolder),
            this, &LibraryWindow::copyAndImportComicsToFolder);
    connect(foldersView, QOverload<QList<QPair<QString, QString>>, QModelIndex>::of(&YACReaderFoldersView::moveComicsToFolder),
            this, &LibraryWindow::moveAndImportComicsToFolder);
    connect(foldersView, &QWidget::customContextMenuRequested, this, &LibraryWindow::showFoldersContextMenu);

    // comic vine
    connect(comicVineDialog, &QDialog::accepted, navigationController, &YACReaderNavigationController::refreshCurrentSource, Qt::QueuedConnection);
    connect(comicVineDialog, &QDialog::rejected, navigationController, &YACReaderNavigationController::cancelCurrentSourceRefresh);

    connect(optionsDialog, &YACReaderOptionsDialog::optionsChanged, this, &LibraryWindow::reloadOptions);
    connect(optionsDialog, &YACReaderOptionsDialog::editShortcuts, editShortcutsDialog, &QWidget::show);

    auto searchDebouncer = new KDToolBox::KDStringSignalDebouncer(this);
    searchDebouncer->setTimeout(400);

// Search filter
#ifdef Y_MAC_UI
    connect(libraryToolBar, &YACReaderMacOSXToolbar::filterChanged, searchDebouncer, &KDToolBox::KDStringSignalDebouncer::throttle);
    connect(searchDebouncer, &KDToolBox::KDStringSignalDebouncer::triggered, this, [=](QString filter) {
        setSearchFilter(filter);
    });
#else
    connect(searchEdit, &YACReaderSearchLineEdit::filterChanged, searchDebouncer, &KDToolBox::KDStringSignalDebouncer::throttle);
    connect(searchDebouncer, &KDToolBox::KDStringSignalDebouncer::triggered, this, [=](QString filter) {
        setSearchFilter(filter);
    });
#endif
    connect(&comicQueryResultProcessor, &ComicQueryResultProcessor::newData, this, &LibraryWindow::setComicSearchFilterData);
    qRegisterMetaType<FolderItem *>("FolderItem *");
    qRegisterMetaType<QMap<unsigned long long int, FolderItem *> *>("QMap<unsigned long long int, FolderItem *> *");
    connect(folderQueryResultProcessor.get(), &FolderQueryResultProcessor::newData, this, &LibraryWindow::setFolderSearchFilterData);

    connect(listsModel, &ReadingListModel::addComicsToFavorites, comicsModel, QOverload<const QList<qulonglong> &>::of(&ComicModel::addComicsToFavorites));
    connect(listsModel, &ReadingListModel::addComicsToLabel, comicsModel, QOverload<const QList<qulonglong> &, qulonglong>::of(&ComicModel::addComicsToLabel));
    connect(listsModel, &ReadingListModel::addComicsToReadingList, comicsModel, QOverload<const QList<qulonglong> &, qulonglong>::of(&ComicModel::addComicsToReadingList));
    //--
}

void LibraryWindow::setCurrentLibraryAs(FileType fileType)
{
    foldersModel->updateTreeType(fileType);
}

void LibraryWindow::loadLibrary(const QString &name)
{
    if (libraries.isEmpty()) {
        actions.disableAllActions();
        showNoLibrariesWidget();
        return;
    }

    libraryManagementCoordinator->loadLibrary(name, libraries.getPath(name));
}

void LibraryWindow::applyLoadedLibrary(const QString &libraryDataPath, bool readOnly)
{
    foldersModel->setupModelData(libraryDataPath);
    foldersModelProxy->setSourceModel(foldersModel);
    foldersView->setModel(foldersModelProxy);
    foldersView->setCurrentIndex(QModelIndex()); // By default this can return an arbitrary index.

    listsModel->setupReadingListsData(libraryDataPath);
    listsModelProxy->setSourceModel(listsModel);
    listsView->setModel(listsModelProxy);

    actions.disableFoldersActions(foldersModel->rowCount(QModelIndex()) == 0);
    actions.disableLibrariesActions(false);

    if (readOnly) {
        actions.updateLibraryAction->setDisabled(true);
        actions.repairLibraryAction->setDisabled(true);
        actions.openContainingFolderAction->setDisabled(true);
        actions.rescanLibraryForXMLInfoAction->setDisabled(true);

        setComicActionsDisabled(true);
#ifndef Q_OS_MACOS
        actions.toggleFullScreenAction->setEnabled(true);
#endif
    }
    importedCovers = readOnly;

    setRootIndex();
    clearSearchInput(true);
}

void LibraryWindow::showLibraryManagementOnly()
{
    contentViewsManager->comicsView->setModel(nullptr);
    foldersView->setModel(nullptr);
    listsView->setModel(nullptr);
    actions.disableAllActions();
    actions.renameLibraryAction->setEnabled(true);
    actions.removeLibraryAction->setEnabled(true);
    actions.restoreLibraryAction->setEnabled(true);
}

void LibraryWindow::loadCoversFromCurrentModel()
{
    contentViewsManager->comicsView->setModel(comicsModel);
}

void LibraryWindow::copyAndImportComicsToCurrentFolder(const QList<QPair<QString, QString>> &comics)
{
    const QModelIndex destinationFolder = getCurrentFolderIndex();
    comicManagementCoordinator->copyAndImportComics(comics, currentFolderPath(), destinationFolder.data(FolderModel::IdRole).toULongLong());
}

void LibraryWindow::moveAndImportComicsToCurrentFolder(const QList<QPair<QString, QString>> &comics)
{
    const QModelIndex destinationFolder = getCurrentFolderIndex();
    comicManagementCoordinator->moveAndImportComics(comics, currentFolderPath(), destinationFolder.data(FolderModel::IdRole).toULongLong());
}

void LibraryWindow::copyAndImportComicsToFolder(const QList<QPair<QString, QString>> &comics, const QModelIndex &miFolder)
{
    const QModelIndex folderDestination = foldersModelProxy->mapToSource(miFolder);
    const QString destinationPath = QDir::cleanPath(currentPath() + foldersModel->getFolderPath(folderDestination));
    comicManagementCoordinator->copyAndImportComics(comics, destinationPath, folderDestination.data(FolderModel::IdRole).toULongLong());
}

void LibraryWindow::moveAndImportComicsToFolder(const QList<QPair<QString, QString>> &comics, const QModelIndex &miFolder)
{
    const QModelIndex folderDestination = foldersModelProxy->mapToSource(miFolder);
    const QString destinationPath = QDir::cleanPath(currentPath() + foldersModel->getFolderPath(folderDestination));
    comicManagementCoordinator->moveAndImportComics(comics, destinationPath, folderDestination.data(FolderModel::IdRole).toULongLong());
}

void LibraryWindow::updateCurrentFolder()
{
    updateFolder(getCurrentFolderIndex());
}

void LibraryWindow::updateFolder(const QModelIndex &miFolder)
{
    QLOG_DEBUG() << "UPDATE FOLDER!!!!";

    importWidget->setUpdateLook();
    showImportingWidget();

    const auto libraryName = selectedLibrary->currentText();
    const auto libraryPath = QDir::cleanPath(libraries.getPath(libraryName));
    libraryManagementCoordinator->updateFolder(
            libraryName,
            libraryPath,
            QDir::cleanPath(currentPath() + foldersModel->getFolderPath(miFolder)),
            miFolder.data(FolderModel::IdRole).toULongLong());
}

void LibraryWindow::reloadCurrentFolderComicsContent()
{
    navigationController->loadFolderContent(getCurrentFolderIndex());

    enableNeededActions();
}

void LibraryWindow::reloadAfterCopyMove(const QModelIndex &mi)
{
    if (getCurrentFolderIndex() == mi) {
        auto item = static_cast<FolderItem *>(mi.internalPointer());

        if (item == nullptr) {
            foldersModel->reload();
        } else {
            foldersModel->reload(mi);
        }

        navigationController->refreshCurrentSource();
    }

    enableNeededActions();
}

QModelIndex LibraryWindow::getCurrentFolderIndex()
{
    if (!hasLoadedLibraryModels())
        return QModelIndex();

    if (foldersView->selectionModel()->selectedRows().length() > 0)
        return foldersModelProxy->mapToSource(foldersView->currentIndex());
    else
        return QModelIndex();
}

void LibraryWindow::enableNeededActions()
{
    if (foldersModel->rowCount(QModelIndex()) > 0)
        actions.disableFoldersActions(false);

    if (comicsModel->rowCount() > 0)
        setComicActionsDisabled(false);

    actions.disableLibrariesActions(false);
}

void LibraryWindow::setComicActionsDisabled(bool disabled)
{
    if (!disabled && librariesUpdateCoordinator->isRunning()) {
        setComicActionsDisabled(true);
        return;
    }

    actions.setComicActionsDisabled(disabled);
    setComicToolbarEntriesVisible(comicsModel != nullptr && comicsModel->rowCount() > 0);
}

void LibraryWindow::setComicToolbarEntriesVisible(bool visible)
{
    if (editInfoToolBar == nullptr || comicToolbarEndAnchor == nullptr)
        return;

    const auto currentActions = editInfoToolBar->actions();
    for (auto *action : comicToolbarEntries) {
        if (visible && !currentActions.contains(action))
            editInfoToolBar->insertAction(comicToolbarEndAnchor, action);
        else if (!visible && currentActions.contains(action))
            editInfoToolBar->removeAction(action);
    }
}

void LibraryWindow::addFolderToCurrentIndex()
{
    exitSearchMode(); // Creating a folder in search mode is broken => exit it.

    const auto currentIndex = getCurrentFolderIndex();

    bool ok;
    const auto newFolderName = QInputDialog::getText(this, tr("Add new folder"),
                                                     tr("Folder name:"), QLineEdit::Normal,
                                                     "", &ok);

    if (ok) {
        const auto parentPath = QDir::cleanPath(currentPath() + foldersModel->getFolderPath(currentIndex));
        const auto newIndex = folderManagementCoordinator->createFolder(currentIndex, parentPath, newFolderName);
        if (newIndex.isValid()) {
            foldersView->setCurrentIndex(foldersModelProxy->mapFromSource(newIndex));
            navigationController->loadFolderContent(newIndex);
            historyController->updateHistory(YACReaderLibrarySourceContainer(newIndex, YACReaderLibrarySourceContainer::Folder));
        }
    }
}

void LibraryWindow::renameSelectedFolder()
{
    renameFolder(getCurrentFolderIndex());
}

void LibraryWindow::renameFolder(const QModelIndex &folder)
{
    if (!folder.isValid()) {
        QMessageBox::information(this, tr("No folder selected"), tr("Please, select a folder first"));
        return;
    }

    const auto oldName = folder.data(FolderModel::FolderNameRole).toString();
    bool accepted = false;
    const auto newName = QInputDialog::getText(this, tr("Rename folder"), tr("Folder name:"), QLineEdit::Normal, oldName, &accepted);
    if (!accepted || newName == oldName)
        return;

    const auto result = folderManagementCoordinator->renameFolder(folder, currentPath(), newName);
    switch (result.error) {
    case FolderManagementCoordinator::RenameError::None:
        navigationController->refreshCurrentSource();
        return;
    case FolderManagementCoordinator::RenameError::InvalidName:
        QMessageBox::warning(this, tr("Invalid folder name"), tr("The folder name is empty or contains characters that are not supported."));
        return;
    case FolderManagementCoordinator::RenameError::TargetAlreadyExists:
        QMessageBox::warning(this, tr("Unable to rename folder"), tr("A file or folder named '%1' already exists.").arg(newName));
        return;
    case FolderManagementCoordinator::RenameError::FileSystemRenameFailed:
        QMessageBox::critical(this, tr("Unable to rename folder"), tr("The folder could not be renamed on disk. Please check the folder name and write permissions.\n\nFolder: %1").arg(result.folderPath));
        return;
    case FolderManagementCoordinator::RenameError::DatabaseUpdateFailed:
    case FolderManagementCoordinator::RenameError::DatabaseUpdateAndRollbackFailed: {
        auto message = result.error == FolderManagementCoordinator::RenameError::DatabaseUpdateFailed
                ? tr("The library database could not be updated. The folder rename on disk was reverted.")
                : tr("The library database could not be updated, and the folder rename on disk could not be reverted. The library now needs to be updated manually.");
        if (!result.databaseError.isEmpty())
            message += "\n\n" + result.databaseError;
        QMessageBox::critical(this, tr("Unable to rename folder"), message);
        return;
    }
    }
}

void LibraryWindow::deleteSelectedFolder()
{
    QModelIndex currentIndex = getCurrentFolderIndex();
    QString relativePath = foldersModel->getFolderPath(currentIndex);
    QString folderPath = QDir::cleanPath(currentPath() + relativePath);

    if (!currentIndex.isValid())
        QMessageBox::information(this, tr("No folder selected"), tr("Please, select a folder first"));
    else {
        QString libraryPath = QDir::cleanPath(currentPath());
        if ((libraryPath == folderPath) || relativePath.isEmpty() || relativePath == "/")
            QMessageBox::critical(this, tr("Error in path"), tr("There was an error accessing the folder's path"));
        else {
            int ret = QMessageBox::question(this, tr("Delete folder"), tr("The selected folder and all its contents will be deleted from your disk. Are you sure?") + "\n\nFolder : " + folderPath, QMessageBox::Yes, QMessageBox::No);

            if (ret == QMessageBox::Yes) {
                // The unified grid observes the main folder model directly. Move
                // away from the folder before removing its model index so the
                // content view never retains the index being deleted.
                const QModelIndex parentIndex = currentIndex.parent();
                if (parentIndex.isValid())
                    foldersView->setCurrentIndex(foldersModelProxy->mapFromSource(parentIndex));
                else
                    setRootIndex();

                folderManagementCoordinator->deleteFolder(currentIndex, folderPath);
            }
        }
    }
}

void LibraryWindow::errorDeletingFolder()
{
    QMessageBox::critical(this, tr("Unable to delete"), tr("There was an issue trying to delete the selected folders. Please, check for write permissions and be sure that any applications are using these folders or any of the contained files."));
}

void LibraryWindow::addNewReadingList()
{
    QModelIndexList selectedLists = listsView->selectionModel()->selectedIndexes();
    QModelIndex sourceMI;
    if (!selectedLists.isEmpty())
        sourceMI = listsModelProxy->mapToSource(selectedLists.at(0));

    if (selectedLists.isEmpty() || !listsModel->isReadingSubList(sourceMI)) {
        bool ok;
        QString newListName = QInputDialog::getText(this, tr("Add new reading lists"),
                                                    tr("List name:"), QLineEdit::Normal,
                                                    "", &ok);
        if (ok) {
            if (selectedLists.isEmpty() || !listsModel->isReadingList(sourceMI))
                listsModel->addReadingList(newListName); // top level
            else {
                listsModel->addReadingListAt(newListName, sourceMI); // sublist
            }
        }
    }
}

void LibraryWindow::deleteSelectedReadingList()
{
    QModelIndexList selectedLists = listsView->selectionModel()->selectedIndexes();
    if (!selectedLists.isEmpty()) {
        QModelIndex mi = listsModelProxy->mapToSource(selectedLists.at(0));
        if (listsModel->isEditable(mi)) {
            int ret = QMessageBox::question(this, tr("Delete list/label"), tr("The selected item will be deleted, your comics or folders will NOT be deleted from your disk. Are you sure?"), QMessageBox::Yes, QMessageBox::No);
            if (ret == QMessageBox::Yes) {
                listsModel->deleteItem(mi);
                navigationController->reselectCurrentList();
            }
        }
    }
}

void LibraryWindow::showAddNewLabelDialog()
{
    auto dialog = new AddLabelDialog();
    int ret = dialog->exec();

    if (ret == QDialog::Accepted) {
        YACReader::LabelColors color = dialog->selectedColor();
        QString name = dialog->name();

        listsModel->addNewLabel(name, color);
    }
}

// TODO implement editors in treeview
void LibraryWindow::showRenameCurrentList()
{
    QModelIndexList selectedLists = listsView->selectionModel()->selectedIndexes();
    if (!selectedLists.isEmpty()) {
        QModelIndex mi = listsModelProxy->mapToSource(selectedLists.at(0));
        if (listsModel->isEditable(mi)) {
            bool ok;
            QString newListName = QInputDialog::getText(this, tr("Rename list name"),
                                                        tr("List name:"), QLineEdit::Normal,
                                                        listsModel->name(mi), &ok);

            if (ok)
                listsModel->rename(mi, newListName);
        }
    }
}

void LibraryWindow::addSelectedComicsToFavorites()
{
    QModelIndexList indexList = getSelectedComics();
    comicsModel->addComicsToFavorites(indexList);
}

void LibraryWindow::showComicsViewContextMenu(const QPoint &point)
{
    showComicsContextMenu(point, true);
}

void LibraryWindow::showComicsItemContextMenu(const QPoint &point)
{
    showComicsContextMenu(point, false);
}

void LibraryWindow::showComicsContextMenu(const QPoint &point, bool showFullScreenAction)
{
    auto selection = this->getSelectedComics();
    auto menu = new QMenu(this);
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);

    auto setNormalAction = new QAction(menu);
    setNormalAction->setText(tr("comic"));

    auto setMangaAction = new QAction(menu);
    setMangaAction->setText(tr("manga"));

    auto setWesternMangaAction = new QAction(menu);
    setWesternMangaAction->setText(tr("western manga (left to right)"));

    auto setWebComicAction = new QAction(menu);
    setWebComicAction->setText(tr("web comic"));

    auto setYonkomaAction = new QAction(menu);
    setYonkomaAction->setText(tr("4koma (top to botom)"));

    setNormalAction->setCheckable(true);
    setMangaAction->setCheckable(true);
    setWesternMangaAction->setCheckable(true);
    setWebComicAction->setCheckable(true);
    setYonkomaAction->setCheckable(true);

    connect(setNormalAction, &QAction::triggered, actions.setNormalAction, &QAction::trigger);
    connect(setMangaAction, &QAction::triggered, actions.setMangaAction, &QAction::trigger);
    connect(setWesternMangaAction, &QAction::triggered, actions.setWesternMangaAction, &QAction::trigger);
    connect(setWebComicAction, &QAction::triggered, actions.setWebComicAction, &QAction::trigger);
    connect(setYonkomaAction, &QAction::triggered, actions.setYonkomaAction, &QAction::trigger);

    auto setupActions = [=](FileType type) {
        switch (type) {
        case YACReader::FileType::Comic:
            setNormalAction->setChecked(true);
            break;
        case YACReader::FileType::Manga:
            setMangaAction->setChecked(true);
            break;
        case YACReader::FileType::WesternManga:
            setWesternMangaAction->setChecked(true);
            break;
        case YACReader::FileType::WebComic:
            setWebComicAction->setChecked(true);
            break;
        case YACReader::FileType::Yonkoma:
            setYonkomaAction->setChecked(true);
            break;
        }
    };

    if (selection.size() == 1) {
        QModelIndex index = selection.at(0);
        auto type = index.data(ComicModel::TypeRole).value<YACReader::FileType>();
        setupActions(type);
    }

    menu->addAction(actions.openComicAction);
    menu->addAction(actions.saveCoversToAction);
    menu->addSeparator();
    menu->addAction(actions.openContainingFolderComicAction);
    if (YACReader::FeatureFlags::organizeFiles)
        menu->addAction(actions.organizeComicsFilesAction);
    menu->addAction(actions.updateCurrentFolderAction);
    menu->addSeparator();
    menu->addAction(actions.editSelectedComicsAction);
    menu->addAction(actions.getInfoAction);
    menu->addAction(actions.asignOrderAction);
    menu->addSeparator();
    menu->addAction(actions.selectAllComicsAction);
    menu->addSeparator();
    menu->addAction(actions.setAsReadAction);
    menu->addAction(actions.setAsNonReadAction);
    menu->addSeparator();
    auto typeMenu = new QMenu(tr("Set type"), menu);
    menu->addMenu(typeMenu);
    typeMenu->addAction(setNormalAction);
    typeMenu->addAction(setMangaAction);
    typeMenu->addAction(setWesternMangaAction);
    typeMenu->addAction(setWebComicAction);
    typeMenu->addAction(setYonkomaAction);
    menu->addSeparator();
    menu->addAction(actions.resetComicRatingAction);
    menu->addSeparator();
    menu->addAction(actions.deleteMetadataAction);
    menu->addSeparator();
    menu->addAction(actions.deleteComicsAction);
    menu->addSeparator();
    menu->addAction(actions.addToMenuAction);
    auto subMenu = new QMenu(menu);
    setupAddToSubmenu(*subMenu);

#ifndef Q_OS_MACOS
    if (showFullScreenAction) {
        menu->addSeparator();
        menu->addAction(actions.toggleFullScreenAction);
    }
#endif

    menu->popup(contentViewsManager->comicsView->mapToGlobal(point));
}

void LibraryWindow::showGridFoldersContextMenu(QPoint point, Folder folder)
{
    auto menu = new QMenu(this);
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);

    const auto folderId = folder.id;
    const auto libraryPath = currentPath();
    const auto &menuIcons = theme.menuIcons;

    auto openContainingFolderAction = new QAction(menu);
    openContainingFolderAction->setText(tr("Open folder..."));
    openContainingFolderAction->setIcon(menuIcons.openContainingFolderIcon);

    auto updateFolderAction = new QAction(tr("Update folder"), menu);
    updateFolderAction->setIcon(menuIcons.updateCurrentFolderIcon);

    auto renameFolderAction = new QAction(tr("Rename folder"), menu);
    renameFolderAction->setIcon(theme.sidebarIcons.renameListIcon);

    auto rescanLibraryForXMLInfoAction = new QAction(tr("Rescan library for XML info"), menu);

    auto setFolderAsNotCompletedAction = new QAction(menu);
    setFolderAsNotCompletedAction->setText(tr("Set as uncompleted"));

    auto setFolderAsCompletedAction = new QAction(menu);
    setFolderAsCompletedAction->setText(tr("Set as completed"));

    auto setFolderAsReadAction = new QAction(menu);
    setFolderAsReadAction->setText(tr("Set as read"));

    auto setFolderAsUnreadAction = new QAction(menu);
    setFolderAsUnreadAction->setText(tr("Set as unread"));

    auto setFolderAsMangaAction = new QAction(menu);
    setFolderAsMangaAction->setText(tr("manga"));

    auto setFolderAsNormalAction = new QAction(menu);
    setFolderAsNormalAction->setText(tr("comic"));

    auto setFolderAsWesternMangaAction = new QAction(menu);
    setFolderAsWesternMangaAction->setText(tr("western manga (left to right)"));

    auto setFolderAsWebComicAction = new QAction(menu);
    setFolderAsWebComicAction->setText(tr("web comic"));

    auto setFolderAs4KomaAction = new QAction(menu);
    setFolderAs4KomaAction->setText(tr("4koma (top to botom)"));

    auto setFolderCoverAction = new QAction(menu);
    setFolderCoverAction->setText(tr("Set custom cover"));

    auto deleteCustomFolderCoverAction = new QAction(menu);
    deleteCustomFolderCoverAction->setText(tr("Delete custom cover"));

    menu->addAction(openContainingFolderAction);
    menu->addAction(renameFolderAction);
    menu->addAction(updateFolderAction);
    menu->addSeparator();
    menu->addAction(rescanLibraryForXMLInfoAction);
    menu->addSeparator();
    if (folder.completed)
        menu->addAction(setFolderAsNotCompletedAction);
    else
        menu->addAction(setFolderAsCompletedAction);
    menu->addSeparator();
    if (folder.finished)
        menu->addAction(setFolderAsUnreadAction);
    else
        menu->addAction(setFolderAsReadAction);
    menu->addSeparator();

    setFolderAsNormalAction->setCheckable(true);
    setFolderAsMangaAction->setCheckable(true);
    setFolderAsWesternMangaAction->setCheckable(true);
    setFolderAsWebComicAction->setCheckable(true);
    setFolderAs4KomaAction->setCheckable(true);

    switch (folder.type) {
    case FileType::Comic:
        setFolderAsNormalAction->setChecked(true);
        break;
    case FileType::Manga:
        setFolderAsMangaAction->setChecked(true);
        break;
    case FileType::WesternManga:
        setFolderAsWesternMangaAction->setChecked(true);
        break;
    case FileType::WebComic:
        setFolderAsWebComicAction->setChecked(true);
        break;
    case FileType::Yonkoma:
        setFolderAs4KomaAction->setChecked(true);
        break;
    }

    auto typeMenu = new QMenu(tr("Set type"), menu);
    menu->addMenu(typeMenu);
    typeMenu->addAction(setFolderAsNormalAction);
    typeMenu->addAction(setFolderAsMangaAction);
    typeMenu->addAction(setFolderAsWesternMangaAction);
    typeMenu->addAction(setFolderAsWebComicAction);
    typeMenu->addAction(setFolderAs4KomaAction);

    connect(openContainingFolderAction, &QAction::triggered, this, [=]() {
        QDesktopServices::openUrl(QUrl("file:///" + QDir::cleanPath(currentPath() + "/" + folder.path), QUrl::TolerantMode));
    });
    connect(updateFolderAction, &QAction::triggered, this, [=]() {
        updateFolder(foldersModel->getIndexFromFolder(folder));
    });
    connect(renameFolderAction, &QAction::triggered, this, [=]() {
        renameFolder(foldersModel->getIndexFromFolder(folder));
    });
    connect(rescanLibraryForXMLInfoAction, &QAction::triggered, this, [=]() {
        rescanFolderForXMLInfo(foldersModel->getIndexFromFolder(folder));
    });
    connect(setFolderAsNotCompletedAction, &QAction::triggered, this, [=]() {
        foldersModel->updateFolderCompletedStatus(QModelIndexList() << foldersModel->getIndexFromFolder(folder), false);
    });
    connect(setFolderAsCompletedAction, &QAction::triggered, this, [=]() {
        foldersModel->updateFolderCompletedStatus(QModelIndexList() << foldersModel->getIndexFromFolder(folder), true);
    });
    connect(setFolderAsReadAction, &QAction::triggered, this, [=]() {
        foldersModel->updateFolderFinishedStatus(QModelIndexList() << foldersModel->getIndexFromFolder(folder), true);
    });
    connect(setFolderAsUnreadAction, &QAction::triggered, this, [=]() {
        foldersModel->updateFolderFinishedStatus(QModelIndexList() << foldersModel->getIndexFromFolder(folder), false);
    });
    connect(setFolderAsMangaAction, &QAction::triggered, this, [=]() {
        foldersModel->updateFolderType(QModelIndexList() << foldersModel->getIndexFromFolder(folder), FileType::Manga);
    });
    connect(setFolderAsNormalAction, &QAction::triggered, this, [=]() {
        foldersModel->updateFolderType(QModelIndexList() << foldersModel->getIndexFromFolder(folder), FileType::Comic);
    });
    connect(setFolderAsWesternMangaAction, &QAction::triggered, this, [=]() {
        foldersModel->updateFolderType(QModelIndexList() << foldersModel->getIndexFromFolder(folder), FileType::WesternManga);
    });
    connect(setFolderAsWebComicAction, &QAction::triggered, this, [=]() {
        foldersModel->updateFolderType(QModelIndexList() << foldersModel->getIndexFromFolder(folder), FileType::WebComic);
    });
    connect(setFolderAs4KomaAction, &QAction::triggered, this, [=]() {
        foldersModel->updateFolderType(QModelIndexList() << foldersModel->getIndexFromFolder(folder), FileType::Yonkoma);
    });
    connect(setFolderCoverAction, &QAction::triggered, this, [this, folderId, libraryPath]() {
        folderManagementCoordinator->selectAndSetCustomCover(folderId, libraryPath);
    });

    connect(deleteCustomFolderCoverAction, &QAction::triggered, this, [this, folderId, libraryPath]() {
        folderManagementCoordinator->resetCustomCover(folderId, libraryPath);
    });

    menu->addSeparator();

    menu->addAction(setFolderCoverAction);
    if (!folder.customImage.isEmpty()) {
        menu->addAction(deleteCustomFolderCoverAction);
    }

    menu->popup(point);
}

void LibraryWindow::showContinueReadingContextMenu(QPoint point, ComicDB comic)
{
    QMenu menu;

    auto setAsUnReadAction = new QAction();
    setAsUnReadAction->setText(tr("Set as unread"));
    setAsUnReadAction->setIcon(theme.comicsViewToolbar.setAsUnreadIcon);

    menu.addAction(setAsUnReadAction);

    connect(setAsUnReadAction, &QAction::triggered, this, [=]() {
        auto libraryId = libraries.getId(selectedLibrary->currentText());
        auto info = comic.info;
        info.setRead(false);
        info.currentPage = 1;
        info.hasBeenOpened = false;
        info.lastTimeOpened = QVariant();
        DBHelper::update(libraryId, info);

        navigationController->reloadRootContinueReading();
    });

    menu.exec(point);
}

void LibraryWindow::setupAddToSubmenu(QMenu &menu)
{
    menu.addAction(actions.addToFavoritesAction);
    actions.addToMenuAction->setMenu(&menu);

    const QList<LabelItem *> labels = listsModel->getLabels();
    if (labels.count() > 0)
        menu.addSeparator();
    for (auto *label : labels) {
        auto action = new QAction(&menu);
        action->setIcon(label->getIcon());
        action->setText(label->name());

        action->setData(label->getId());

        menu.addAction(action);

        connect(action, &QAction::triggered, this, &LibraryWindow::onAddComicsToLabel);
    }
}

void LibraryWindow::onAddComicsToLabel()
{
    auto action = static_cast<QAction *>(sender());

    qulonglong labelId = action->data().toULongLong();

    QModelIndexList comics = getSelectedComics();

    comicsModel->addComicsToLabel(comics, labelId);
}

void LibraryWindow::setToolbarTitle(const QModelIndex &modelIndex)
{
#ifndef Y_MAC_UI
    if (!modelIndex.isValid())
        libraryToolBar->setCurrentFolderName(selectedLibrary->currentText());
    else
        libraryToolBar->setCurrentFolderName(modelIndex.data().toString());
#endif
}

// this methods is only using after deleting comics
// TODO broken window :)
void LibraryWindow::checkEmptyFolder()
{
    if (comicsModel->rowCount() > 0 && !importedCovers) {
        setComicActionsDisabled(false);
    } else {
        setComicActionsDisabled(true);
#ifndef Q_OS_MACOS
        if (comicsModel->rowCount() > 0)
            actions.toggleFullScreenAction->setEnabled(true);
#endif
        if (comicsModel->rowCount() == 0)
            navigationController->reselectCurrentFolder();
    }
}

void LibraryWindow::openComic()
{
    if (!importedCovers) {

        auto comic = comicsModel->getComic(contentViewsManager->comicsView->currentIndex());
        auto mode = comicsModel->getMode();

        openComic(comic, mode);
    }
}

void LibraryWindow::openComic(const ComicDB &comic, const ComicModel::Mode mode)
{
    auto libraryId = libraries.getId(selectedLibrary->currentText());

    OpenComicSource::Source source;

    if (mode == ComicModel::ReadingList) {
        source = OpenComicSource::Source::ReadingList;
    } else if (mode == ComicModel::Reading) {
        // TODO check where the comic was opened from the last time it was read
        source = OpenComicSource::Source::Folder;
    } else {
        source = OpenComicSource::Source::Folder;
    }

    auto thirdPartyReaderCommand = settings->value(THIRD_PARTY_READER_COMMAND, "").toString();
    if (thirdPartyReaderCommand.isEmpty()) {
        auto yacreaderFound = YACReader::openComic(comic, libraryId, currentPath(), OpenComicSource { source, comicsModel->getSourceId() });

        if (!yacreaderFound) {
#ifdef Q_OS_WIN
            QMessageBox::critical(this, tr("YACReader not found"), tr("YACReader not found. YACReader should be installed in the same folder as YACReaderLibrary."));
#else
            QMessageBox::critical(this, tr("YACReader not found"), tr("YACReader not found. There might be a problem with your YACReader installation."));
#endif
        }
    } else {
        auto exec = YACReader::openComicInThirdPartyApp(thirdPartyReaderCommand, QDir::cleanPath(currentPath() + comic.path));

        if (!exec) {
            QMessageBox::critical(this, tr("Error"), tr("Error opening comic with third party reader."));
        }
    }
}

void LibraryWindow::createLibrary()
{
    libraryManagementCoordinator->warnIfLibraryCountIsHigh();
    createLibraryDialog->open(libraries);
}

void LibraryWindow::reloadCurrentLibrary()
{
    if (!hasLoadedLibraryModels())
        return;

    foldersModel->reload();
    navigationController->refreshCurrentSource();

    enableNeededActions();
}

void LibraryWindow::showAddLibrary()
{
    libraryManagementCoordinator->warnIfLibraryCountIsHigh();
    addLibraryDialog->open();
}

void LibraryWindow::loadLibraries()
{
    const auto storedLibraries = libraryManagementCoordinator->loadLibraries();
    for (const auto &[name, path] : storedLibraries)
        selectedLibrary->addItem(name, path);
}

void LibraryWindow::addLibraryToSelector(const QString &libraryName, const QString &libraryPath)
{
    const QSignalBlocker blocker(selectedLibrary);
    selectedLibrary->addItem(libraryName, libraryPath);
    selectedLibrary->setCurrentIndex(selectedLibrary->findText(libraryName));
    addLibraryDialog->close();
    loadLibrary(libraryName);
}

void LibraryWindow::handleLibraryRemoved(const QString &libraryName, bool librariesEmpty)
{
    const auto index = selectedLibrary->findText(libraryName);
    if (index >= 0)
        selectedLibrary->removeItem(index);

    if (!librariesEmpty)
        return;

    contentViewsManager->comicsView->setModel(nullptr);
    foldersView->setModel(nullptr);
    listsView->setModel(nullptr);
    actions.disableAllActions();
    showNoLibrariesWidget();
}

void LibraryWindow::updateLibrary()
{
    const auto libraryName = selectedLibrary->currentText();
    libraryManagementCoordinator->updateLibrary(libraryName, libraries.getPath(libraryName));
}

void LibraryWindow::backupLibrary()
{
    libraryDatabaseMaintenanceCoordinator->backupLibrary(libraries.getPath(selectedLibrary->currentText()), actions.backupLibraryAction->text());
}

void LibraryWindow::restoreLibrary()
{
    const auto libraryName = selectedLibrary->currentText();
    libraryDatabaseMaintenanceCoordinator->restoreLibrary(libraryName, libraries.getPath(libraryName), actions.restoreLibraryAction->text());
}

void LibraryWindow::offerDatabaseRecovery(const QString &libraryName)
{
    libraryDatabaseMaintenanceCoordinator->offerDatabaseRecovery(libraryName, libraries.getPath(libraryName), actions.restoreLibraryAction->text());
}

void LibraryWindow::repairLibrary()
{
    const auto libraryName = selectedLibrary->currentText();
    libraryRepairCoordinator->repairLibrary(libraryName, libraries.getPath(libraryName), actions.repairLibraryAction->text());
}

void LibraryWindow::deleteCurrentLibrary()
{
    libraryManagementCoordinator->deleteLibrary(selectedLibrary->currentText(), true);
}

void LibraryWindow::removeLibrary()
{
    libraryManagementCoordinator->askToRemoveLibrary(selectedLibrary->currentText());
}

void LibraryWindow::renameLibrary()
{
    renameLibraryDialog->open();
}

void LibraryWindow::rename(QString newName) // TODO replace
{
    const auto currentLibrary = selectedLibrary->currentText();
    if (!libraryManagementCoordinator->renameLibrary(currentLibrary, newName))
        return;

    if (newName != currentLibrary) {
        selectedLibrary->renameCurrentLibrary(newName);
#ifndef Y_MAC_UI
        if (!foldersModelProxy->mapToSource(foldersView->currentIndex()).isValid())
            libraryToolBar->setCurrentFolderName(selectedLibrary->currentText());
#endif
    }
    renameLibraryDialog->close();
}

void LibraryWindow::rescanLibraryForXMLInfo()
{
    importWidget->setXMLScanLook();
    showImportingWidget();

    const auto currentLibrary = selectedLibrary->currentText();
    const auto path = libraries.getPath(currentLibrary);

    xmlInfoLibraryScanner->scanLibrary(path, LibraryPaths::libraryDataPath(path));
}

void LibraryWindow::showLibraryInfo()
{
    auto id = libraries.getUuid(selectedLibrary->currentText());
    auto info = DBHelper::getLibraryInfo(id);

    // TODO: use something nicer than a QMessageBox
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("Library info"));
    msgBox.setText(info);
    QSpacerItem *horizontalSpacer = new QSpacerItem(420, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    QGridLayout *layout = (QGridLayout *)msgBox.layout();
    layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());
    msgBox.setStandardButtons(QMessageBox::Close);
    msgBox.setDefaultButton(QMessageBox::Close);
    msgBox.exec();
}

void LibraryWindow::openLibraryFolder()
{
    const auto path = libraries.getPath(selectedLibrary->currentText());
    if (!path.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(QDir::cleanPath(path)));
}

void LibraryWindow::rescanCurrentFolderForXMLInfo()
{
    rescanFolderForXMLInfo(getCurrentFolderIndex());
}

void LibraryWindow::rescanFolderForXMLInfo(QModelIndex modelIndex)
{
    importWidget->setXMLScanLook();
    showImportingWidget();

    const auto currentLibrary = selectedLibrary->currentText();
    const auto path = libraries.getPath(currentLibrary);

    xmlInfoLibraryScanner->scanFolder(path, LibraryPaths::libraryDataPath(path), QDir::cleanPath(currentPath() + foldersModel->getFolderPath(modelIndex)), modelIndex);
}

void LibraryWindow::stopXMLScanning()
{
    xmlInfoLibraryScanner->stop();
    xmlInfoLibraryScanner->wait();
}

void LibraryWindow::setRootIndex()
{
    if (!libraries.isEmpty()) {
        QString path = LibraryPaths::libraryDataPath(libraries.getPath(selectedLibrary->currentText()));
        QDir d; // TODO change this by static methods (utils class?? with delTree for example)
        if (d.exists(path)) {
            navigationController->selectedFolder(QModelIndex());
        } else {
            contentViewsManager->comicsView->setModel(NULL);
        }

        auto selectionModel = foldersView->selectionModel();
        if (selectionModel != nullptr)
            selectionModel->clear();
    }
}

void LibraryWindow::toggleFullScreen()
{
    fullscreen ? toNormal() : toFullScreen();
    fullscreen = !fullscreen;
}

void LibraryWindow::toFullScreen()
{
    fromMaximized = this->isMaximized();

    sideBar->hide();
    libraryToolBar->hide();

    contentViewsManager->toFullscreen();

    showFullScreen();
}

void LibraryWindow::toNormal()
{
    sideBar->show();

    contentViewsManager->toNormal();

    if (fromMaximized)
        showMaximized();
    else
        showNormal();

#ifdef Y_MAC_UI
    auto timer = new QTimer();
    timer->setSingleShot(true);
    timer->start();
    connect(timer, &QTimer::timeout, libraryToolBar, &YACReaderMacOSXToolbar::show);
    connect(timer, &QTimer::timeout, timer, &QTimer::deleteLater);
#else
    libraryToolBar->show();
#endif
}

void LibraryWindow::setSearchFilter(QString filter)
{
    if (!filter.isEmpty()) {
        folderQueryResultProcessor->createModelData(filter);
        comicQueryResultProcessor.createModelData(filter, foldersModel->getDatabase());
    } else if (status == LibraryWindow::Searching) { // if no searching, then ignore this
        clearSearchFilter();
        navigationController->loadPreviousStatus();
    }
}

void LibraryWindow::setComicSearchFilterData(QList<ComicItem *> *data, const QString &databasePath)
{
    status = LibraryWindow::Searching;

    comicsModel->setModelData(data, databasePath);
    contentViewsManager->comicsView->enableFilterMode(true);
    contentViewsManager->comicsView->setModel(comicsModel); // TODO, columns are messed up after ResetModel some times, this shouldn't be necesary

    if (comicsModel->rowCount() == 0) {
        contentViewsManager->showNoSearchResults();
        setComicActionsDisabled(true);
    } else {
        contentViewsManager->showComicsView();
        setComicActionsDisabled(false);
    }
}

void LibraryWindow::setFolderSearchFilterData(QMap<unsigned long long, FolderItem *> *filteredItems, FolderItem *root)
{
    foldersModelProxy->setFilterData(filteredItems, root);
    foldersView->expandAll();
}

void LibraryWindow::clearSearchFilter()
{
    foldersModelProxy->clear();
    contentViewsManager->comicsView->enableFilterMode(false);
    foldersView->collapseAll();
    status = LibraryWindow::Normal;
}

void LibraryWindow::showComicVineScraper()
{
    QSettings s(YACReader::getSettingsPath() + "/YACReaderLibrary.ini", QSettings::IniFormat); // TODO unificar la creación del fichero de config con el servidor
    s.beginGroup("ComicVine");

    if (!s.contains(COMIC_VINE_API_KEY)) {
        ApiKeyDialog d;
        d.exec();
    }

    // check if the api key was inserted
    if (s.contains(COMIC_VINE_API_KEY)) {
        QModelIndexList indexList = getSelectedComics();

        const auto comics = comicsModel->getComics(indexList);
        comicVineDialog->databasePath = foldersModel->getDatabase();
        comicVineDialog->basePath = currentPath();
        comicVineDialog->setComics(comics);

        navigationController->beginCurrentSourceRefresh();
        comicVineDialog->show();
    }
}

void LibraryWindow::checkSearchNumResults(int numResults)
{
    if (numResults == 0)
        contentViewsManager->showNoSearchResults();
    else
        contentViewsManager->showComicsView();
}

void LibraryWindow::openContainingFolderComic()
{
    QModelIndex modelIndex = contentViewsManager->comicsView->currentIndex();
    QFileInfo file(QDir::cleanPath(currentPath() + comicsModel->getComicPath(modelIndex)));
#if defined Q_OS_UNIX && !defined Q_OS_MACOS
    QString path = file.absolutePath();
    QDesktopServices::openUrl(QUrl("file:///" + path, QUrl::TolerantMode));
#endif

#ifdef Q_OS_MACOS
    // `open -R` reveals and selects the file in Finder without sending an Apple
    // Event, so it doesn't trigger the macOS automation permission prompt.
    QStringList args;
    args << "-R";
    args << file.absoluteFilePath();
    QProcess::startDetached("open", args);
#endif

#ifdef Q_OS_WIN
    QString filePath = file.absoluteFilePath();
    QString cmdArgs = QString("/select,\"") + QDir::toNativeSeparators(filePath) + QStringLiteral("\"");
    ShellExecuteW(0, L"open", L"explorer.exe", reinterpret_cast<LPCWSTR>(cmdArgs.utf16()), 0, SW_NORMAL);
#endif
}

void LibraryWindow::openContainingFolder()
{
    QModelIndex modelIndex = foldersModelProxy->mapToSource(foldersView->currentIndex());
    QString path;
    if (modelIndex.isValid())
        path = QDir::cleanPath(currentPath() + foldersModel->getFolderPath(modelIndex));
    else
        path = QDir::cleanPath(currentPath());
    QDesktopServices::openUrl(QUrl("file:///" + path, QUrl::TolerantMode));
}

void LibraryWindow::organizeFiles()
{
    const QModelIndex sourceIndex = getCurrentFolderIndex();
    if (!sourceIndex.isValid())
        return;

    const auto libraryId = libraries.getId(selectedLibrary->currentText());
    const auto folder = foldersModel->getFolder(sourceIndex);
    const QString folderAbsolutePath = QDir::cleanPath(currentPath() + foldersModel->getFolderPath(sourceIndex));

    if (organizeFilesCoordinator->organizeFolder(libraryId, folder.id, currentPath(), folderAbsolutePath))
        updateFolder(sourceIndex);
}

void LibraryWindow::organizeComicsFiles()
{
    const QModelIndexList indexList = getSelectedComics();
    if (indexList.isEmpty())
        return;

    const QList<ComicDB> comics = comicsModel->getComics(indexList);
    if (comics.isEmpty())
        return;

    const QModelIndex folderIndex = getCurrentFolderIndex();
    const QString folderAbsolutePath = folderIndex.isValid()
            ? QDir::cleanPath(currentPath() + foldersModel->getFolderPath(folderIndex))
            : QDir::cleanPath(currentPath());

    if (organizeFilesCoordinator->organizeComics(comics, currentPath(), folderAbsolutePath)) {
        if (folderIndex.isValid())
            updateFolder(folderIndex);
        else
            reloadCurrentFolderComicsContent();
    }
}

void LibraryWindow::setFolderAsNotCompleted()
{
    // foldersModel->updateFolderCompletedStatus(foldersView->selectionModel()->selectedRows(),false);
    foldersModel->updateFolderCompletedStatus(QModelIndexList() << foldersModelProxy->mapToSource(foldersView->currentIndex()), false);
}

void LibraryWindow::setFolderAsCompleted()
{
    // foldersModel->updateFolderCompletedStatus(foldersView->selectionModel()->selectedRows(),true);
    foldersModel->updateFolderCompletedStatus(QModelIndexList() << foldersModelProxy->mapToSource(foldersView->currentIndex()), true);
}

void LibraryWindow::setFolderAsRead()
{
    // foldersModel->updateFolderFinishedStatus(foldersView->selectionModel()->selectedRows(),true);
    foldersModel->updateFolderFinishedStatus(QModelIndexList() << foldersModelProxy->mapToSource(foldersView->currentIndex()), true);
}

void LibraryWindow::setFolderAsUnread()
{
    // foldersModel->updateFolderFinishedStatus(foldersView->selectionModel()->selectedRows(),false);
    foldersModel->updateFolderFinishedStatus(QModelIndexList() << foldersModelProxy->mapToSource(foldersView->currentIndex()), false);
}

void LibraryWindow::setFolderType(FileType type)
{
    foldersModel->updateFolderType(QModelIndexList() << foldersModelProxy->mapToSource(foldersView->currentIndex()), type);
}

void LibraryWindow::setFolderCover()
{
    const auto folderIndex = foldersModelProxy->mapToSource(foldersView->currentIndex());
    if (!folderIndex.isValid())
        return;

    folderManagementCoordinator->selectAndSetCustomCover(folderIndex.data(FolderModel::IdRole).toULongLong(), currentPath());
}

void LibraryWindow::deleteCustomFolderCover()
{
    const auto folderIndex = foldersModelProxy->mapToSource(foldersView->currentIndex());
    if (!folderIndex.isValid())
        return;

    folderManagementCoordinator->resetCustomCover(folderIndex.data(FolderModel::IdRole).toULongLong(), currentPath());
}

void LibraryWindow::exportLibrary(QString destPath)
{
    QString currentLibrary = selectedLibrary->currentText();
    QString path = LibraryPaths::libraryDataPath(libraries.getPath(currentLibrary));
    packageManager->createPackage(path, destPath + "/" + currentLibrary);
}

void LibraryWindow::importLibrary(QString clc, QString destPath, QString name)
{
    packageManager->extractPackage(clc, destPath + "/" + name);
    libraryManagementCoordinator->prepareImportedLibrary(name, destPath + "/" + name);
}

void LibraryWindow::reloadOptions()
{
    contentViewsManager->comicsView->updateConfig(settings);

    trayIconController->updateIconVisibility();

    recentVisibilityCoordinator->updateTimeRange();
}

QString LibraryWindow::currentPath()
{
    return libraries.getPath(selectedLibrary->currentText());
}

QString LibraryWindow::currentFolderPath()
{
    QString path;

    if (foldersView->selectionModel()->selectedRows().length() > 0)
        path = foldersModel->getFolderPath(foldersModelProxy->mapToSource(foldersView->currentIndex()));
    else
        path = foldersModel->getFolderPath(QModelIndex());

    QLOG_DEBUG() << "current folder path : " << QDir::cleanPath(currentPath() + path);

    return QDir::cleanPath(currentPath() + path);
}

void LibraryWindow::showExportComicsInfo()
{
    exportComicsInfoDialog->source = LibraryPaths::libraryDatabasePath(currentPath());
    exportComicsInfoDialog->open();
}

void LibraryWindow::showImportComicsInfo()
{
    importComicsInfoDialog->dest = LibraryPaths::libraryDatabasePath(currentPath());
    importComicsInfoDialog->open();
}

void LibraryWindow::closeEvent(QCloseEvent *event)
{
    if (!trayIconController->handleCloseToTrayIcon(event)) {
        event->accept();
        closeApp();
    }
}

void LibraryWindow::prepareToCloseApp()
{
    httpServer->stop();

    libraryManagementCoordinator->stop();
    librariesUpdateCoordinator->stop();
    libraryRepairCoordinator->stop();

    settings->setValue(MAIN_WINDOW_GEOMETRY, saveGeometry());
    settings->setValue(MAIN_WINDOW_STATE, saveState());

    contentViewsManager->prepareToClose();
    sideBar->close();

    QApplication::instance()->processEvents();
}

void LibraryWindow::closeApp()
{
    prepareToCloseApp();

    qApp->exit(0);
}

void LibraryWindow::showNoLibrariesWidget()
{
    actions.disableAllActions();
    setSearchInputEnabled(false);
    mainWidget->setCurrentIndex(1);
}

void LibraryWindow::showRootWidget()
{
#ifndef Y_MAC_UI
    libraryToolBar->setDisabled(false);
#endif
    setSearchInputEnabled(true);
    mainWidget->setCurrentIndex(0);
}

void LibraryWindow::showImportingWidget()
{
    actions.disableAllActions();
    importWidget->clear();
#ifndef Y_MAC_UI
    libraryToolBar->setDisabled(true);
#endif
    setSearchInputEnabled(false);
    mainWidget->setCurrentIndex(2);
}

void LibraryWindow::manageCreatingError(const QString &error)
{
    QMessageBox::critical(this, tr("Error creating the library"), error);
}

void LibraryWindow::manageUpdatingError(const QString &error)
{
    QMessageBox::critical(this, tr("Error updating the library"), error);
}

void LibraryWindow::manageOpeningLibraryError(const QString &error)
{
    QMessageBox::critical(this, tr("Error opening the library"), error);
}

bool lessThanModelIndexRow(const QModelIndex &m1, const QModelIndex &m2)
{
    return m1.row() < m2.row();
}

QModelIndexList LibraryWindow::getSelectedComics()
{
    // se fuerza a que haya almenos una fila seleccionada TODO comprobar se se puede forzar a la tabla a que lo haga automáticamente
    // avoid selection.count()==0 forcing selection in comicsView
    QModelIndexList selection = contentViewsManager->comicsView->selectionModel()->selectedRows();
    QLOG_TRACE() << "selection count " << selection.length();
    std::sort(selection.begin(), selection.end(), lessThanModelIndexRow);

    if (selection.count() == 0) {
        contentViewsManager->comicsView->selectIndex(0);
        selection = contentViewsManager->comicsView->selectionModel()->selectedRows();
    }
    return selection;
}

void LibraryWindow::showFoldersContextMenu(const QPoint &point)
{
    QModelIndex sourceMI = foldersModelProxy->mapToSource(foldersView->indexAt(point));

    if (!sourceMI.isValid())
        return;

    auto folder = foldersModel->getFolder(sourceMI);

    actions.setFolderAsNormalAction->setCheckable(true);
    actions.setFolderAsMangaAction->setCheckable(true);
    actions.setFolderAsWesternMangaAction->setCheckable(true);
    actions.setFolderAsWebComicAction->setCheckable(true);
    actions.setFolderAsYonkomaAction->setCheckable(true);

    actions.setFolderAsNormalAction->setChecked(false);
    actions.setFolderAsMangaAction->setChecked(false);
    actions.setFolderAsWesternMangaAction->setChecked(false);
    actions.setFolderAsWebComicAction->setChecked(false);
    actions.setFolderAsYonkomaAction->setChecked(false);

    switch (folder.type) {
    case FileType::Comic:
        actions.setFolderAsNormalAction->setChecked(true);
        break;
    case FileType::Manga:
        actions.setFolderAsMangaAction->setChecked(true);
        break;
    case FileType::WesternManga:
        actions.setFolderAsWesternMangaAction->setChecked(true);
        break;
    case FileType::WebComic:
        actions.setFolderAsWebComicAction->setChecked(true);
        break;
    case FileType::Yonkoma:
        actions.setFolderAsYonkomaAction->setChecked(true);
        break;
    }

    QMenu menu;

    menu.addAction(actions.openContainingFolderAction);
    menu.addAction(actions.renameFolderAction);
    if (YACReader::FeatureFlags::organizeFiles)
        menu.addAction(actions.organizeFilesAction);
    menu.addAction(actions.updateFolderAction);
    menu.addSeparator(); //-------------------------------
    menu.addAction(actions.rescanXMLFromCurrentFolderAction);
    menu.addSeparator(); //-------------------------------
    if (folder.completed)
        menu.addAction(actions.setFolderAsNotCompletedAction);
    else
        menu.addAction(actions.setFolderAsCompletedAction);
    menu.addSeparator(); //-------------------------------
    if (folder.finished)
        menu.addAction(actions.setFolderAsUnreadAction);
    else
        menu.addAction(actions.setFolderAsReadAction);
    menu.addSeparator(); //-------------------------------
    auto typeMenu = new QMenu(tr("Set type"));
    menu.addMenu(typeMenu);
    typeMenu->addAction(actions.setFolderAsNormalAction);
    typeMenu->addAction(actions.setFolderAsMangaAction);
    typeMenu->addAction(actions.setFolderAsWesternMangaAction);
    typeMenu->addAction(actions.setFolderAsWebComicAction);
    typeMenu->addAction(actions.setFolderAsYonkomaAction);
    menu.addSeparator(); //-------------------------------
    menu.addAction(actions.setFolderCoverAction);
    if (!folder.customImage.isEmpty()) {
        menu.addAction(actions.deleteCustomFolderCoverAction);
    }

    menu.exec(foldersView->mapToGlobal(point));
}

void LibraryWindow::importLibraryPackage()
{
    importLibraryDialog->open(libraries);
}

void LibraryWindow::updateViewsOnClientSync()
{
    comicsModel->reload();
    contentViewsManager->updateCurrentComicView();
    navigationController->reloadRootContinueReading();
}

void LibraryWindow::updateViewsOnComicUpdateWithId(quint64 libraryId, quint64 comicId)
{
    if (libraryId == (quint64)libraries.getId(selectedLibrary->currentText())) {
        auto path = libraries.getPath(libraryId);
        if (path.isEmpty()) {
            return;
        }
        QString connectionName = "";
        {
            QSqlDatabase db = DataBaseManagement::loadDatabase(LibraryPaths::libraryDataPath(path));
            bool found;
            auto comic = DBHelper::loadComic(comicId, db, found);
            if (found) {
                updateViewsOnComicUpdate(libraryId, comic);
            }

            qDebug() << db.lastError();
            connectionName = db.connectionName();
        }
        QSqlDatabase::removeDatabase(connectionName);
    }
}

void LibraryWindow::updateViewsOnComicUpdate(quint64 libraryId, const ComicDB &comic)
{
    if (libraryId == (quint64)libraries.getId(selectedLibrary->currentText())) {
        comicsModel->reload(comic);
        contentViewsManager->updateCurrentComicView();
        navigationController->reloadRootContinueReading();
    }
}

bool LibraryWindow::exitSearchMode()
{
    if (status != LibraryWindow::Searching)
        return false;
    clearSearchInput(false);
    clearSearchFilter();
    return true;
}
