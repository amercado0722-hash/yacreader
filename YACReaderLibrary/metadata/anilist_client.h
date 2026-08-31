#ifndef ANILIST_CLIENT_H
#define ANILIST_CLIENT_H

#include "series_metadata.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>

// AniList is the metadata source for a manga library, for two reasons that matter to a
// whole-library scrape. It answers a search and the full record for every hit in one
// GraphQL request, so a series costs one call rather than one per volume; and it carries
// the English title, the romaji title and the synonyms together, which is what lets a
// folder named either way find its series.
//
// No API key. The published limit is 90 requests a minute, currently degraded to 30, and
// the server says how long to wait when it refuses - which the caller must honour, or the
// rest of a long run is spent being turned away.
namespace YACReader {

class AniListClient : public QObject
{
    Q_OBJECT

public:
    struct Response {
        QList<SeriesMetadata> candidates;
        bool error = false;
        bool timedOut = false;
        bool rateLimited = false;
        int retryAfterSeconds = 0;
        QString errorString;
    };

    explicit AniListClient(QObject *parent = nullptr);

    // Blocking, because the scraper that calls it is already on a worker thread and its
    // whole job is to go through a queue one series at a time.
    Response searchSeries(const QString &name, int limit = 5);

    // Exposed for testing: turning AniList's answer into our own shape is where the
    // fiddly parts live, and it is worth being able to check them without a network.
    static Response parseResponse(const QByteArray &json);
    static QByteArray buildRequestBody(const QString &name, int limit);
    static QString stripHtml(const QString &text);

private:
    QString endpoint;
    QString userAgent;
};

}

#endif // ANILIST_CLIENT_H
