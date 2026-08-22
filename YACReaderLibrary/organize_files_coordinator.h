#ifndef ORGANIZE_FILES_COORDINATOR_H
#define ORGANIZE_FILES_COORDINATOR_H

#include "comic_db.h"

#include <QObject>

class QSettings;
class QWidget;

class OrganizeFilesCoordinator : public QObject
{
    Q_OBJECT
public:
    explicit OrganizeFilesCoordinator(QSettings *settings, QWidget *window);

    bool organizeFolder(qulonglong libraryId,
                        qulonglong folderId,
                        const QString &libraryRoot,
                        const QString &folderPath);
    bool organizeComics(const QList<ComicDB> &comics,
                        const QString &libraryRoot,
                        const QString &cleanupPath);

private:
    QSettings *settings;
    QWidget *window;
};

#endif // ORGANIZE_FILES_COORDINATOR_H
