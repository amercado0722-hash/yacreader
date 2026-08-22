#ifndef LIBRARY_REPAIR_COORDINATOR_H
#define LIBRARY_REPAIR_COORDINATOR_H

#include <QObject>
#include <QString>

class QSettings;
class QWidget;

namespace YACReader {
class ComicInfoRepairer;
}

class LibraryRepairCoordinator : public QObject
{
    Q_OBJECT

public:
    LibraryRepairCoordinator(QSettings *settings, QWidget *dialogParent);

    void repairLibrary(const QString &libraryName, const QString &libraryPath, const QString &dialogTitle);
    void stop();

signals:
    void repairStarted();
    void repairFinished();
    void comicProcessed(const QString &relativePath, const QString &coverPath);
    void databaseRecoveryRequested(const QString &libraryName);

private:
    void startRepair(bool removeStaleLock);
    void handleFinished();
    void handleFailure(const QString &error);

    QWidget *dialogParent;
    YACReader::ComicInfoRepairer *repairer;
    QString libraryName;
    QString libraryPath;
    QString dialogTitle;
};

#endif
