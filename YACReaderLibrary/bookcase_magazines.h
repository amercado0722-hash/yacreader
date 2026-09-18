#ifndef BOOKCASE_MAGAZINES_H
#define BOOKCASE_MAGAZINES_H

#include <QHash>
#include <QRegularExpression>
#include <QString>

namespace YACReader {

// Turning "COMIC Kairakuten 2016-08" into a magazine and an issue.
//
// A library of one-shots has no series to shelve by, but half of this one records which
// magazine each piece ran in, and that is a second way through the collection: not "what
// else did this artist draw" but "what else was in that issue". Six magazines here carry
// more than four hundred works each, the largest across ninety two issues.
//
// The magazine and the issue arrive as one string in the story arc field, because that is
// how the file names write it. Splitting them is what lets a magazine be a section on the
// wall with its issues standing in order inside it.

// Where the store's name ends and the issue begins. Four shapes appear in practice: a year
// and month, a year and a season, a hash number, and a volume number.
//
// The season is not decoration. A quarterly - COMIC Aoha is the one here - dates its issues
// "2019 Winter", and a pattern that only knows about months makes every season its own
// magazine: fifteen of the forty one on this wall were "COMIC Aoha <season>" before this
// line existed, which is a shelf of one issue each instead of one shelf of fifteen. Both
// orders appear - Aoha writes "2019 Winter" and Gelatin writes "Spring 2009" - so both are
// read, and the sort key puts either into the same order as a monthly.
inline const QRegularExpression &magazineIssuePattern()
{
    static const QRegularExpression pattern(QStringLiteral("\\s*(?:\\b(?:19|20)\\d{2}-\\d{2}\\b|\\b(?:19|20)\\d{2}\\s+(?:Spring|Summer|Fall|Autumn|Winter)\\b|\\b(?:Spring|Summer|Fall|Autumn|Winter)\\s+(?:19|20)\\d{2}\\b|#\\s*\\d+|\\bVol\\.?\\s*\\d+)\\s*"), QRegularExpression::CaseInsensitiveOption);
    return pattern;
}

// A season as the month it starts, so a quarterly's issues fall into the same order as a
// monthly's rather than being sorted alphabetically into Fall, Spring, Summer, Winter.
inline QString monthForSeason(const QString &season)
{
    if (season.compare(QStringLiteral("Spring"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("03");
    }
    if (season.compare(QStringLiteral("Summer"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("06");
    }
    if (season.compare(QStringLiteral("Fall"), Qt::CaseInsensitive) == 0 || season.compare(QStringLiteral("Autumn"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("09");
    }
    if (season.compare(QStringLiteral("Winter"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("12");
    }
    return { };
}

// The magazine on its own, with the issue taken off.
//
// Two shapes of damage in the source names are repaired here, because each one otherwise
// becomes a section of the wall - a sign, a colour and a shelf, for a single book:
//
//   "COMIC Bavel , Bavel"      two names run together, which a comma always means: no
//                              magazine here has one in its title.
//   "COMIC Comic Kairakuten"   the word doubled by whoever typed the file name.
//
// Neither rule invents anything. They both say that a name which cannot be what it claims
// should be read as the name it obviously is.
inline QString magazineTitleFrom(const QString &storyArc)
{
    auto title = storyArc;
    title.remove(magazineIssuePattern());

    const auto comma = title.indexOf(QLatin1Char(','));
    if (comma > 0) {
        title = title.left(comma);
    }

    static const QRegularExpression doubledComic(QStringLiteral("^\\s*(COMIC)\\s+COMIC\\s+"), QRegularExpression::CaseInsensitiveOption);
    title.replace(doubledComic, QStringLiteral("\\1 "));
    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    title = title.replace(whitespace, QStringLiteral(" ")).trimmed();
    while (!title.isEmpty() && QStringLiteral("-,:").contains(title.at(title.size() - 1))) {
        title.chop(1);
    }
    return title.trimmed();
}

// The issue on its own - "2016-08", "50", "3" - or empty when the name carries no issue.
inline QString magazineIssueFrom(const QString &storyArc)
{
    const auto match = magazineIssuePattern().match(storyArc);
    if (!match.hasMatch()) {
        return { };
    }
    auto issue = match.captured(0).trimmed();
    // The marker is punctuation for the reader, not part of the number.
    issue.remove(QRegularExpression(QStringLiteral("^(?:#\\s*|Vol\\.?\\s*)"), QRegularExpression::CaseInsensitiveOption));
    return issue.trimmed();
}

// What the issue sorts by within its magazine.
//
// A year and month sorts as itself, because "2016-08" already orders correctly as text. A
// number has to be padded, or issue 9 lands after issue 89. Anything else goes to the end
// of the magazine under its own name, which is the only honest place for it.
inline QString magazineIssueSortKey(const QString &storyArc)
{
    const auto issue = magazineIssueFrom(storyArc);
    if (issue.isEmpty()) {
        return QStringLiteral("9~") + storyArc;
    }
    static const QRegularExpression dated(QStringLiteral("^(?:19|20)\\d{2}-\\d{2}$"));
    if (dated.match(issue).hasMatch()) {
        return QStringLiteral("1~") + issue;
    }
    static const QRegularExpression seasoned(QStringLiteral("^((?:19|20)\\d{2})\\s+(Spring|Summer|Fall|Autumn|Winter)$"), QRegularExpression::CaseInsensitiveOption);
    const auto seasonMatch = seasoned.match(issue);
    if (seasonMatch.hasMatch()) {
        return QStringLiteral("1~%1-%2").arg(seasonMatch.captured(1), monthForSeason(seasonMatch.captured(2)));
    }
    static const QRegularExpression seasonFirst(QStringLiteral("^(Spring|Summer|Fall|Autumn|Winter)\\s+((?:19|20)\\d{2})$"), QRegularExpression::CaseInsensitiveOption);
    const auto seasonFirstMatch = seasonFirst.match(issue);
    if (seasonFirstMatch.hasMatch()) {
        return QStringLiteral("1~%1-%2").arg(seasonFirstMatch.captured(2), monthForSeason(seasonFirstMatch.captured(1)));
    }
    auto ok = false;
    const auto number = issue.toInt(&ok);
    if (ok) {
        return QStringLiteral("2~%1").arg(number, 6, 10, QLatin1Char('0'));
    }
    return QStringLiteral("9~") + storyArc;
}

// What two spellings have to agree on to be the same magazine: case, and whether whoever
// typed it bothered with the leading "COMIC". This library carries "Weekly Kairakuten" four
// hundred and twenty nine times and "COMIC Weekly Kairakuten" twenty five, and they are one
// magazine; so are "Dascomi" and "COMIC Dascomi", and "Girls forM" and "COMIC Girls forM".
//
// Only the leading word, and only for grouping - the name on the sign is still whichever
// spelling the collection uses most. "COMIC Kairakuten BEAST" reduces to "kairakuten beast"
// and stays a different magazine from "COMIC Kairakuten", which is correct.
inline QString magazineGroupingKey(const QString &title)
{
    auto key = title.toLower().trimmed();
    if (key.startsWith(QStringLiteral("comic "))) {
        key = key.mid(6).trimmed();
    }
    return key;
}

// The same magazine is spelled "COMIC Kairakuten" in one file name and "Comic Kairakuten"
// in the next, which would otherwise put a magazine on the wall twice. Counting the
// spellings and keeping the commonest one settles it without anybody choosing a house style:
// the collection votes, and the majority spelling is the one on the sign.
inline QHash<QString, QString> canonicalMagazineTitles(const QHash<QString, int> &titleCounts)
{
    QHash<QString, int> bestCount;
    QHash<QString, QString> best;

    for (auto it = titleCounts.constBegin(); it != titleCounts.constEnd(); ++it) {
        const auto key = magazineGroupingKey(it.key());
        if (!best.contains(key) || it.value() > bestCount.value(key)) {
            best.insert(key, it.key());
            bestCount.insert(key, it.value());
        }
    }

    QHash<QString, QString> canonical;
    for (auto it = titleCounts.constBegin(); it != titleCounts.constEnd(); ++it) {
        canonical.insert(it.key(), best.value(magazineGroupingKey(it.key()), it.key()));
    }
    return canonical;
}

// Where the works that name no magazine go. Half this library is doujin and one-off
// releases that never ran in one, and they get a section at the end rather than being
// hidden: a view that quietly drops nine thousand books is lying about the library.
inline QString noMagazineSectionName()
{
    return QStringLiteral("No magazine");
}

}

#endif // BOOKCASE_MAGAZINES_H
