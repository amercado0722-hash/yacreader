#include "batch_scraper.h"

#include "anilist_client.h"
#include "comic_db.h"
#include "data_base_management.h"
#include "db_helper.h"
#include "series_name_utils.h"
#include "volume_number_utils.h"

#include <QElapsedTimer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QThread>

#include <utility>

using namespace YACReader;

namespace {

// Two letter language codes for the places AniList reports. Anything else is left alone
// rather than guessed at.
QString languageForCountry(const QString &countryOfOrigin)
{
    if (countryOfOrigin == QStringLiteral("JP")) {
        return QStringLiteral("ja");
    }
    if (countryOfOrigin == QStringLiteral("KR")) {
        return QStringLiteral("ko");
    }
    if (countryOfOrigin == QStringLiteral("CN") || countryOfOrigin == QStringLiteral("TW")) {
        return QStringLiteral("zh");
    }
    return { };
}

bool isEmptyField(const QVariant &field)
{
    return !field.isValid() || field.toString().trimmed().isEmpty();
}

}

BatchScraper::BatchScraper(const QString &databasePath, QObject *parent)
    : QObject(parent), databasePath(databasePath)
{
}

// Only the series that still need it, unless asked for all of them.
//
// A run over a library this size takes an hour, because the provider is asked about one
// series every two seconds and being polite about that is not optional. Asking again about
// nineteen hundred series to find the four that arrived yesterday is an hour for nothing,
// and it is the difference between a scrape you run when you remember to and one that can
// follow every import without anybody thinking about it.
//
// "Still needs it" is a series with at least one volume that has no synopsis. Folders whose
// name begins with an underscore are left alone throughout - those are the ones this
// application's own housekeeping puts aside, and they are not series.
QList<ScrapeTarget> BatchScraper::targetsForLibrary(const QString &databasePath, bool onlyMissing)
{
    QList<ScrapeTarget> targets;

    QString connectionName;
    {
        auto db = DataBaseManagement::loadDatabase(databasePath);
        if (!db.open()) {
            return targets;
        }
        connectionName = db.connectionName();

        const auto missingOnly = QStringLiteral(
                " and exists (select 1 from comic c"
                "   join comic_info ci on ci.id = c.comicInfoId"
                "   where c.parentId = folder.id"
                "     and (ci.synopsis is null or trim(ci.synopsis) = ''))");

        QSqlQuery query(db);
        query.prepare(QStringLiteral("select id, name from folder"
                                     " where id <> 1"
                                     "   and name not like '\\_%' escape '\\'"
                                     "   and id in (select distinct parentId from comic)") +
                      (onlyMissing ? missingOnly : QString()) + QStringLiteral(" order by name"));
        query.exec();

        while (query.next()) {
            ScrapeTarget target;
            target.folderId = query.value(0).toULongLong();
            target.folderName = query.value(1).toString();
            target.searchName = cleanSeriesSearchName(target.folderName);
            if (!target.searchName.trimmed().isEmpty()) {
                targets.append(target);
            }
        }
    }
    QSqlDatabase::removeDatabase(connectionName);

    return targets;
}

void BatchScraper::setTargets(const QList<ScrapeTarget> &targets)
{
    this->targets = targets;
}

void BatchScraper::setOverwriteExisting(bool overwrite)
{
    overwriteExisting = overwrite;
}

void BatchScraper::setRequestIntervalMs(int intervalMs)
{
    requestIntervalMs = qMax(0, intervalMs);
}

void BatchScraper::cancel()
{
    cancelled = true;
}

// Waiting in short slices so that cancelling a run that is part way through a rate limit
// pause does not leave the user staring at a dialog for another minute.
bool BatchScraper::sleepInterruptibly(int milliseconds)
{
    static constexpr auto kSliceMs = 100;

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds) {
        if (cancelled) {
            return false;
        }
        QThread::msleep(kSliceMs);
    }

    return !cancelled;
}

void BatchScraper::writeInfo(ComicInfo &info, const SeriesMetadata &series, const QString &fileName) const
{
    const auto set = [this](QVariant &field, const QVariant &value) {
        if (value.toString().trimmed().isEmpty()) {
            return;
        }
        if (overwriteExisting || isEmptyField(field)) {
            field = value;
        }
    };

    set(info.series, series.title);
    if (!series.romajiTitle.isEmpty() && series.romajiTitle != series.title) {
        set(info.alternateSeries, series.romajiTitle);
    }

    set(info.synopsis, series.synopsis);
    set(info.genere, series.genres.join(QStringLiteral(", ")));
    set(info.tags, series.tags.join(QStringLiteral(", ")));
    set(info.writer, series.writer);
    set(info.penciller, series.penciller);
    set(info.languageISO, languageForCountry(series.countryOfOrigin));

    if (series.volumes > 0) {
        set(info.count, QString::number(series.volumes));
    }
    if (series.startYear > 0) {
        set(info.year, QString::number(series.startYear));
        if (series.startMonth > 0) {
            set(info.month, QString::number(series.startMonth));
        }
        if (series.startDay > 0) {
            set(info.day, QString::number(series.startDay));
        }
    }
    if (series.isAdult) {
        set(info.ageRating, QStringLiteral("Adult"));
    }

    // The volume number lives in the file name and nowhere else, so it is read per file
    // rather than taken from the series. A one shot legitimately has none.
    const auto number = normalizedVolumeNumber(volumeNumberFromFileName(fileName));
    if (!number.isEmpty()) {
        set(info.number, number);
    }

    info.edited = true;
}

