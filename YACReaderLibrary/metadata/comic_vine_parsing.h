#ifndef COMIC_VINE_PARSING_H
#define COMIC_VINE_PARSING_H

#include "series_metadata.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QRegularExpression>
#include <QString>

// Reading Comic Vine's answer, kept apart from the client that fetches it.
//
// Header only and free of everything else on purpose: this is the part worth testing, and a
// test that has to link an HTTP worker and a settings file to check a JSON parser is a test
// that will be quietly dropped the first time it breaks the build.
namespace YACReader {

struct ComicVineResponse {
    QList<SeriesMetadata> candidates;
    bool error = false;
    bool rateLimited = false;
    int retryAfterSeconds = 0;
    QString errorString;
};

// Comic Vine writes its descriptions as HTML - paragraphs, links, the lot - and a synopsis
// field shows them as literal tags. Its own, rather than shared with the AniList client:
// what the two providers put in the text is different, and a common one would be a knot
// tying two unrelated things together.
inline QString stripComicVineHtml(const QString &text)
{
    if (text.isEmpty()) {
        return { };
    }

    auto plain = text;
    // Block ends become spaces so words either side of them do not run together.
    static const QRegularExpression blockEnd(QStringLiteral("</(p|div|li|h[1-6])>|<br\\s*/?>"), QRegularExpression::CaseInsensitiveOption);
    plain.replace(blockEnd, QStringLiteral(" "));

    static const QRegularExpression anyTag(QStringLiteral("<[^>]*>"));
    plain.remove(anyTag);

    plain.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    plain.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    plain.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    plain.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    plain.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    plain.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));

    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    return plain.replace(whitespace, QStringLiteral(" ")).trimmed();
}

inline ComicVineResponse parseComicVineSearch(const QByteArray &json)
{
    ComicVineResponse response;

    QJsonParseError parseError { };
    const auto document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        response.error = true;
        response.errorString = parseError.errorString();
        return response;
    }

    const auto root = document.object();

    // Comic Vine answers a refused request with HTTP 200 and the reason in the body, so the
    // only way to know is to read it. "OK" with status 1 is success; anything else is not.
    const auto status = root.value(QStringLiteral("status_code")).toInt();
    const auto errorText = root.value(QStringLiteral("error")).toString();
    if (status != 1) {
        response.error = true;
        response.errorString = errorText.isEmpty() ? QStringLiteral("Comic Vine returned status %1").arg(status) : errorText;
        // 107 is its rate limit. It does not say for how long.
        if (status == 107) {
            response.rateLimited = true;
            response.retryAfterSeconds = 60;
        }
        return response;
    }

    const auto results = root.value(QStringLiteral("results")).toArray();
    for (const auto &value : results) {
        const auto entry = value.toObject();

        SeriesMetadata series;
        series.providerName = QStringLiteral("Comic Vine");
        series.providerId = QString::number(entry.value(QStringLiteral("id")).toInt());
        series.title = entry.value(QStringLiteral("name")).toString().trimmed();
        series.synopsis = stripComicVineHtml(entry.value(QStringLiteral("deck")).toString());
        series.startYear = entry.value(QStringLiteral("start_year")).toString().toInt();
        series.volumes = entry.value(QStringLiteral("count_of_issues")).toInt();
        series.publisher = entry.value(QStringLiteral("publisher")).toObject().value(QStringLiteral("name")).toString().trimmed();

        // No genres, ever - Comic Vine has no such field. Leaving this empty is what makes
        // the sorter shelve the series under its publisher instead of guessing at a genre.
        if (series.isValid()) {
            response.candidates.append(series);
        }
    }

    return response;
}

inline QString parseComicVineDescription(const QByteArray &json)
{
    const auto document = QJsonDocument::fromJson(json);
    if (!document.isObject()) {
        return { };
    }

    const auto results = document.object().value(QStringLiteral("results")).toObject();
    return stripComicVineHtml(results.value(QStringLiteral("description")).toString());
}

}

#endif // COMIC_VINE_PARSING_H
