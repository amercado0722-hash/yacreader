#ifndef SERIES_SORTER_H
#define SERIES_SORTER_H

#include "bookcase_sections.h"

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
