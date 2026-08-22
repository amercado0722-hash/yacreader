#ifndef LIBRARY_MANAGEMENT_COORDINATOR_H
#define LIBRARY_MANAGEMENT_COORDINATOR_H

#include <QObject>
#include <QString>

#include <future>

class LibraryCreator;
class QSettings;
class QWidget;
class YACReaderLibraries;

class LibraryManagementCoordinator : public QObject
{
    Q_OBJECT

public:
    LibraryManagementCoordinator(QSettings *settings, YACReaderLibraries &libraries, QWidget *dialogParent);

    void loadLibrary(const QString &libraryName, const QString &libraryPath);
    QList<QPair<QString, QString>> loadLibraries();

    void createLibrary(const QString &source, const QString &destination, const QString &name);
    void updateLibrary(const QString &libraryName, const QString &libraryPath);
    void updateFolder(const QString &libraryName, const QString &libraryPath, const QString &folderPath, qulonglong folderId);
    void addExistingLibrary(QString libraryPath, const QString &libraryName);
    void prepareImportedLibrary(const QString &libraryName, const QString &libraryPath);
    void finishAddingLibrary();

    void askToRemoveLibrary(const QString &libraryName);
    void deleteLibrary(const QString &libraryName, bool deleteMetadata);
    bool renameLibrary(const QString &currentName, const QString &newName);

    void warnIfLibraryCountIsHigh();
    void showLibraryAlreadyExists(const QString &libraryName);
    void stop();

signals:
    void loadStarted();
    void libraryReady(const QString &libraryDataPath, bool readOnly);
    void libraryManagementOnlyRequested();
    void databaseRecoveryRequested(const QString &libraryName);
    void upgradeStarted();
    void upgradeFailed(const QString &libraryDataPath);
    void libraryReloadRequested(const QString &libraryName);
    void libraryRecreationRequested(const QString &libraryName, const QString &libraryPath);
    void openingError(const QString &error);

    void creationStarted();
    void updateStarted();
    void operationUiResetRequested();
    void operationFinished();
    void currentLibraryReloadRequested();
    void libraryAdded(const QString &libraryName, const QString &libraryPath);
    void libraryRemoved(const QString &libraryName, bool librariesEmpty);
    void folderUpdateFinished(qulonglong folderId);
    void comicAdded(const QString &relativePath, const QString &coverPath);
    void creationFailed(const QString &error);
    void updateFailed(const QString &error);

private:
    void startUpgrade(const QString &libraryName, const QString &libraryPath, const QString &libraryDataPath);
    void handleCreatorOpeningFailure(const QString &error);

    YACReaderLibraries &libraries;
    QWidget *dialogParent;
    LibraryCreator *libraryCreator;
    QString pendingLibraryName;
    QString pendingLibraryPath;
    QString operationLibraryName;
    QString operationLibraryPath;
    std::future<void> upgradeFuture;
};

#endif
