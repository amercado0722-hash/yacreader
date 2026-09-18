#ifndef FILENAME_METADATA_H
#define FILENAME_METADATA_H

#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace YACReader {

// What a comic's file name says about it.
//
// Most of a library gets its tags from a metadata provider, and that is the right source
// when the provider knows the work. Some collections it never will: a pack of seventeen
// thousand one-shots by seventeen hundred artists is not in any series database, and
// looking it up returns nothing seventeen thousand times.
//
// Those files are not untagged though. The people who assembled them put the tags in the
// name, to a convention followed closely enough to parse:
//
//     [Artist] Title (COMIC Kairakuten 2019-05) (x3200) [FAKKU]
//     (C85) [Circle (Artist)] Title (Parody) [English] [Decensored]
//
// Measured against a real collection of eighteen thousand files this finds the artist on
// 99.9% of them, a title on all of them, the magazine on half and a date on a third. No
// synopsis, because a file name has never contained one - but a shelf of clean titles and
// artists beats a shelf of hundred-and-fifty-character file names, which is what those
// comics show without this.
struct FilenameMetadata {
    QString artist;
    QString title;
    // The magazine an issue came out of, and the issue itself: "COMIC Kairakuten 2019-05"
    // gives a magazine and a date, "COMIC X-Eros #50" gives a magazine and a number.
    QString magazine;
    QString issue;
    // The issue date, in the day/month/year the rest of the library writes, and broken out
    // because the comic's info keeps the three parts separately as well.
    QString date;
    int year = 0;
    int month = 0;
    QString publisher;
    QString language;
    // Decensored, Digital, Colorized and the like - things about this particular copy
    // rather than about the work.
    QStringList flags;

    bool isEmpty() const
    {
        return artist.isEmpty() && title.isEmpty() && magazine.isEmpty() && publisher.isEmpty() && flags.isEmpty();
    }
};

namespace detail {

// A name in brackets is only sometimes the publisher. Recognising the few that are keeps
// scanlation groups and release tags out of the publisher field, where they would be worse
// than nothing.
inline const QStringList &knownPublishers()
{
    static const QStringList publishers = {
        QStringLiteral("FAKKU"),
        // FAKKU's own commissions rather than a magazine reprint - a real distinction in
        // this collection, and forty five files say so in as many words.
        QStringLiteral("FAKKU Original"),
        QStringLiteral("FAKKU & Irodori Comics"),
        QStringLiteral("FAKKU & 2D Market"),
        QStringLiteral("Irodori Comics"),
        QStringLiteral("2D Market"),
        QStringLiteral("Project Hentai"),
        QStringLiteral("Doujins.com"),
    };
    return publishers;
}

// Things said about a copy rather than about the work. Kept apart from the title because
// they repeat across thousands of files and would drown it.
inline const QStringList &knownFlags()
{
    static const QStringList flags = {
        QStringLiteral("Decensored"),
        QStringLiteral("Uncensored"),
        QStringLiteral("Censored"),
        QStringLiteral("Digital"),
        QStringLiteral("Colorized"),
        QStringLiteral("Color"),
        QStringLiteral("Sample"),
        QStringLiteral("Complete"),
        QStringLiteral("Ongoing"),
    };
    return flags;
}

struct LanguageName {
    const char *name;
    const char *isoCode;
};

// Language names as they appear in these names, and the ISO code each one means.
inline const QList<LanguageName> &knownLanguages()
{
    static const QList<LanguageName> languages = {
        { "English", "en" },
        { "Japanese", "ja" },
        { "Chinese", "zh" },
        { "Korean", "ko" },
        { "Spanish", "es" },
        { "French", "fr" },
        { "German", "de" },
        { "Russian", "ru" },
    };
    return languages;
}

// What makes a parenthesised group a magazine rather than part of the title. Deliberately a
// list of things magazines are called plus the two shapes an issue takes, because the
// alternative - treating every parenthesised group as a magazine - files "(Kantai
// Collection)" as a magazine on every doujin in the library.
inline const QRegularExpression &magazinePattern()
{
    static const QRegularExpression pattern(QStringLiteral("(COMIC|Kairakuten|X-Eros|Bavel|Anthurium|HOTMiLK|Shitsurakuten|Megastore|Tenma|Penguin Club|Mujin|Girls ?forM|Dolce|Failure|Magazine|Monthly|Weekly|\\b(?:19|20)\\d{2}-\\d{2}\\b|#\\d+)"), QRegularExpression::CaseInsensitiveOption);
    return pattern;
}

}

