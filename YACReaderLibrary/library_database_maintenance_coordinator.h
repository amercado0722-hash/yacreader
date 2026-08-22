#ifndef LIBRARY_DATABASE_MAINTENANCE_COORDINATOR_H
#define LIBRARY_DATABASE_MAINTENANCE_COORDINATOR_H

#include <QObject>
#include <QString>

class QWidget;

class LibraryDatabaseMaintenanceCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit LibraryDatabaseMaintenanceCoordinator(QWidget *dialogParent);

    void backupLibrary(const QString &libraryPath, const QString &dialogTitle);
    void restoreLibrary(const QString &libraryName, const QString &libraryPath, const QString &dialogTitle);
    void offerDatabaseRecovery(const QString &libraryName, const QString &libraryPath, const QString &restoreDialogTitle);

signals:
    void backupAvailabilityChanged(bool available);
    void maintenanceStarted();
    void libraryReloadRequested(const QString &libraryName);
    void libraryUpdateRequested();
    void invalidDatabaseRestoreCancelled();
    void databaseUnavailableAfterRestore();
    void databaseSalvageFailed();

private:
    void startLibraryRestore(const QString &libraryName, const QString &libraryPath, const QString &backupPath, const QString &dialogTitle, bool allowInvalidCurrent = false, bool removeStaleLock = false);
    void startDatabaseSalvage(const QString &libraryName, const QString &libraryPath, bool removeStaleLock = false);

    QWidget *dialogParent;
};

#endif
