#include "library_intake.h"

#include "QsLog.h"
#include "data_base_management.h"
#include "metadata/series_match_scorer.h"
#include "series_name_utils.h"
#include "volume_number_utils.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTextStream>

using namespace YACReader;

namespace {

// What counts as a comic sitting at the top of the library. Deliberately the same list the
// rest of the application reads, minus the loose image folders: a directory of pages is a
// series folder as far as this is concerned, not a volume.
const QStringList kComicSuffixes = {
    QStringLiteral("cbz"), QStringLiteral("cbr"), QStringLiteral("cb7"),
    QStringLiteral("zip"), QStringLiteral("rar"), QStringLiteral("7z"),
    QStringLiteral("tar"), QStringLiteral("pdf")
};

bool looksLikeAComic(const QFileInfo &info)
{
    return info.isFile() && kComicSuffixes.contains(info.suffix().toLower());
}

bool holdsAComic(const QString &folderPath)
{
    QDirIterator it(folderPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (looksLikeAComic(QFileInfo(it.next()))) {
            return true;
        }
    }
    return false;
}

}

LibraryIntake::LibraryIntake(QObject *parent)
    : QObject(parent)
{
    debounce.setSingleShot(true);
    debounce.setInterval(2000);
    connect(&debounce, &QTimer::timeout, this, &LibraryIntake::settle);

    settleTimer.setSingleShot(true);
    settleTimer.setInterval(2000);
    connect(&settleTimer, &QTimer::timeout, this, &LibraryIntake::settle);

    connect(&watcher, &QFileSystemWatcher::directoryChanged, this, &LibraryIntake::onDirectoryChanged);
}

QString LibraryIntake::quarantineFolderName()
{
    return QStringLiteral("_Needs a look");
}

void LibraryIntake::watch(const QString &libraryPath, const QString &databasePath)
{
    stop();

    this->libraryPath = libraryPath;
    this->databasePath = databasePath;

    if (libraryPath.isEmpty() || !QDir(libraryPath).exists()) {
        return;
    }

    watcher.addPath(libraryPath);

    // Anything dropped while the application was closed is still sitting there.
    debounce.start();
}

void LibraryIntake::stop()
{
    debounce.stop();
    settleTimer.stop();
    lastSeenSize.clear();

    const auto watched = watcher.directories();
    if (!watched.isEmpty()) {
        watcher.removePaths(watched);
    }

    libraryPath.clear();
    databasePath.clear();
}

void LibraryIntake::onDirectoryChanged()
{
    debounce.start();
}

// The series already in the library, by the name they are filed under. Read from the folder
// table rather than from disk, so a folder that exists but has not been scanned in yet is
// not mistaken for a series that is ready to receive volumes.
QStringList LibraryIntake::seriesFolderNames() const
{
    QStringList names;
    if (databasePath.isEmpty()) {
        return names;
    }

    QString connectionName;
    {
        auto db = DataBaseManagement::loadDatabase(databasePath);
        if (!db.open()) {
            return names;
        }
        connectionName = db.connectionName();

        QSqlQuery query(db);
        query.prepare("select name from folder where id <> 1 and id in (select distinct parentId from comic)");
        query.exec();
        while (query.next()) {
            names.append(query.value(0).toString());
        }
    }
    QSqlDatabase::removeDatabase(connectionName);

    return names;
}

// What is sitting at the top of the library that is not already a series: loose comic files,
// and folders the library has never heard of. Everything the library does know about is left
// strictly alone, which is what keeps this from touching the nineteen hundred series already
// filed.
QList<LibraryIntake::Arrival> LibraryIntake::arrivalsAtTop() const
{
    QList<Arrival> arrivals;

    const auto known = seriesFolderNames();

    QDir top(libraryPath);
    const auto entries = top.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &entry : entries) {
        // The library's own housekeeping, and this application's own working folders.
        if (entry.fileName().startsWith(QLatin1Char('_')) || entry.fileName().startsWith(QLatin1Char('.'))) {
            continue;
        }

        if (looksLikeAComic(entry)) {
            arrivals.append({ entry.absoluteFilePath(), false, false });
            continue;
        }

        if (!entry.isDir()) {
            continue;
        }

        const auto alreadyFiled = known.contains(entry.fileName());

        // A folder whose name still carries a volume number is looked at again even when the
        // library has already taken it in. Being filed once is not a reason to stay wrong:
        // "A Silent Voice v01-07" was scanned in under that name by an earlier version of
        // this, and skipping everything the library knows about meant nothing could ever put
        // it right. The test is the marked form only, so it cannot fire on a title.
        const auto stillMarked = withoutVolumeMarker(entry.fileName()) != entry.fileName();
        if (alreadyFiled && !stillMarked) {
            continue;
        }

        // Cheap tests first: this runs over every folder in the library, and looking inside
        // nineteen hundred of them for a comic on every pass would not be cheap.
        if (holdsAComic(entry.absoluteFilePath())) {
            arrivals.append({ entry.absoluteFilePath(), true, alreadyFiled });
        }
    }

    return arrivals;
}

