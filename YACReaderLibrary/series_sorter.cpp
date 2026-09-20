#include "series_sorter.h"

#include "QsLog.h"
#include "data_base_management.h"

#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QThread>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

using namespace YACReader;

namespace {

// Moving a series folder is one call, but on Windows it is not always one attempt.
//
// The same renames that failed from inside the application went through instantly from
// PowerShell a few minutes later, which is what a transient lock looks like: the sort runs
// straight after a scrape or a library update has just read every volume in those folders,
// and antivirus and the search indexer follow a burst of reads by holding the folder for a
// few seconds. So a folder that answers "access denied" or "in use" is tried again, with a
// growing pause, before it is reported.
//
// What used to come back was only false, which is why five builds went into this without
// learning anything. Whatever Windows says now goes into the report, by name and number, so
// a failure that survives the retries says what is holding it.
bool moveFolder(const QString &from, const QString &to, int &retryBudgetMs, QString &why)
{
#ifdef Q_OS_WIN
    // Past 260 characters the plain form is refused outright, and a manga library gets there.
    const auto native = [](const QString &path) {
        auto result = QDir::toNativeSeparators(QDir::cleanPath(path));
        if (result.size() >= MAX_PATH - 12 && !result.startsWith(QStringLiteral("\\\\?\\"))) {
            result.prepend(QStringLiteral("\\\\?\\"));
        }
        return result;
    };
    const auto source = native(from);
    const auto target = native(to);

    DWORD error = ERROR_SUCCESS;
    for (auto attempt = 1;; ++attempt) {
        if (MoveFileExW(reinterpret_cast<LPCWSTR>(source.utf16()), reinterpret_cast<LPCWSTR>(target.utf16()), 0)) {
            return true;
        }
        error = GetLastError();

        const auto busy = error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION;
        const auto pause = 250 * attempt;
        // One budget for the whole run, not per folder: if something holds every folder for
        // good, the first few use it up and the rest are reported without the application
        // sitting frozen for minutes.
        if (!busy || attempt >= 6 || pause > retryBudgetMs) {
            break;
        }
        retryBudgetMs -= pause;
        QThread::msleep(pause);
    }

    why = QStringLiteral("%1 (Windows error %2)").arg(qt_error_string(static_cast<int>(error)).trimmed()).arg(error);
    return false;
#else
    Q_UNUSED(retryBudgetMs);
    if (QDir().rename(from, to)) {
        return true;
    }
    why = QStringLiteral("the rename was refused");
    return false;
#endif
}

}

SeriesSorter::SeriesSorter(const QString &libraryPath, const QString &databasePath)
    : libraryPath(libraryPath), databasePath(databasePath)
{
}

QList<SortedSeries> SeriesSorter::pending() const
{
    QList<SortedSeries> ready;

    if (libraryPath.isEmpty() || databasePath.isEmpty()) {
        return ready;
    }

    QString connectionName;
    {
        auto db = DataBaseManagement::loadDatabase(databasePath);
        if (!db.open()) {
            return ready;
        }
        connectionName = db.connectionName();

        struct Loose {
            QString name;
            SeriesTally tally;
        };
        QHash<qulonglong, Loose> loose;

        // Which folders are in play, and how many volumes each has to agree with itself
        // about. Narrowed here so the two counting queries below are read for these only.
        QSqlQuery folders(db);
        folders.prepare("SELECT f.id, f.name, f.path, COUNT(*) "
                        "FROM folder f "
                        "INNER JOIN comic c ON (c.parentId = f.id) "
                        "WHERE f.id <> 1 "
                        "GROUP BY f.id");
        folders.exec();
        while (folders.next()) {
            auto path = folders.value(2).toString();
            while (path.startsWith(QLatin1Char('/'))) {
                path.remove(0, 1);
            }
            if (!isLooseSeriesPath(path)) {
                continue;
            }

            Loose entry;
            entry.name = folders.value(1).toString();
            entry.tally.volumes = folders.value(3).toInt();
            loose.insert(folders.value(0).toULongLong(), entry);
        }

        if (loose.isEmpty()) {
            QSqlDatabase::removeDatabase(connectionName);
            return ready;
        }

        // One row per folder and genre string, with how many volumes carry it. A volume's
        // genre field is itself a comma separated list, so each one is split and every genre
        // in it credited with that row's count.
        QSqlQuery genres(db);
        genres.prepare("SELECT c.parentId, ci.genere, COUNT(*) "
                       "FROM comic c INNER JOIN comic_info ci ON (c.comicInfoId = ci.id) "
                       "WHERE ci.genere IS NOT NULL AND TRIM(ci.genere) <> '' "
                       "GROUP BY c.parentId, ci.genere");
        genres.exec();
        while (genres.next()) {
            const auto folderId = genres.value(0).toULongLong();
            if (!loose.contains(folderId)) {
                continue;
            }
            const auto count = genres.value(2).toInt();
            const auto parts = genres.value(1).toString().split(QLatin1Char(','), Qt::SkipEmptyParts);
            for (const auto &part : parts) {
                const auto genre = part.trimmed();
                if (!genre.isEmpty()) {
                    loose[folderId].tally.genreCounts[genre] += count;
                }
            }
        }

        QSqlQuery publishers(db);
        publishers.prepare("SELECT c.parentId, TRIM(ci.publisher), COUNT(*) "
                           "FROM comic c INNER JOIN comic_info ci ON (c.comicInfoId = ci.id) "
                           "WHERE ci.publisher IS NOT NULL AND TRIM(ci.publisher) <> '' "
                           "GROUP BY c.parentId, TRIM(ci.publisher)");
        publishers.exec();
        while (publishers.next()) {
            const auto folderId = publishers.value(0).toULongLong();
            if (!loose.contains(folderId)) {
                continue;
            }
            loose[folderId].tally.publisherCounts[publishers.value(1).toString()] += publishers.value(2).toInt();
        }

        for (auto it = loose.constBegin(); it != loose.constEnd(); ++it) {
            const auto &tally = it.value().tally;

            SortedSeries entry;
            entry.name = it.value().name;

            const auto section = bookcaseSectionFor(agreedGenres(tally));
            if (section != kUnsortedSection) {
                entry.section = bookcaseSectionName(section);
            } else {
                // A genre is what a manga is shelved by and a publisher is what a comic is
                // shelved by, because Comic Vine - the only source that knows these - has no
                // genres at all. Dark Horse and Zenescope are as real a shelf as Horror, and
                // they come off the data rather than out of a guess.
                const auto publisher = agreedPublisher(tally);
                if (publisher.isEmpty()) {
                    // Nothing the volumes agree on. It stays where it is: the wall shows it
                    // as unidentified, which is honest, and a wrong shelf is worse than an
                    // unsorted one.
                    continue;
                }
                entry.section = publisher;
            }

            ready.append(entry);
        }
    }
    QSqlDatabase::removeDatabase(connectionName);

    return ready;
}

