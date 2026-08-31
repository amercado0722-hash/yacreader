#ifndef VOLUME_NUMBER_UTILS_H
#define VOLUME_NUMBER_UTILS_H

#include <QRegularExpression>
#include <QString>

// A downloaded library carries the volume or chapter number in the file name and nowhere
// else, so a scraper that wants to write "this is volume 7" has to read it back out of
// "A Bride's Story v07 (2024).cbz". The shapes below all occur in a real library:
//
//     A Bride's Story v07 (2024).cbz              -> 7
//     Booty Royale v01-02 (2021).cbz              -> 1   (an omnibus starts at its first)
//     One-Punch Man 207 (2025).cbz                -> 207
//     My Clueless First Friend 001 (2022).cbz     -> 1
//     Insomniacs After School 125.1 (2023).cbz    -> 125.1
//     Some Series Vol. 3.cbz                      -> 3
//
// The year in parentheses is the trap: it is a number, it sits at the end, and taking it
// would tag every file in a series as volume 2024.
namespace YACReader {

namespace VolumeNumberUtils {

// Trailing "(2024)" and "(2024-2025)" groups, which are publication years rather than
// anything to do with the numbering.
inline QString withoutYearGroups(const QString &name)
{
    static const QRegularExpression yearGroup(QStringLiteral("\\s*\\((?:19|20)\\d{2}(?:\\s*-\\s*(?:19|20)\\d{2})?\\)"));
    auto stripped = name;
    stripped.remove(yearGroup);
    return stripped.trimmed();
}

}

// Returns the volume or chapter number encoded in a file name, or an empty string when the
// name carries no number at all. The result is kept as a string because numbering is not
// always integral - "125.1" is a real chapter, and ComicDB stores the number as text.
inline QString volumeNumberFromFileName(const QString &fileName)
{
    auto base = fileName.trimmed();

    const auto lastDot = base.lastIndexOf(QLatin1Char('.'));
    // Only treat it as an extension if what follows looks like one, so the "125.1" of a
    // half chapter survives a file that somehow lost its suffix. A name ending in a dot
    // has nothing after it to inspect, hence the bound on lastDot.
    if (lastDot > 0 && lastDot < base.length() - 1 && base.length() - lastDot <= 5 && !base.at(lastDot + 1).isDigit()) {
        base = base.left(lastDot);
    }

    base = VolumeNumberUtils::withoutYearGroups(base);

    // An explicit volume marker wins wherever it appears: "v07", "v01-02", "Vol. 3".
    static const QRegularExpression volumeMarker(QStringLiteral("\\bv(?:ol)?\\.?\\s*(\\d+(?:\\.\\d+)?)"), QRegularExpression::CaseInsensitiveOption);
    const auto volumeMatch = volumeMarker.match(base);
    if (volumeMatch.hasMatch()) {
        return volumeMatch.captured(1);
    }

    // Otherwise the last bare number in the name, which is how chapter files are written.
    static const QRegularExpression bareNumber(QStringLiteral("(?:^|[\\s_#-])(\\d+(?:\\.\\d+)?)(?=$|[\\s_)\\]-])"));
    QString last;
    auto it = bareNumber.globalMatch(base);
    while (it.hasNext()) {
        last = it.next().captured(1);
    }

    return last;
}

// "007" and "7" are the same volume, and a database full of the former sorts and reads
// badly. Leading zeros go; a fractional part is kept as written.
inline QString normalizedVolumeNumber(const QString &number)
{
    const auto trimmed = number.trimmed();
    if (trimmed.isEmpty()) {
        return trimmed;
    }

    auto integerPart = trimmed;
    QString fractionPart;
    const auto dot = trimmed.indexOf(QLatin1Char('.'));
    if (dot >= 0) {
        integerPart = trimmed.left(dot);
        fractionPart = trimmed.mid(dot);
    }

    while (integerPart.length() > 1 && integerPart.startsWith(QLatin1Char('0'))) {
        integerPart.remove(0, 1);
    }

    return integerPart + fractionPart;
}

}

#endif // VOLUME_NUMBER_UTILS_H