// A file that is still being copied grows between one look and the next. Two identical
// sizes two seconds apart is not a guarantee, but it is what stops this moving the first
// half of a two hundred megabyte volume.
bool LibraryIntake::hasSettled(const Arrival &arrival)
{
    if (arrival.isFolder) {
        // A folder's own size says nothing, so the total of what is inside it stands in.
        qint64 total = 0;
        QDirIterator it(arrival.path, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            total += QFileInfo(it.next()).size();
        }

        const auto previous = lastSeenSize.value(arrival.path, -1);
        lastSeenSize.insert(arrival.path, total);
        return previous == total;
    }

    const QFileInfo info(arrival.path);
    const auto size = info.size();
    const auto previous = lastSeenSize.value(arrival.path, -1);

    lastSeenSize.insert(arrival.path, size);

    if (previous != size) {
        return false;
    }

    // It also has to be openable for writing, or something else still has hold of it.
    QFile file(arrival.path);
    if (!file.open(QIODevice::ReadWrite)) {
        return false;
    }
    file.close();

    return true;
}

void LibraryIntake::settle()
{
    if (libraryPath.isEmpty()) {
        return;
    }

    const auto arrivals = arrivalsAtTop();
    if (arrivals.isEmpty()) {
        lastSeenSize.clear();
        return;
    }

    auto allSettled = true;
    for (const auto &arrival : arrivals) {
        if (!hasSettled(arrival)) {
            allSettled = false;
        }
    }

    if (!allSettled) {
        settleTimer.start();
        return;
    }

    process();
}

void LibraryIntake::process()
{
    const auto arrivals = arrivalsAtTop();
    const auto series = seriesFolderNames();

    auto filed = 0;
    auto putAside = 0;

    // A library with nothing in it yet is indistinguishable from an enormous drop, and being
    // wrong about that renames every series in it. Once the library knows about even one
    // series, an unknown folder is genuinely an arrival however many of them there are - so
    // emptying a whole download folder in at once works, which counting them did not allow.
    const auto libraryIsEmpty = series.isEmpty();
    if (libraryIsEmpty) {
        QLOG_INFO() << "Library intake leaving folders alone: this library has no series in it yet";
    }

    for (const auto &arrival : arrivals) {
        const QFileInfo info(arrival.path);

        if (arrival.isFolder) {
            if (libraryIsEmpty) {
                continue;
            }

            // A volume range in the folder name is bookkeeping, not part of the title -
            // "A Silent Voice v01-07" is the first seven volumes of A Silent Voice, and
            // filing it as a series of its own is how you end up with two of them. Only the
            // marked form is removed; a bare trailing number stays, because "Zom 100" and
            // "12 Beast" are titles.
            const auto tidied = cleanSeriesDisplayName(withoutVolumeMarker(info.fileName()));

            // The same series arriving again is how a series gets its next few volumes, and
            // it is the ordinary case rather than a problem - so the volumes are moved in
            // rather than the whole folder being set aside for somebody to merge by hand.
            if (QDir(libraryPath).exists(tidied) && tidied != info.fileName()) {
                const auto moved = mergeInto(arrival.path, tidied);
                filed += moved.filed;
                putAside += moved.setAside;
                continue;
            }

            // Otherwise it is a series that is not here yet. It stays where it is and keeps
            // its own volumes; only its name is cleaned, which is the whole of what sorting
            // means for something that is already a series folder.
            if (tidied == info.fileName()) {
                continue;
            }

            if (QDir(libraryPath).rename(info.fileName(), tidied)) {
                note(tr("%1  ->  %2").arg(info.fileName(), tidied));
                filed++;
            } else if (arrival.alreadyFiled) {
                // Already a series in the library. Tidying its name is worth attempting and
                // not worth taking it out of the library over.
                note(tr("%1  ->  left alone (could not be renamed to \"%2\")").arg(info.fileName(), tidied));
            } else {
                putAside += setAside(arrival.path, tr("could not be renamed to \"%1\"").arg(tidied)) ? 1 : 0;
            }
            continue;
        }

        // The series a volume belongs to is the file's own name with the volume number and
        // the release tags taken off it - the same cleaning the metadata lookup uses, so a
        // file lands in the folder whose name the lookup would have matched.
        const auto wanted = cleanSeriesSearchName(seriesNameFromVolumeFileName(info.completeBaseName()));
        if (wanted.isEmpty()) {
            putAside += setAside(arrival.path, tr("no series name could be read from the file name")) ? 1 : 0;
            continue;
        }

        QStringList exact;
        for (const auto &candidate : series) {
            if (SeriesMatchScorer::scoreTitle(wanted, cleanSeriesSearchName(candidate)) >= 95) {
                exact.append(candidate);
            }
        }

        if (exact.size() == 1) {
            if (fileInto(arrival.path, exact.first())) {
                filed++;
            } else {
                putAside += setAside(arrival.path, tr("could not be moved into \"%1\"").arg(exact.first())) ? 1 : 0;
            }
        } else if (exact.isEmpty()) {
            // Not a series in this library. That is a new series rather than a mistake, but
            // one loose file is not a series folder, and inventing one from a single volume
            // gets the name wrong as often as not.
            putAside += setAside(arrival.path, tr("no series here is called \"%1\"").arg(wanted)) ? 1 : 0;
        } else {
            putAside += setAside(arrival.path, tr("\"%1\" matches %2 series here").arg(wanted).arg(exact.size())) ? 1 : 0;
        }
    }

    lastSeenSize.clear();

    if (filed > 0 || putAside > 0) {
        QLOG_INFO() << "Library intake filed" << filed << "and set aside" << putAside;
        emit imported(filed, putAside);
    }
}

