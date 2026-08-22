#include "comic_files_coordinator.h"

#include "comic_files_manager.h"

#include <QCoreApplication>
#include <QProgressDialog>
#include <QThread>
#include <QWidget>
#include <QsLog.h>

ComicFilesCoordinator::ComicFilesCoordinator(QWidget *window)
    : QObject(window), window(window)
{
}

void ComicFilesCoordinator::copyAndImportComics(const QList<QPair<QString, QString>> &comics,
                                                const QString &destinationPath,
                                                qulonglong destinationFolderId)
{
    QLOG_DEBUG() << "Copying comics to" << destinationPath;
    if (comics.isEmpty())
        return;

    auto progressDialog = newProgressDialog(QCoreApplication::translate("LibraryWindow", "Copying comics..."), comics.size());
    auto comicFilesManager = new ComicFilesManager;
    comicFilesManager->copyComicsTo(comics, destinationPath, destinationFolderId);
    processComicFiles(comicFilesManager, progressDialog);
}

void ComicFilesCoordinator::moveAndImportComics(const QList<QPair<QString, QString>> &comics,
                                                const QString &destinationPath,
                                                qulonglong destinationFolderId)
{
    QLOG_DEBUG() << "Moving comics to" << destinationPath;
    if (comics.isEmpty())
        return;

    auto progressDialog = newProgressDialog(QCoreApplication::translate("LibraryWindow", "Moving comics..."), comics.size());
    auto comicFilesManager = new ComicFilesManager;
    comicFilesManager->moveComicsTo(comics, destinationPath, destinationFolderId);
    processComicFiles(comicFilesManager, progressDialog);
}

QProgressDialog *ComicFilesCoordinator::newProgressDialog(const QString &label, int maximum)
{
    auto progressDialog = new QProgressDialog(label, QStringLiteral("Cancel"), 0, maximum, window);
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->setMinimumWidth(350);
    progressDialog->show();
    return progressDialog;
}

void ComicFilesCoordinator::processComicFiles(ComicFilesManager *comicFilesManager, QProgressDialog *progressDialog)
{
    connect(comicFilesManager, &ComicFilesManager::progress, progressDialog, &QProgressDialog::setValue);

    auto thread = new QThread;
    comicFilesManager->moveToThread(thread);

    connect(progressDialog, &QProgressDialog::canceled, comicFilesManager, &ComicFilesManager::cancel, Qt::DirectConnection);
    connect(thread, &QThread::started, comicFilesManager, &ComicFilesManager::process);
    connect(comicFilesManager, &ComicFilesManager::success, this, &ComicFilesCoordinator::importRequested);
    connect(comicFilesManager, &ComicFilesManager::finished, thread, &QThread::quit);
    connect(comicFilesManager, &ComicFilesManager::finished, comicFilesManager, &QObject::deleteLater);
    connect(comicFilesManager, &ComicFilesManager::finished, progressDialog, &QWidget::close);
    connect(comicFilesManager, &ComicFilesManager::finished, progressDialog, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
}
