#ifndef BATCH_SCRAPER_H
#define BATCH_SCRAPER_H

#include "series_match_scorer.h"
#include "series_metadata.h"

#include <QList>
#include <QObject>
#include <QString>

#include <atomic>

class ComicInfo;

// Scraping a library one series at a time, unattended.
//
// The design rule throughout: a run must never need a person while it is going, and must
// never guess. Anything the matcher is not sure of is set aside with its candidates and
// shown at the end, so a thousand series can be scraped while nobody is watching and the
// twenty doubtful ones are settled in one sitting afterwards.
namespace YACReader {

struct ScrapeTarget {
    qulonglong folderId = 0;
    QString folderName; // as it is on disk
    QString searchName; // the folder name with its release tags removed
};

struct ScrapeOutcome {
    enum Result {
        Applied,
        NeedsReview,
        NotFound,
        Failed,
        Cancelled
    };

    ScrapeTarget target;
    Result result = Cancelled;
    QList<SeriesMatch> candidates;
    int comicsUpdated = 0;
    QString message;
};

class BatchScraper : public QObject
{
    Q_OBJECT

public:
    explicit BatchScraper(const QString &databasePath, QObject *parent = nullptr);

    // Every folder in the library that actually holds comics, which is what a series is in
    // a downloaded library. Folders that only contain other folders are skipped: they are
    // shelves, not series, and looking them up would waste a request each.
    // onlyMissing leaves out the series that already have a synopsis, which is what makes a
    // second run cost seconds rather than the hour a first one takes.
    static QList<ScrapeTarget> targetsForLibrary(const QString &databasePath, bool onlyMissing = true);

    void setTargets(const QList<ScrapeTarget> &targets);
    // Off by default: a library that has been tagged by hand should not have that work
    // overwritten by a bulk run nobody was watching.
    void setOverwriteExisting(bool overwrite);
    void setRequestIntervalMs(int intervalMs);

    // Writes a matched series onto every comic in a folder. Public so the review pass can
    // apply a choice the user made without going back through the search.
    ScrapeOutcome applyToFolder(const ScrapeTarget &target, const SeriesMetadata &series);

public slots:
    // Blocking. Meant to be run on a worker thread.
    void run();
    void cancel();

signals:
    void progress(int done, int total, const QString &currentName);
    void folderFinished(const YACReader::ScrapeOutcome &outcome);
    // Emitted when the provider has asked us to slow down, so the UI can say so rather
    // than looking as though it has hung.
    void waiting(int seconds, const QString &reason);
    void finished(int applied, int needsReview, int notFound, int failed);

private:
    bool sleepInterruptibly(int milliseconds);
    void writeInfo(ComicInfo &info, const SeriesMetadata &series, const QString &fileName) const;

    QString databasePath;
    QList<ScrapeTarget> targets;
    bool overwriteExisting = false;
    // AniList publishes 90 requests a minute and is currently serving 30. Two seconds
    // stays inside the degraded limit with room to spare; being refused costs far more
    // time than going slowly does.
    int requestIntervalMs = 2000;
    std::atomic<bool> cancelled { false };
};

}

Q_DECLARE_METATYPE(YACReader::ScrapeOutcome)

#endif // BATCH_SCRAPER_H