// The metadata a comic's file name carries. Give it the name only - a path would put the
// folder's brackets and parentheses into the parse.
inline FilenameMetadata parseComicFileName(const QString &fileName)
{
    FilenameMetadata metadata;

    auto stem = fileName;
    const auto dot = stem.lastIndexOf(QLatin1Char('.'));
    if (dot > 0) {
        stem = stem.left(dot);
    } else if (dot == 0) {
        // A name that is nothing but an extension has no metadata in it, and leaving the
        // extension standing would make "cbz" the title of every such file.
        stem.clear();
    }
    stem = stem.trimmed();
    if (stem.isEmpty()) {
        return metadata;
    }

    // "(C85)" and friends: the convention marks the event a doujin was sold at, and it sits
    // in front of the artist. Taking it off first is what lets the artist still be found.
    static const QRegularExpression event(QStringLiteral("^\\s*\\((?:C\\d{2,3}|COMIC1[^)]*|Reitaisai[^)]*)\\)\\s*"), QRegularExpression::CaseInsensitiveOption);
    stem.remove(event);

    // Scan resolution, which says nothing about the work. An older run of these names writes
    // it bare rather than in parentheses - "Secret_x3200_FAKKU" - so both shapes go.
    static const QRegularExpression resolution(QStringLiteral("\\(\\s*x\\s*\\d{3,5}\\s*\\)|(?:^|[\\s_])x\\d{3,5}(?=$|[\\s_])"), QRegularExpression::CaseInsensitiveOption);
    stem.replace(resolution, QStringLiteral(" "));

    // The leading bracket is the artist. Later brackets are tags, which is why this only
    // looks at the front: "[Artist] Title [English]" must not make English an artist.
    static const QRegularExpression leadingArtist(QStringLiteral("^\\s*\\[([^\\]]+)\\]\\s*"));
    const auto artistMatch = leadingArtist.match(stem);
    if (artistMatch.hasMatch()) {
        metadata.artist = artistMatch.captured(1).trimmed();
        stem = stem.mid(artistMatch.capturedLength()).trimmed();
    }

    const auto classify = [&metadata](const QString &value) {
        for (const auto &publisher : detail::knownPublishers()) {
            if (value.compare(publisher, Qt::CaseInsensitive) == 0) {
                metadata.publisher = publisher;
                return true;
            }
        }
        for (const auto &language : detail::knownLanguages()) {
            if (value.compare(QString::fromLatin1(language.name), Qt::CaseInsensitive) == 0) {
                metadata.language = QString::fromLatin1(language.isoCode);
                return true;
            }
        }
        for (const auto &flag : detail::knownFlags()) {
            if (value.compare(flag, Qt::CaseInsensitive) == 0) {
                if (!metadata.flags.contains(flag, Qt::CaseInsensitive)) {
                    metadata.flags.append(flag);
                }
                return true;
            }
        }
        return false;
    };

    // Every remaining bracket is a tag of some kind. The ones worth keeping are recognised;
    // the rest are scanlation groups and release names, and they come off the title either
    // way because leaving them on it is the thing this is meant to fix.
    static const QRegularExpression bracket(QStringLiteral("\\[([^\\]]*)\\]"));
    auto bracketMatches = bracket.globalMatch(stem);
    while (bracketMatches.hasNext()) {
        classify(bracketMatches.next().captured(1).trimmed());
    }
    stem.remove(bracket);

    // Parentheses are the ambiguous ones: a magazine, a flag, or part of the title. Only the
    // first two are taken away, so an unrecognised group stays where the namer put it.
    static const QRegularExpression paren(QStringLiteral("\\(([^()]*)\\)"));
    QStringList consumed;
    auto parenMatches = paren.globalMatch(stem);
    while (parenMatches.hasNext()) {
        const auto match = parenMatches.next();
        const auto value = match.captured(1).trimmed();
        if (value.isEmpty()) {
            consumed.append(match.captured(0));
            continue;
        }
        if (classify(value)) {
            consumed.append(match.captured(0));
            continue;
        }
        if (detail::magazinePattern().match(value).hasMatch()) {
            // Every group that looks like a magazine comes off the title, but only the first
            // becomes the magazine. A few names carry both the anthology a piece was
            // collected in and the magazine it first ran in, and leaving the second one in
            // the title is the worst of the three available answers.
            if (metadata.magazine.isEmpty()) {
                metadata.magazine = value;
            }
            consumed.append(match.captured(0));
        }
    }
    for (const auto &group : consumed) {
        stem.remove(group);
    }

    if (!metadata.magazine.isEmpty()) {
        // "COMIC Kairakuten 2019-05" is a date; "COMIC X-Eros #50" is a number. Both are the
        // issue, and only the first can also be a date.
        static const QRegularExpression issueDate(QStringLiteral("\\b((?:19|20)\\d{2})-(\\d{2})\\b"));
        static const QRegularExpression issueNumber(QStringLiteral("#\\s*(\\d+)"));
        const auto dateMatch = issueDate.match(metadata.magazine);
        if (dateMatch.hasMatch()) {
            metadata.issue = dateMatch.captured(0);
            // day/month/year, which is what the rest of the library writes into this field.
            // The issue gives no day, and the first is the convention for a monthly.
            metadata.date = QStringLiteral("01/%1/%2").arg(dateMatch.captured(2), dateMatch.captured(1));
            metadata.year = dateMatch.captured(1).toInt();
            metadata.month = dateMatch.captured(2).toInt();
        } else {
            const auto numberMatch = issueNumber.match(metadata.magazine);
            if (numberMatch.hasMatch()) {
                metadata.issue = numberMatch.captured(1);
            }
        }
    }

    // A tag whose opening bracket was never typed - "Title (x3200) FAKKU]" - leaves the
    // publisher's name stranded in the title with a bracket hanging off it. Rare, but a
    // name typed by hand eighteen thousand times will have a few of these, and the rule that
    // fixes it is honest on its own terms: a publisher's name at the end of a title, outside
    // any bracket, is a malformed tag rather than something the work is called.
    static const QRegularExpression strayBracket(QStringLiteral("[\\[\\]]"));
    stem.remove(strayBracket);
    for (const auto &publisher : detail::knownPublishers()) {
        if (stem.trimmed().endsWith(publisher, Qt::CaseInsensitive)) {
            stem = stem.trimmed();
            stem.chop(publisher.size());
            if (metadata.publisher.isEmpty()) {
                metadata.publisher = publisher;
            }
            break;
        }
    }

    // A name with no spaces in it and underscores between the words is using them as spaces.
    // That is the only reading, so the title may as well be readable.
    if (stem.count(QLatin1Char('_')) >= 2 && stem.count(QLatin1Char('_')) > stem.count(QLatin1Char(' '))) {
        stem.replace(QLatin1Char('_'), QLatin1Char(' '));
    }

    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    metadata.title = stem.replace(whitespace, QStringLiteral(" ")).trimmed();
    // Separators left stranded by a group that has just been taken out, and nothing else. A
    // full stop is pointedly not in this set: "Don't Avert Your Eyes." ends in one because
    // the title does, and trimming it turns a real title into a slightly wrong one.
    static const QString strays = QStringLiteral("-_,;: ");
    while (!metadata.title.isEmpty() && strays.contains(metadata.title.at(metadata.title.size() - 1))) {
        metadata.title.chop(1);
    }
    while (!metadata.title.isEmpty() && strays.contains(metadata.title.at(0))) {
        metadata.title.remove(0, 1);
    }

    return metadata;
}

}

#endif // FILENAME_METADATA_H