QList<qulonglong> SeriesSorter::looseFolderIds(const QString &databasePath)
{
    QList<qulonglong> ids;

    if (databasePath.isEmpty()) {
        return ids;
    }

    QString connectionName;
    {
        auto db = DataBaseManagement::loadDatabase(databasePath);
        if (!db.open()) {
            return ids;
        }
        connectionName = db.connectionName();

        QSqlQuery query(db);
        query.prepare("SELECT id, path FROM folder WHERE id <> 1 AND id IN (SELECT DISTINCT parentId FROM comic)");
        query.exec();

        while (query.next()) {
            auto path = query.value(1).toString();
            while (path.startsWith(QLatin1Char('/'))) {
                path.remove(0, 1);
            }
            if (isLooseSeriesPath(path)) {
                ids.append(query.value(0).toULongLong());
            }
        }
    }
    QSqlDatabase::removeDatabase(connectionName);

    return ids;
}

QList<SortedSeries> SeriesSorter::sort()
{
    lastProblems.clear();

    QList<SortedSeries> moved;
    const auto ready = pending();
    if (ready.isEmpty()) {
        return moved;
    }

    QDir top(libraryPath);
    const auto unsorted = bookcaseSectionName(kUnsortedSection);
    auto retryBudgetMs = 15000;

    for (const auto &entry : ready) {
        // Wherever it currently is of the two places it can be.
        auto from = top.absoluteFilePath(entry.name);
        if (!QDir(from).exists()) {
            from = top.absoluteFilePath(unsorted + QLatin1Char('/') + entry.name);
        }
        if (!QDir(from).exists()) {
            lastProblems.append(QStringLiteral("%1: could not find it on disk").arg(entry.name));
            continue;
        }

        if (!top.exists(entry.section) && !top.mkpath(entry.section)) {
            lastProblems.append(QStringLiteral("%1: could not make the %2 folder").arg(entry.name, entry.section));
            continue;
        }

        const auto to = top.absoluteFilePath(entry.section + QLatin1Char('/') + entry.name);
        if (QDir(to).exists()) {
            // A series of this name is already shelved there. Merging the two is a decision
            // with volumes at stake and no way to check it afterwards, so it is not made here.
            lastProblems.append(QStringLiteral("%1: something of that name is already in %2").arg(entry.name, entry.section));
            continue;
        }

        QString why;
        if (!moveFolder(from, to, retryBudgetMs, why)) {
            lastProblems.append(QStringLiteral("%1: could not be moved into %2 - %3").arg(entry.name, entry.section, why));
            QLOG_WARN() << "SeriesSorter could not move" << from << "to" << to << "-" << why;
            continue;
        }

        moved.append(entry);
    }

    return moved;
}

QStringList SeriesSorter::problems() const
{
    return lastProblems;
}
