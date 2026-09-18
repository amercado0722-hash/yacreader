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

        // The genres of a folder are the genres of its volumes, joined. Each volume's is
        // already a comma separated list and the join uses a comma too, so splitting the
        // result on commas gives every genre the series carries.
        QSqlQuery query(db);
        query.prepare("SELECT f.name, f.path, GROUP_CONCAT(DISTINCT ci.genere) "
                      "FROM folder f "
                      "INNER JOIN comic c ON (c.parentId = f.id) "
                      "INNER JOIN comic_info ci ON (c.comicInfoId = ci.id) "
                      "WHERE f.id <> 1 "
                      "GROUP BY f.id");
        query.exec();

        while (query.next()) {
            auto path = query.value(1).toString();
            while (path.startsWith(QLatin1Char('/'))) {
                path.remove(0, 1);
            }
            if (!isLooseSeriesPath(path)) {
                continue;
            }

            const auto joined = query.value(2).toString();
            if (joined.isEmpty()) {
                continue;
            }

            auto genres = joined.split(QLatin1Char(','), Qt::SkipEmptyParts);
            for (auto &genre : genres) {
                genre = genre.trimmed();
            }

            const auto section = bookcaseSectionFor(genres);
            if (section == kUnsortedSection) {
                // Tagged, but with nothing any section is named after. It stays where it is:
                // the wall shows it as unidentified, which is honest, and a wrong shelf is
                // worse than an unsorted one.
                continue;
            }

            SortedSeries entry;
            entry.name = query.value(0).toString();
            entry.section = bookcaseSectionName(section);
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