ScrapeOutcome BatchScraper::applyToFolder(const ScrapeTarget &target, const SeriesMetadata &series)
{
    ScrapeOutcome outcome;
    outcome.target = target;
    outcome.result = ScrapeOutcome::Failed;

    QList<ComicDB> comics;
    QString connectionName;
    {
        auto db = DataBaseManagement::loadDatabase(databasePath);
        if (!db.open()) {
            outcome.message = QObject::tr("Could not open the library database");
            return outcome;
        }
        connectionName = db.connectionName();

        const auto items = DBHelper::getComicsFromParent(target.folderId, db, false);
        for (auto *item : items) {
            auto *comic = static_cast<ComicDB *>(item);
            comics.append(*comic);
            delete comic;
        }
    }
    QSqlDatabase::removeDatabase(connectionName);

    if (comics.isEmpty()) {
        outcome.result = ScrapeOutcome::NotFound;
        outcome.message = QObject::tr("No comics in this folder");
        return outcome;
    }

    for (auto &comic : comics) {
        writeInfo(comic.info, series, comic.name);
    }

    DBHelper::updateComicsInfo(comics, databasePath);

    outcome.result = ScrapeOutcome::Applied;
    outcome.comicsUpdated = static_cast<int>(comics.size());
    return outcome;
}

void BatchScraper::run()
{
    AniListClient client;

    auto applied = 0;
    auto needsReview = 0;
    auto notFound = 0;
    auto failed = 0;
    auto done = 0;
    const auto total = static_cast<int>(targets.size());

    for (const auto &target : std::as_const(targets)) {
        if (cancelled) {
            break;
        }

        emit progress(done, total, target.folderName);

        // A folder name that answers nothing is asked again with a little more of its tail
        // cut off. Only a name that failed costs a second request, so a library of clean
        // names still makes exactly one request per series.
        const auto searchNames = seriesSearchNames(target.folderName);

        AniListClient::Response response;
        QList<SeriesMatch> ranked;
        auto usedName = target.searchName;

        for (auto attemptIndex = 0; attemptIndex < searchNames.size(); ++attemptIndex) {
            if (cancelled) {
                break;
            }

            const auto searchName = searchNames.at(attemptIndex);
            usedName = searchName;
            response = client.searchSeries(searchName);

            // Being told to slow down is not a failure. Wait exactly as long as we were
            // asked to and put the same series back through, rather than losing it from the
            // run.
            auto rateLimitRetries = 0;
            while (response.rateLimited && rateLimitRetries < 3 && !cancelled) {
                const auto seconds = qMax(1, response.retryAfterSeconds);
                emit waiting(seconds, tr("waiting for the provider's rate limit"));
                if (!sleepInterruptibly(seconds * 1000)) {
                    break;
                }
                response = client.searchSeries(searchName);
                rateLimitRetries++;
            }

            if (response.error) {
                break;
            }

            const auto attempt = rankSeriesMatches(searchName, response.candidates);
            if (!attempt.isEmpty() && (ranked.isEmpty() || attempt.first().score > ranked.first().score)) {
                ranked = attempt;
            }

            if (!ranked.isEmpty() && ranked.first().confident) {
                break;
            }

            if (attemptIndex + 1 < searchNames.size()) {
                sleepInterruptibly(requestIntervalMs);
            }
        }

        if (cancelled) {
            break;
        }

        ScrapeOutcome outcome;
        outcome.target = target;

        if (response.error) {
            outcome.result = ScrapeOutcome::Failed;
            outcome.message = response.errorString;
            failed++;
        } else if (ranked.isEmpty()) {
            outcome.result = ScrapeOutcome::NotFound;
            outcome.message = tr("Nothing found for \"%1\"").arg(usedName);
            notFound++;
        } else {
            outcome.candidates = ranked;

            if (ranked.first().confident) {
                auto writeOutcome = applyToFolder(target, ranked.first().series);
                writeOutcome.candidates = ranked;
                outcome = writeOutcome;
                if (outcome.result == ScrapeOutcome::Applied) {
                    applied++;
                } else {
                    failed++;
                }
            } else {
                outcome.result = ScrapeOutcome::NeedsReview;
                outcome.message = tr("More than one series could be this one");
                needsReview++;
            }
        }

        done++;
        emit folderFinished(outcome);
        emit progress(done, total, target.folderName);

        if (done < total && !cancelled) {
            sleepInterruptibly(requestIntervalMs);
        }
    }

    emit finished(applied, needsReview, notFound, failed);
}
