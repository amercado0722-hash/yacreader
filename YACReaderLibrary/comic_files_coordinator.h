#ifndef COMIC_FILES_COORDINATOR_H
#define COMIC_FILES_COORDINATOR_H

#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <QtGlobal>

class ComicFilesManager;
class QProgressDialog;
class QWidget;

class ComicFilesCoordinator : public QObject
{
    Q_OBJECT
public:
    explicit ComicFilesCoordinator(QWidget *window);

    void copyAndImportComics(const QList<QPair<QString, QString>> &comics,
                             const QString &destinationPath,
                             qulonglong destinationFolderId);
    void moveAndImportComics(const QList<QPair<QString, QString>> &comics,
                             const QString &destinationPath,
                             qulonglong destinationFolderId);

signals:
    void importRequested(qulonglong destinationFolderId);

private:
    QProgressDialog *newProgressDialog(const QString &label, int maximum);
    void processComicFiles(ComicFilesManager *comicFilesManager, QProgressDialog *progressDialog);

    QWidget *window;
};

#endif // COMIC_FILES_COORDINATOR_H
