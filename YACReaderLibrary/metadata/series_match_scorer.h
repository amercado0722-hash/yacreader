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
//
// A match on one of the names the series is published under counts for more than a match on
// one of its synonyms. That is not a nicety: a spin-off's synonym list routinely contains
// its parent series' title, so scoring the two the same made the real "My Hero Academia"
// tie at 100 with a My Hero Academia side story - and to the rule below, a tie looks exactly
// like genuine ambiguity. Synonyms still count, because plenty of series are only findable
// by one, but they sit in a band of their own beneath the real names.
inline int scoreSeries(const QString &wanted, const SeriesMetadata &series)
{
    static constexpr auto kSynonymExactScore = 92;

    auto best = 0;
    const auto primary = series.primaryTitles();
    for (const auto &title : primary) {
        best = qMax(best, scoreTitle(wanted, title));
    }

    for (const auto &synonym : series.synonyms) {
        const auto score = scoreTitle(wanted, synonym);
        // An exact synonym match is strong evidence, just not as strong as the real name.
        // A partial one is word overlap and keeps the score it earned, which is well below
        // anything that can be taken automatically - but the ranking is also what orders the
        // list the user picks from when the scraper gives up, and a half-recognised name is
        // worth more to them there than nothing at all.
        best = qMax(best, score >= 95 ? kSynonymExactScore : score);
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
// nothing else is within reach of it. The gap matters as much as the score: a library holds
// spin-offs, side stories and sequels whose names differ by a word, and two candidates both
// scoring 100 means the name alone cannot tell them apart.
//
// But "the name alone cannot tell them apart" is not the same as "nobody can". Measured
// against the real library, requiring a clear lead rejected almost every famous series in
// it - One Piece, Naruto, Bleach, My Hero Academia - because a famous series is precisely
// the one with duplicate entries, parodies and side stories sharing its name. When two
// candidates are named identically they are nearly always the same work listed twice, or a
// well known work and something trading on its title, and readership separates those two
// cases decisively: One Piece has two hundred thousand readers and the parody has two
// hundred. So a tie on the name is settled by a wide margin in readership, and only a
// genuine contest - two real series of the same name and comparable following - is still
// handed to the user.
inline QList<SeriesMatch> rankSeriesMatches(const QString &wanted, const QList<SeriesMetadata> &candidates)
{
    // The synonym band sits just under this, so an exact synonym match with nothing else
    // near it can still be taken.
    static constexpr auto kConfidentScore = 92;
    static constexpr auto kRequiredLead = 15;
    // Deliberately a multiple rather than a difference: what matters is that one candidate
    // is in a different league, not that it is a fixed number of readers ahead.
    static constexpr auto kRequiredPopularityRatio = 3;
    // Below this, a provider's readership figures are too thin to mean anything.
    static constexpr auto kMinimumPopularity = 300;

    QList<SeriesMatch> ranked;
    ranked.reserve(candidates.size());

    for (const auto &candidate : candidates) {
        SeriesMatch match;
        match.series = candidate;
        match.score = SeriesMatchScorer::scoreSeries(wanted, candidate);
        ranked.append(match);
    }

    // Sorted by score, then by readership, so that when the scores tie the candidate the
    // tiebreak would choose is the one sitting at the front.
    std::stable_sort(ranked.begin(), ranked.end(), [](const SeriesMatch &a, const SeriesMatch &b) {
        if (a.score != b.score) {
            return a.score > b.score;
        }
        return a.series.popularity > b.series.popularity;
    });

    if (!ranked.isEmpty() && ranked.first().score >= kConfidentScore) {
        const auto runnerUpScore = ranked.size() > 1 ? ranked.at(1).score : 0;
        const auto runnerUpPopularity = ranked.size() > 1 ? ranked.at(1).series.popularity : 0;
        const auto winnerPopularity = ranked.first().series.popularity;

        const auto clearOnName = (ranked.first().score - runnerUpScore) >= kRequiredLead;
        // One line because the project's clang-format has no column limit and joins a
        // wrapped condition back up regardless of where it was broken.
        const auto clearOnReaders = winnerPopularity >= kMinimumPopularity && winnerPopularity >= kRequiredPopularityRatio * qMax(runnerUpPopularity, 1);

        ranked.first().confident = clearOnName || clearOnReaders;
    }

    return ranked;
}

}

#endif // SERIES_MATCH_SCORER_H
