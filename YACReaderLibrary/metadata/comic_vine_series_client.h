#ifndef COMIC_VINE_SERIES_CLIENT_H
#define COMIC_VINE_SERIES_CLIENT_H

#include "comic_vine_parsing.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>

// Comic Vine, for the half of a library AniList has never heard of.
//
// AniList is an anime and manga database and it is the right source for manga. It does not
// know Hellboy, or Hellblazer, or Grimm Fairy Tales, and asking it about them returns
// nothing however the question is phrased - so a library with western comics in it leaves
// them permanently unidentified no matter how many times the scrape is run.
//
// What Comic Vine gives and does not give matters to how it is used here. It has the title,
// the description, the year, the issue count and the PUBLISHER. It has no genre: there is no
// such field in the API and no taxonomy behind one. So a western comic tagged from here can
// never be filed under Horror or Action the way a manga is, and pretending otherwise would
// mean inventing the genre. Publisher is what it does know, and publisher is what a comic
// shop shelves by, which is the honest shelf for this material.
//
// Blocking, like the AniList client and for the same reason: the scraper that calls it is
// already on a worker thread going through a queue one series at a time.
namespace YACReader {

class ComicVineSeriesClient : public QObject
{
    Q_OBJECT

public:
    // The parsing lives in comic_vine_parsing.h and is tested there; this adds only the
    // things that need a network to be true.
    struct Response : ComicVineResponse {
        bool timedOut = false;
    };

    explicit ComicVineSeriesClient(QObject *parent = nullptr);

    Response searchSeries(const QString &name, int limit = 10);
    // The fuller description, which the search results do not carry. Worth a second request
    // per series that actually matched, and only for those.
    QString fetchDescription(const QString &volumeId);

private:
    QString requestUrl(const QString &path) const;

    QString baseUrl;
    QString apiKey;
    QString userAgent;
};

}

#endif // COMIC_VINE_SERIES_CLIENT_H
