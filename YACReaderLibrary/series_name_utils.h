#ifndef SERIES_NAME_UTILS_H
#define SERIES_NAME_UTILS_H

#include <QRegularExpression>
#include <QString>

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

// Drops a trailing "[...]" group, unless it is the whole name: "[Oshi No Ko]" is a title,
// while "Alfie [InCase]" carries a scanner tag.
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
        cleaned = SeriesNameUtils::stripTrailingBracketGroup(cleaned);
        if (cleaned == before) {
            break;
        }
    }

    return cleaned.isEmpty() ? name : cleaned;
}

}

#endif // SERIES_NAME_UTILS_H
