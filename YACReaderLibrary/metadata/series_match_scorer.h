#ifndef SERIES_MATCH_SCORER_H
#define SERIES_MATCH_SCORER_H

#include "series_metadata.h"

#include <QList>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <algorithm>

// Deciding whether a search result is *the* series, without a human looking at it.
//
// This is the part of an unattended scrape that can quietly do damage: a wrong match does
// not fail, it writes a confident, wrong synopsis and author onto every volume of a series
// and looks exactly like a right one. So the rule here is deliberately timid - a match is
// only taken automatically when the names agree almost exactly AND no other candidate
// comes close. Everything else is handed back for the user to settle in one pass, which is
// cheap; undoing 1,900 wrong ones is not.
namespace YACReader {

namespace SeriesMatchScorer {

// Two titles for the same series routinely differ by punctuation, case, and the way an
// ampersand or a dash was typed. None of that is a real difference, so it is flattened
// away before anything is compared.
inline QString normalizeTitle(const QString &title)
{
    auto normalized = title.toLower();

    normalized.replace(QStringLiteral("&"), QStringLiteral(" and "));
    normalized.replace(QStringLiteral("＆"), QStringLiteral(" and "));

    // Anything that is not a letter, a digit or a space is punctuation as far as matching
    // is concerned. Kept as a character class rather than a list so this works for the
    // Japanese titles too.
    static const QRegularExpression punctuation(QStringLiteral("[^\\p{L}\\p{N}\\s]"));
    normalized.replace(punctuation, QStringLiteral(" "));

    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    normalized.replace(whitespace, QStringLiteral(" "));

    return normalized.trimmed();
}

// "Akane-banashi" and "Akane Banashi" are the same series; so are "Re:Zero" and "ReZero".
// Comparing with the spaces gone catches the ones normalizeTitle alone does not.
inline QString compactTitle(const QString &title)
{
    auto compact = normalizeTitle(title);
    compact.remove(QLatin1Char(' '));
    return compact;
}

inline QSet<QString> titleTokens(const QString &title)
{
    const auto normalized = normalizeTitle(title);
    if (normalized.isEmpty()) {
        return { };
    }

    const auto parts = normalized.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    return QSet<QString>(parts.begin(), parts.end());
}

// 0 to 100. 100 and 95 mean the names are the same modulo punctuation or spacing; below
// that is word overlap, which is a hint for a human rather than grounds for a decision.
inline int scoreTitle(const QString &wanted, const QString &candidate)
{
    const auto wantedNormalized = normalizeTitle(wanted);
    const auto candidateNormalized = normalizeTitle(candidate);

    if (wantedNormalized.isEmpty() || candidateNormalized.isEmpty()) {
        return 0;
    }

    if (wantedNormalized == candidateNormalized) {
        return 100;
    }

    if (compactTitle(wanted) == compactTitle(candidate)) {
        return 95;
    }

    const auto wantedWords = titleTokens(wanted);
    const auto candidateWords = titleTokens(candidate);
    if (wantedWords.isEmpty() || candidateWords.isEmpty()) {
        return 0;
    }

    const auto shared = static_cast<int>((wantedWords & candidateWords).size());
    const auto total = static_cast<int>((wantedWords | candidateWords).size());
    if (total == 0) {
        return 0;
    }

    // Capped below the exact-match band on purpose: no amount of word overlap should ever
    // be mistaken for the names actually agreeing.
    return (shared * 80) / total;
}

// The best score any of a series' known titles achieves against the name we are looking up.
inline int scoreSeries(const QString &wanted, const SeriesMetadata &series)
{
    auto best = 0;
    const auto titles = series.allTitles();
    for (const auto &title : titles) {
        best = qMax(best, scoreTitle(wanted, title));
    }
    return best;
}

}

struct SeriesMatch {
    SeriesMetadata series;
    int score = 0;
    bool confident = false;
};

// A match is taken automatically only when the winner is an all-but-exact name match and
// nothing else is within reach of it. The gap matters as much as the score: a library
// holds spin-offs, side stories and sequels whose names differ by a word, and two
// candidates both scoring 100 means the name alone cannot tell them apart.
inline QList<SeriesMatch> rankSeriesMatches(const QString &wanted, const QList<SeriesMetadata> &candidates)
{
    static constexpr auto kConfidentScore = 95;
    static constexpr auto kRequiredLead = 15;

    QList<SeriesMatch> ranked;
    ranked.reserve(candidates.size());

    for (const auto &candidate : candidates) {
        SeriesMatch match;
        match.series = candidate;
        match.score = SeriesMatchScorer::scoreSeries(wanted, candidate);
        ranked.append(match);
    }

    std::stable_sort(ranked.begin(), ranked.end(), [](const SeriesMatch &a, const SeriesMatch &b) {
        return a.score > b.score;
    });

    if (!ranked.isEmpty() && ranked.first().score >= kConfidentScore) {
        const auto runnerUp = ranked.size() > 1 ? ranked.at(1).score : 0;
        ranked.first().confident = (ranked.first().score - runnerUp) >= kRequiredLead;
    }

    return ranked;
}

}

#endif // SERIES_MATCH_SCORER_H
