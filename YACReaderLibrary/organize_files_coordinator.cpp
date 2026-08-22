#include "organize_files_coordinator.h"

#include "db_helper.h"
#include "organize_files_dialog.h"
#include "organize_files_preview_dialog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMessageBox>
#include <QSet>
#include <QSettings>
#include <QWidget>

#include <algorithm>

namespace {
void collectComicsRecursively(qulonglong libraryId, qulonglong folderId, QList<ComicDB> &out)
{
    const auto comics = DBHelper::getFolderComicsFromLibrary(libraryId, folderId);
    for (auto *item : comics) {
        if (auto *comic = static_cast<ComicDB *>(item))
            out.append(*comic);
    }
    qDeleteAll(comics);

    const auto subfolders = DBHelper::getFolderSubfoldersFromLibrary(libraryId, folderId);
    for (auto *item : subfolders) {
        collectComicsRecursively(libraryId, item->id, out);
    }
    qDeleteAll(subfolders);
}

void removeEmptyDirs(const QString &basePath)
{
    QDir base(basePath);
    const auto entries = base.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        const QString childPath = base.absoluteFilePath(entry);
        removeEmptyDirs(childPath);
        QDir().rmdir(childPath);
    }
}

QString uniqueDestination(const QString &destination, const QSet<QString> &taken)
{
    if (!QFileInfo::exists(destination) && !taken.contains(destination))
        return destination;

    const QFileInfo destInfo(destination);
    const QString dir = destInfo.absolutePath();
    const QString base = destInfo.completeBaseName();
    const QString suffix = destInfo.suffix().isEmpty() ? QString() : QStringLiteral(".") + destInfo.suffix();
    int counter = 1;
    QString candidate;
    do {
        candidate = QDir::cleanPath(dir + QStringLiteral("/") + base + QStringLiteral(" (") + QString::number(counter++) + QStringLiteral(")") + suffix);
    } while (QFileInfo::exists(candidate) || taken.contains(candidate));
    return candidate;
}
}

OrganizeFilesCoordinator::OrganizeFilesCoordinator(QSettings *settings, QWidget *window)
    : QObject(window), settings(settings), window(window)
{
}

bool OrganizeFilesCoordinator::organizeFolder(qulonglong libraryId,
                                              qulonglong folderId,
                                              const QString &libraryRoot,
                                              const QString &folderPath)
{
    QList<ComicDB> comics;
    collectComicsRecursively(libraryId, folderId, comics);

    if (comics.isEmpty()) {
        QMessageBox::information(window, tr("Organize files"), tr("This folder does not contain any comics to organize."));
        return false;
    }

    return organizeComics(comics, libraryRoot, folderPath);
}

bool OrganizeFilesCoordinator::organizeComics(const QList<ComicDB> &comics,
                                              const QString &libraryRoot,
                                              const QString &cleanupPath)
{
    const QString cleanLibraryRoot = QDir::cleanPath(libraryRoot);

    OrganizeFilesDialog dialog(cleanLibraryRoot, cleanupPath, settings, window);
    if (dialog.exec() != QDialog::Accepted)
        return false;

    const QString pattern = dialog.formatPattern();
    if (pattern.trimmed().isEmpty())
        return false;

    using Move = OrganizeFilesPreviewDialog::Move;
    QList<Move> moves;
    QSet<QString> takenDestinations;
    const QDir destinationRoot(dialog.relativeToRoot() ? cleanLibraryRoot : cleanupPath);

    QHash<QString, int> seriesNumberWidth;
    for (const ComicDB &comic : comics) {
        const QString series = comic.info.series.toString().trimmed();
        bool ok = false;
        const int value = comic.info.number.toString().trimmed().toInt(&ok);
        if (!ok)
            continue;
        const int width = QString::number(value).size();
        int &current = seriesNumberWidth[series];
        current = std::max(current, width);
    }

    for (const ComicDB &comic : comics) {
        const QString source = QDir::cleanPath(cleanLibraryRoot + comic.path);
        const QFileInfo sourceInfo(source);
        if (!sourceInfo.exists())
            continue;

        const QString extension = sourceInfo.suffix().isEmpty() ? QString() : QStringLiteral(".") + sourceInfo.suffix();

        const int numberPadding = seriesNumberWidth.value(comic.info.series.toString().trimmed(), 0);

        const QString relative = OrganizeFilesDialog::buildRelativePath(pattern,
                                                                        comic.info.publisher.toString(),
                                                                        comic.info.series.toString(),
                                                                        comic.info.number.toString(),
                                                                        comic.info.title.toString(),
                                                                        comic.info.volume.toString(),
                                                                        comic.info.year.toString(),
                                                                        extension,
                                                                        numberPadding);

        QString destination = QDir::cleanPath(destinationRoot.absoluteFilePath(relative));
        if (destination == QDir::cleanPath(source))
            continue;

        destination = uniqueDestination(destination, takenDestinations);
        takenDestinations.insert(destination);

        moves.append({ source, destination });
    }

    if (moves.isEmpty()) {
        QMessageBox::information(window, tr("Organize files"), tr("All files are already organized according to this format."));
        return false;
    }

    OrganizeFilesPreviewDialog preview(destinationRoot.absolutePath(), cleanLibraryRoot, moves, window);
    if (preview.exec() != QDialog::Accepted)
        return false;

    QList<Move> finalMoves;
    QSet<QString> finalTaken;
    for (const Move &move : preview.moves()) {
        if (QDir::cleanPath(move.destination) == QDir::cleanPath(move.source))
            continue;
        const QString destination = uniqueDestination(move.destination, finalTaken);
        finalTaken.insert(destination);
        finalMoves.append({ move.source, destination });
    }

    if (finalMoves.isEmpty())
        return false;

    int moved = 0;
    QStringList failures;
    for (const Move &move : finalMoves) {
        const QString targetDir = QFileInfo(move.destination).absolutePath();
        if (!QDir().mkpath(targetDir)) {
            failures << move.source;
            continue;
        }
        if (QFile::rename(move.source, move.destination))
            moved++;
        else
            failures << move.source;
    }

    removeEmptyDirs(cleanupPath);

    if (!failures.isEmpty()) {
        QMessageBox::warning(window, tr("Organize files"),
                             tr("%1 of %2 file(s) were moved. %3 file(s) could not be moved.")
                                     .arg(moved)
                                     .arg(finalMoves.size())
                                     .arg(failures.size()));
    }

    return moved > 0;
}
