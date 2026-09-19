#ifndef SERIES_SORTER_H
#define SERIES_SORTER_H

#include "bookcase_sections.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace YACReader {

// Which folders are candidates to be moved into a genre section, and - just as importantly -
// which are not.
//
// Exactly two shapes qualify: a series sitting loose at the top of the library, and a series
// inside "Not yet identified". Anything else is already somewhere on purpose.
//
// The rule is this narrow deliberately. "Light Novels" is a top level folder that is not a
// genre, and a looser rule - anything whose first folder is not a genre - would empty it into
// the genre sections, which is the opposite of what its owner meant by making it.
inline bool isLooseSeriesPath(const QString &relativePath)
{
    const auto parts = relativePath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.size() == 1) {
        return true;
    }
    if (parts.size() == 2) {
        return parts.first() == bookcaseSectionName(kUnsortedSection);
    }
    return false;
}

// What a series' volumes say between them, and what that makes it.
//
// Counted rather than sampled. The first version of this took ANY volume's genre and any
// volume's publisher, which on a real library shelved fourteen volumes of Hellboy under
// Slice of Life because one of them carried that tag from an embedded ComicInfo.xml, and
// filed seventy one Grimm Fairy Tales specials by a publisher named on exactly one of them.
//
// A value has to be on at least half the volumes to speak for the series. That is not a
// tuning knob, it is the definition of the series agreeing with itself: below half, the
// volumes disagree, and the honest answer is to leave the series where it is.
struct SeriesTally {
    int volumes = 0;
    QHash<QString, int> genreCounts;
    QHash<QString, int> publisherCounts;
};

// The genres a majority of the volumes carry.
inline QStringList agreedGenres(const SeriesTally &tally)
{
    QStringList agreed;
    for (auto it = tally.genreCounts.constBegin(); it != tally.genreCounts.constEnd(); ++it) {
        if (it.value() * 2 >= tally.volumes) {
            agreed.append(it.key());
        }
    }
    return agreed;
}

// The publisher a majority of the volumes name, or nothing when they do not agree on one.
inline QString agreedPublisher(const SeriesTally &tally)
{
    QString best;
    auto bestCount = 0;
    for (auto it = tally.publisherCounts.constBegin(); it != tally.publisherCounts.constEnd(); ++it) {
        if (it.value() > bestCount || (it.value() == bestCount && it.key() < best)) {
            best = it.key();
            bestCount = it.value();
        }
    }
    return bestCount * 2 >= tally.volumes ? best : QString();
}

// One series that has just been given a home.
struct SortedSeries {
    QString name;
    QString section;
};

// Moving a newly identified series into the folder for its genre.
//
// The drop folder files what arrives into "Not yet identified", because at that moment the
// only thing known about a series is its file names and guessing a genre from those is not
// something this does. Once its tags have been looked up the guessing is over: the genres
// are on the volumes, and the series can go where it belongs.
//
// Which is the last piece of what was asked for in the first place - drop a folder in and
// have it end up sorted - and until now the step that was still done by hand, with a
// PowerShell script, every time.
class SeriesSorter
{
public:
    SeriesSorter(const QString &libraryPath, const QString &databasePath);

    // Series that are identified and not yet in a section. Reading this without moving
    // anything is what makes the move testable and what lets a caller say what it did.
    QList<SortedSeries> pending() const;

    // Moves them. Returns what actually moved, which is not always what was pending: a
    // folder can fail to move, and one whose name is already taken in the target section is
    // deliberately left where it is rather than merged into it.
    QList<SortedSeries> sort();

    // Series left behind and why, after the last call to sort().
    QStringList problems() const;

    // The folder ids of every series that is loose - at the top of the library or waiting in
    // "Not yet identified". What a look-up triggered by a drop should be limited to.
    //
    // Without this the drop would start a scrape of every untagged series in the library,
    // unattended and with nothing on screen to say so or to stop it. Dropping one folder in
    // should cost one look-up, not a re-scrape of everything that was already skipped once.
    static QList<qulonglong> looseFolderIds(const QString &databasePath);

private:
    QString libraryPath;
    QString databasePath;
    QStringList lastProblems;
};

}

#endif // SERIES_SORTER_H