// A second copy of a series that is already here: its volumes go in with the ones already
// filed, and the empty folder it came in goes away.
//
// Only flat folders of comics. Anything with subfolders in it is a different shape from what
// this library holds, and taking half of it apart and leaving the rest is worse than not
// starting. Nothing is ever written over: a volume whose name is already in the destination
// is a different scan of the same volume as often as it is the same file, and neither is
// something to settle by overwriting.
LibraryIntake::MergeResult LibraryIntake::mergeInto(const QString &folderPath, const QString &seriesFolder)
{
    MergeResult result;

    const QDir source(folderPath);
    if (!source.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty()) {
        result.setAside = setAside(folderPath, tr("has folders inside it, so \"%1\" was left as it is").arg(seriesFolder)) ? 1 : 0;
        return result;
    }

    const auto files = source.entryInfoList(QDir::Files);
    QStringList collided;
    for (const auto &file : files) {
        if (!looksLikeAComic(file)) {
            continue;
        }
        if (fileInto(file.absoluteFilePath(), seriesFolder)) {
            result.filed++;
        } else {
            collided.append(file.fileName());
        }
    }

    // rmdir only succeeds on an empty directory, so this can never take anything with it.
    if (QDir(libraryPath).rmdir(source.dirName())) {
        note(tr("%1  ->  emptied into %2").arg(source.dirName(), seriesFolder));
        return result;
    }

    const auto reason = collided.isEmpty()
            ? tr("what is left of it does not belong in \"%1\"").arg(seriesFolder)
            : tr("%n volume(s) of it are already in \"%1\"", "", static_cast<int>(collided.size())).arg(seriesFolder);
    result.setAside += setAside(folderPath, reason) ? 1 : 0;

    return result;
}

bool LibraryIntake::fileInto(const QString &sourceFile, const QString &seriesFolder)
{
    const QFileInfo source(sourceFile);
    const QDir destination(QDir(libraryPath).absoluteFilePath(seriesFolder));

    if (!destination.exists()) {
        return false;
    }

    const auto target = destination.absoluteFilePath(source.fileName());
    // Never write over a volume that is already there. Two files of the same name are
    // either the same volume twice or two different scans of it, and neither is something
    // to resolve by overwriting.
    if (QFile::exists(target)) {
        return false;
    }

    if (!QFile::rename(sourceFile, target)) {
        return false;
    }

    note(tr("%1  ->  %2").arg(source.fileName(), seriesFolder));
    return true;
}

bool LibraryIntake::setAside(const QString &path, const QString &reason)
{
    QDir top(libraryPath);
    if (!top.exists(quarantineFolderName()) && !top.mkdir(quarantineFolderName())) {
        return false;
    }

    const QFileInfo source(path);
    const QDir quarantine(top.absoluteFilePath(quarantineFolderName()));

    auto target = quarantine.absoluteFilePath(source.fileName());
    // A name already waiting there is not a reason to overwrite it either.
    auto attempt = 2;
    while (QFile::exists(target) && attempt < 100) {
        target = quarantine.absoluteFilePath(QStringLiteral("%1 (%2).%3").arg(source.completeBaseName()).arg(attempt).arg(source.suffix()));
        attempt++;
    }

    if (QFile::exists(target) || !QFile::rename(path, target)) {
        return false;
    }

    note(tr("%1  ->  %2  (%3)").arg(source.fileName(), quarantineFolderName(), reason));
    return true;
}

// A line per decision, in the library folder, because this moves people's files and the
// only acceptable version of that is one you can read back afterwards.
void LibraryIntake::note(const QString &line)
{
    QFile log(QDir(libraryPath).absoluteFilePath(QStringLiteral("_intake log.txt")));
    if (!log.open(QIODevice::Append | QIODevice::Text)) {
        return;
    }

    QTextStream out(&log);
    out << QDateTime::currentDateTime().toString(Qt::ISODate) << "  " << line << "\n";
}
