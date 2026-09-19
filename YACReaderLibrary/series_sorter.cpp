#include "series_sorter.h"

#include "QsLog.h"
#include "data_base_management.h"

#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>

using namespace YACReader;

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

        if (!QDir().rename(from, to)) {
            lastProblems.append(QStringLiteral("%1: could not be moved into %2").arg(entry.name, entry.section));
            QLOG_WARN() << "SeriesSorter could not move" << from << "to" << to;
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
