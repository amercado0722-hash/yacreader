#include "filename_tagger.h"

#include "comic_db.h"
#include "data_base_management.h"
#include "db_helper.h"
#include "filename_metadata.h"

#include <QSqlDatabase>
#include <QSqlQuery>

#include <utility>

using namespace YACReader;

namespace {

bool isEmptyField(const QVariant &field)
{
    return !field.isValid() || field.toString().trimmed().isEmpty();
}

// Folders that hold comics. Same rule as the online scraper: a folder of folders is a shelf
// and has nothing of its own to tag. A leading underscore marks the housekeeping folders
// this tool makes - _Duplicates and the like - which are deliberately left out.
QString folderQuery(bool onlyUntitled)
{
    const auto untitledOnly = QStringLiteral(
            " and exists (select 1 from comic c"
            "   join comic_info ci on ci.id = c.comicInfoId"
            "   where c.parentId = folder.id"
            "     and (ci.title is null or trim(ci.title) = ''))");

    return QStringLiteral("select id, name from folder"
                          " where id <> 1"
                          "   and name not like '\\_%' escape '\\'"
                          "   and id in (select distinct parentId from comic)") +
            (onlyUntitled ? untitledOnly : QString()) + QStringLiteral(" order by name");
}

QString comicCountQuery(bool onlyUntitled)
{
    return QStringLiteral("select count(*) from comic c"
                          " join comic_info ci on ci.id = c.comicInfoId"
                          " join folder f on f.id = c.parentId"
                          " where f.id <> 1 and f.name not like '\\_%' escape '\\'") +
            (onlyUntitled ? QStringLiteral(" and (ci.title is null or trim(ci.title) = '')") : QString());
}

}

FilenameTagger::FilenameTagger(const QString &databasePath, QObject *parent)
    : QObject(parent), databasePath(databasePath)
{
}

int FilenameTagger::countComics(const QString &databasePath, bool onlyUntitled)
{
    auto count = 0;

    QString connectionName;
    {
        auto db = DataBaseManagement::loadDatabase(databasePath);
        if (!db.open()) {
            return 0;
        }
        connectionName = db.connectionName();

        QSqlQuery query(db);
        query.prepare(comicCountQuery(onlyUntitled));
        query.exec();
        if (query.next()) {
            count = query.value(0).toInt();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);

    return count;
}

QList<FilenameTagPreview> FilenameTagger::preview(const QString &databasePath, bool onlyUntitled, int limit)
{
    QList<FilenameTagPreview> rows;

    QString connectionName;
    {
        auto db = DataBaseManagement::loadDatabase(databasePath);
        if (!db.open()) {
            return rows;
        }
        connectionName = db.connectionName();

        QSqlQuery query(db);
        query.prepare(QStringLiteral("select c.fileName from comic c"
                                     " join comic_info ci on ci.id = c.comicInfoId"
                                     " join folder f on f.id = c.parentId"
                                     " where f.id <> 1 and f.name not like '\\_%' escape '\\'") +
                      (onlyUntitled ? QStringLiteral(" and (ci.title is null or trim(ci.title) = '')") : QString()) +
                      QStringLiteral(" limit :limit"));
        query.bindValue(QStringLiteral(":limit"), limit);
        query.exec();

        while (query.next()) {
            const auto fileName = query.value(0).toString();
            const auto parsed = parseComicFileName(fileName);

            FilenameTagPreview row;
            row.fileName = fileName;
            row.title = parsed.title;
            row.artist = parsed.artist;
            row.magazine = parsed.magazine;
            rows.append(row);
        }
    }
    QSqlDatabase::removeDatabase(connectionName);

    return rows;
}

void FilenameTagger::setOverwriteExisting(bool overwrite)
{
    overwriteExisting = overwrite;
}

void FilenameTagger::setOnlyUntitled(bool onlyUntitled)
{
    this->onlyUntitled = onlyUntitled;
}

void FilenameTagger::cancel()
{
    cancelled = true;
}

void FilenameTagger::run()
{
    struct Folder {
        qulonglong id = 0;
        QString name;
    };
    QList<Folder> folders;

    QString connectionName;
    {
        auto db = DataBaseManagement::loadDatabase(databasePath);
        if (!db.open()) {
            emit finished(0, 0);
            return;
        }
        connectionName = db.connectionName();

        QSqlQuery query(db);
        query.prepare(folderQuery(onlyUntitled));
        query.exec();
        while (query.next()) {
            folders.append({ query.value(0).toULongLong(), query.value(1).toString() });
        }
    }
    QSqlDatabase::removeDatabase(connectionName);

    auto tagged = 0;
    auto visited = 0;
    const auto total = static_cast<int>(folders.size());

    // A folder at a time rather than a comic at a time: the comics of a folder are loaded
    // and written in one go each, which on eighteen thousand files is the difference between
    // a run that takes seconds and one that takes an hour.
    for (const auto &folder : std::as_const(folders)) {
        if (cancelled) {
            break;
        }

        emit progress(visited, total, folder.name);
        visited++;

        QList<ComicDB> comics;
        QString folderConnection;
        {
            auto db = DataBaseManagement::loadDatabase(databasePath);
            if (!db.open()) {
                continue;
            }
            folderConnection = db.connectionName();

            const auto items = DBHelper::getComicsFromParent(folder.id, db, false);
            for (auto *item : items) {
                auto *comic = static_cast<ComicDB *>(item);
                comics.append(*comic);
                delete comic;
            }
        }
        QSqlDatabase::removeDatabase(folderConnection);

        QList<ComicDB> changed;
        for (auto &comic : comics) {
            if (onlyUntitled && !isEmptyField(comic.info.title)) {
                continue;
            }

            const auto parsed = parseComicFileName(comic.name);
            if (parsed.isEmpty()) {
                continue;
            }

            const auto set = [this](QVariant &field, const QVariant &value) {
                if (value.toString().trimmed().isEmpty()) {
                    return;
                }
                if (overwriteExisting || isEmptyField(field)) {
                    field = value;
                }
            };

            set(comic.info.title, parsed.title);
            // The artist drew it, so writer and penciller are the same person here. Both are
            // filled because the two views of a comic show different ones.
            set(comic.info.writer, parsed.artist);
            set(comic.info.penciller, parsed.artist);
            set(comic.info.publisher, parsed.publisher);
            set(comic.info.languageISO, parsed.language);
            // The magazine is the closest thing these one-shots have to a series, and it is
            // what makes a run of them sortable into the order they were published.
            set(comic.info.storyArc, parsed.magazine);
            set(comic.info.number, parsed.issue);
            set(comic.info.notes, parsed.flags.join(QStringLiteral(", ")));

            if (!parsed.date.isEmpty()) {
                set(comic.info.date, parsed.date);
            }
            if (parsed.year > 0) {
                set(comic.info.year, QString::number(parsed.year));
            }
            if (parsed.month > 0) {
                set(comic.info.month, QString::number(parsed.month));
            }

            comic.info.edited = true;
            changed.append(comic);
        }

        if (!changed.isEmpty()) {
            DBHelper::updateComicsInfo(changed, databasePath);
            tagged += static_cast<int>(changed.size());
        }
    }

    emit progress(visited, total, QString());
    emit finished(tagged, visited);
}
