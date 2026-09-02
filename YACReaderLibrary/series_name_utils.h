#ifndef SERIES_NAME_UTILS_H
#define SERIES_NAME_UTILS_H

#include <QRegularExpression>
#include <QString>
#include <QStringList>

// Folder names in a downloaded library usually carry release metadata that is noise to a
// reader and poison to a metadata lookup: "A Bride's Story (Digital) (1r0n)" matches
// nothing on any comics database.
//
// Two levels of cleaning, because they want different things. The display name keeps
// edition markers, since they are often the only thing telling two folders apart -
// "(Manga UP!)" and "(Square Enix)" can be different releases of the same series. The
// search name drops them too, because the provider is being asked about the series.
namespace YACReader {

namespace SeriesNameUtils {

// Publishers and printings. A tag naming one of these is kept, because it is often the
// only thing separating two real releases: a library can hold "The Ideal Sponger Life"
// (the manga) next to "The Ideal Sponger Life [J-Novel Club]" (the light novel), and
// folding them together loses a distinction the reader is relying on.
inline bool isEditionTag(const QString &tag)
{
    static const QStringList hints = { QStringLiteral("edition"), QStringLiteral("manga up"), QStringLiteral("k manga"), QStringLiteral("comikey"), QStringLiteral("viz"), QStringLiteral("square enix"), QStringLiteral("seven seas"), QStringLiteral("j-novel"), QStringLiteral("kadokawa"), QStringLiteral("yen press"), QStringLiteral("full color"), QStringLiteral("remastered"), QStringLiteral("2-in-1"), QStringLiteral("omnibus"), QStringLiteral("deluxe"), QStringLiteral("colossal") };

    const auto lowered = tag.toLower();
    for (const auto &hint : hints) {
        if (lowered.contains(hint)) {
            return true;
        }
    }
    return false;
}

// Drops a trailing "[...]" group, unless it is the whole name ("[Oshi No Ko]" is a title),
// it names a publisher, or it carries something meaningful like [Decensored]. Only an
// anonymous scanner tag goes, as in "Alfie [InCase]".
inline QString stripTrailingBracketGroup(const QString &name)
{
    const auto trimmed = name.trimmed();
    if (!trimmed.endsWith(QLatin1Char(']'))) {
        return trimmed;
    }

    const auto open = trimmed.lastIndexOf(QLatin1Char('['));
    if (open <= 0) {
        return trimmed;
    }

    const auto inner = trimmed.mid(open + 1, trimmed.length() - open - 2).trimmed();
    if (isEditionTag(inner) || inner.contains(QStringLiteral("decensor"), Qt::CaseInsensitive)) {
        return trimmed;
    }

    const auto remainder = trimmed.left(open).trimmed();
    return remainder.isEmpty() ? trimmed : remainder;
}

// As above but indiscriminate, for building a search string: the provider is being asked
// about the series, so the printing it was published under is noise there.
inline QString stripAnyTrailingBracketGroup(const QString &name)
{
    const auto trimmed = name.trimmed();
    if (!trimmed.endsWith(QLatin1Char(']'))) {
        return trimmed;
    }

    const auto open = trimmed.lastIndexOf(QLatin1Char('['));
    if (open <= 0) {
        return trimmed;
    }

    const auto remainder = trimmed.left(open).trimmed();
    return remainder.isEmpty() ? trimmed : remainder;
}

inline QString stripTrailingParenGroup(const QString &name)
{
    const auto trimmed = name.trimmed();
    if (!trimmed.endsWith(QLatin1Char(')'))) {
        return trimmed;
    }

    const auto open = trimmed.lastIndexOf(QLatin1Char('('));
    if (open <= 0) {
        return trimmed;
    }

    const auto remainder = trimmed.left(open).trimmed();
    return remainder.isEmpty() ? trimmed : remainder;
}

}

// "A Bride's Story (Digital) (1r0n)"                          -> "A Bride's Story"
// "Citrus+ (Digital) (1r0n + danke-Empire)"                   -> "Citrus+"
// "Slightly Older Girlfriend (Manga UP!) (Digital) (1r0n)"    -> "Slightly Older Girlfriend (Manga UP!)"
// "Alfie [InCase]"                                            -> "Alfie"
// "[Oshi No Ko]"                                              -> "[Oshi No Ko]"
inline QString cleanSeriesDisplayName(const QString &name)
{
    auto cleaned = name.trimmed();
    if (cleaned.isEmpty()) {
        return name;
    }

    // Everything from a "(Digital)" marker onwards is release metadata. Anything before
    // it, including an edition marker, is part of how the user tells their folders apart.
    static const QRegularExpression digitalMarker(QStringLiteral("\\s*\\(\\s*digital\\s*\\)"), QRegularExpression::CaseInsensitiveOption);
    const auto match = digitalMarker.match(cleaned);
    if (match.hasMatch()) {
        const auto remainder = cleaned.left(match.capturedStart()).trimmed();
        if (!remainder.isEmpty()) {
            cleaned = remainder;
        }
    }

    cleaned = SeriesNameUtils::stripTrailingBracketGroup(cleaned);

    return cleaned.isEmpty() ? name : cleaned;
}

// As above, and then drops any remaining trailing bracketed or parenthesised groups, so
// the provider is asked about the series rather than one particular printing of it.
// "Slightly Older Girlfriend (Manga UP!)" -> "Slightly Older Girlfriend"
// "[Oshi No Ko]"                          -> "Oshi No Ko"
inline QString cleanSeriesSearchName(const QString &name)
{
    auto cleaned = cleanSeriesDisplayName(name);

    // A title wholly wrapped in brackets is searched without them
    if (cleaned.startsWith(QLatin1Char('[')) && cleaned.endsWith(QLatin1Char(']'))) {
        const auto unwrapped = cleaned.mid(1, cleaned.length() - 2).trimmed();
        if (!unwrapped.isEmpty() && !unwrapped.contains(QLatin1Char('['))) {
            cleaned = unwrapped;
        }
    }

    for (auto pass = 0; pass < 4; ++pass) {
        const auto before = cleaned;
        cleaned = SeriesNameUtils::stripTrailingParenGroup(cleaned);
        cleaned = SeriesNameUtils::stripAnyTrailingBracketGroup(cleaned);
        if (cleaned == before) {
            break;
        }
    }

    return cleaned.isEmpty() ? name : cleaned;
}

// Search names to try, in order, stopping at the first that answers.
//
// A folder name is not always a title. "Bleach - v01-v74 COMPLETE (VIZ Digital)" and
// "Dragon Ball - Digital Colored Comics - Super Arc" and "Naruto Manga Volume" name a
// series and then say something about the scan; searched whole they return nothing at all,
// and the series is reported missing when it is one of the best known in print. Each step
// here gives up a little more of the tail, so a name that was already good is asked first
// and only a name that failed is cut back.
inline QStringList seriesSearchNames(const QString &name)
{
    QStringList names;
    const auto add = [&names](const QString &candidate) {
        const auto trimmed = candidate.trimmed();
        if (!trimmed.isEmpty() && !names.contains(trimmed, Qt::CaseInsensitive)) {
            names.append(trimmed);
        }
    };

    const auto base = cleanSeriesSearchName(name);
    add(base);

    // Accents that the folder carries and the provider's index does not. "Ôoku" and
    // "Polar Bear Café" both find nothing spelled as they are on disk.
    auto folded = base.normalized(QString::NormalizationForm_D);
    static const QRegularExpression combining(QStringLiteral("[\\x{0300}-\\x{036F}]"));
    folded.remove(combining);
    add(folded);

    // What a release group appends after a dash: the volume range, the scan's provenance,
    // the fact that it is complete.
    auto trimmedTail = folded;
    static const QRegularExpression releaseTail(QStringLiteral("\\s+-\\s+(digital|colou?red|complete|official|viz|full[- ]colou?r|v\\d).*$"), QRegularExpression::CaseInsensitiveOption);
    trimmedTail.remove(releaseTail);

    // Words that describe the object rather than name the series, however many are stacked
    // up: "Naruto Manga Volume" is two of them.
    static const QRegularExpression trailingNouns(QStringLiteral("(?:\\s+(manga|volume|volumes|comics|edition|collection|complete))+$"), QRegularExpression::CaseInsensitiveOption);
    trimmedTail.remove(trailingNouns);
    add(trimmedTail);

    // Last resort: whatever stands before the first dash. Enough of these folders are
    // "Series - subtitle nobody indexed" that it is worth one request, and it is tried last
    // because it is the step most likely to find the wrong thing.
    const auto dash = trimmedTail.indexOf(QStringLiteral(" - "));
    if (dash > 0) {
        add(trimmedTail.left(dash));
    }

    if (names.isEmpty()) {
        add(name);
    }

    return names;
}

}

#endif // SERIES_NAME_UTILS_H
