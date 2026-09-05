#ifndef BOOKCASE_SECTIONS_H
#define BOOKCASE_SECTIONS_H

#include <QList>
#include <QString>
#include <QStringList>

namespace YACReader {

// The sections the bookcase sorts itself into, in the order they stand on the wall.
//
// A series usually carries three or four genres - a battle series is Action and Adventure
// and Fantasy and often Comedy as well - so filing it needs a rule for which one wins.
// Filing by the commonest genre is the obvious rule and the wrong one: measured against a
// real library of nineteen hundred series it produced three enormous sections called Comedy,
// Romance and Drama, because those are what almost everything is partly about.
//
// The rule here is that the most specific genre wins, which is what the order of this list
// is: a series that is Sports goes under Sports whatever else it also is, and only something
// that is nothing but a comedy ends up under Comedy. Against that same library this gives
// sections between seven and three hundred and eighty eight, each of which reads as a
// description of what is on the shelf.
struct BookcaseSection {
    const char *genre;
    // Fixed rather than generated, so a section is always the same colour, and chosen so that
    // NEIGHBOURING sections never come out the same shade - the join between two of them has
    // to be visible from across the room, which is the whole point of sorting the wall. The
    // closest two neighbours here are seventy five degrees apart.
    //
    // Deliberately not unique across the whole list, because it cannot be: nineteen sections
    // each occupying a band fifty six degrees wide need a thousand degrees of colour wheel and
    // there are three hundred and sixty. Music and Supernatural are five degrees apart and
    // that is fine - they are eight sections away from each other, so the two are never in
    // view together. Colour marks the transitions; the sign on the case and the plate in the
    // corner are what name a section. An optimiser will happily produce nineteen evenly spaced
    // hues that satisfy every constraint at once, and it puts Horror in pink and Romance in
    // yellow, which is a worse wall than this one.
    int hue;
};

inline const QList<BookcaseSection> &bookcaseSections()
{
    static const QList<BookcaseSection> sections = {
        { "Hentai", 320 },
        { "Mecha", 200 },
        { "Mahou Shoujo", 285 },
        { "Sports", 95 },
        { "Music", 255 },
        { "Horror", 0 },
        { "Mystery", 215 },
        { "Thriller", 30 },
        { "Psychological", 275 },
        { "Sci-Fi", 185 },
        { "Action", 12 },
        { "Adventure", 120 },
        { "Supernatural", 250 },
        { "Fantasy", 150 },
        { "Ecchi", 265 },
        { "Romance", 340 },
        { "Slice of Life", 70 },
        { "Drama", 225 },
        { "Comedy", 50 },
    };
    return sections;
}

// Where a series with no genres at all goes: the end of the wall, in plain board colours.
// A quarter of a freshly scraped library lands here, and leaving it looking unfinished is
// the honest answer - these are the ones still waiting to be identified.
inline constexpr int kUnsortedSection = -1;

// The section a series belongs to, given every genre its volumes carry between them.
inline int bookcaseSectionFor(const QStringList &genres)
{
    const auto &sections = bookcaseSections();
    for (auto i = 0; i < sections.size(); ++i) {
        const auto genre = QString::fromLatin1(sections.at(i).genre);
        for (const auto &candidate : genres) {
            if (candidate.compare(genre, Qt::CaseInsensitive) == 0) {
                return i;
            }
        }
    }
    return kUnsortedSection;
}

inline QString bookcaseSectionName(int section)
{
    const auto &sections = bookcaseSections();
    if (section < 0 || section >= sections.size()) {
        return QStringLiteral("Not yet identified");
    }
    return QString::fromLatin1(sections.at(section).genre);
}

// Every section name, for anything that has to recognise one of these as a folder on disk
// rather than as a shelf on the wall.
//
// The library folder can be arranged into these same sections, one folder per section with
// the series inside them. Whatever walks that folder has to know the difference between a
// section and a series, because they are both just directories: without this, the drop
// folder sees "Romance" as nineteen hundred unfiled volumes that have just arrived.
inline QStringList bookcaseSectionFolderNames()
{
    QStringList names;
    for (const auto &section : bookcaseSections()) {
        names.append(QString::fromLatin1(section.genre));
    }
    names.append(bookcaseSectionName(kUnsortedSection));
    return names;
}

}

#endif // BOOKCASE_SECTIONS_H
