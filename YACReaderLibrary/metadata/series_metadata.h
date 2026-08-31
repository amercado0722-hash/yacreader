#ifndef SERIES_METADATA_H
#define SERIES_METADATA_H

#include <QString>
#include <QStringList>

// What a metadata provider can tell us about a series, flattened out of whatever shape the
// provider answers in. Deliberately plain: the scraper, the matcher and the tests all pass
// this around, and none of them should need to know which provider it came from.
namespace YACReader {

struct SeriesMetadata {
    QString providerId;
    QString providerName;

    QString title; // the title to show, English where there is one
    QString romajiTitle;
    QString nativeTitle;
    QStringList synonyms;

    QString synopsis;
    QStringList genres;
    QStringList tags;

    int volumes = 0;
    int chapters = 0;

    int startYear = 0;
    int startMonth = 0;
    int startDay = 0;

    QString status;
    QString countryOfOrigin;
    bool isAdult = false;

    QString writer;
    QString penciller;

    QString siteUrl;
    QString coverUrl;

    bool isValid() const { return !providerId.isEmpty() && !title.isEmpty(); }

    // Every title this series is known by, which is what a name coming off a folder has to
    // be compared against: a library folder may carry the English name, the romaji one, or
    // a synonym none of the official listings lead with.
    QStringList allTitles() const
    {
        QStringList titles;
        for (const auto &candidate : { title, romajiTitle, nativeTitle }) {
            if (!candidate.isEmpty() && !titles.contains(candidate)) {
                titles.append(candidate);
            }
        }
        for (const auto &synonym : synonyms) {
            if (!synonym.isEmpty() && !titles.contains(synonym)) {
                titles.append(synonym);
            }
        }
        return titles;
    }
};

}

#endif // SERIES_METADATA_H
