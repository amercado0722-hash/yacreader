#include "anilist_client.h"

#include "http_worker.h"
#include "yacreader_global.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSettings>

using namespace YACReader;

namespace {

// One request per series. Asking for a handful of hits rather than one means the matcher
// can see whether anything else came close, which is what a confident match depends on.
const QString kQuery = QStringLiteral(
        "query ($search: String, $perPage: Int) {"
        "  Page(page: 1, perPage: $perPage) {"
        "    media(search: $search, type: MANGA, sort: SEARCH_MATCH) {"
        "      id"
        "      title { romaji english native }"
        "      synonyms"
        "      format"
        "      status"
        "      description(asHtml: false)"
        "      startDate { year month day }"
        "      volumes"
        "      chapters"
        "      genres"
        "      countryOfOrigin"
        "      isAdult"
        "      siteUrl"
        "      coverImage { large }"
        "      tags { name rank isGeneralSpoiler }"
        "      staff(perPage: 8, sort: RELEVANCE) { edges { role node { name { full } } } }"
        "    }"
        "  }"
        "}");

QString jsonString(const QJsonObject &object, const QString &key)
{
    const auto value = object.value(key);
    return value.isString() ? value.toString() : QString();
}

int jsonInt(const QJsonObject &object, const QString &key)
{
    const auto value = object.value(key);
    return value.isDouble() ? value.toInt() : 0;
}

QStringList jsonStringList(const QJsonObject &object, const QString &key)
{
    QStringList values;
    const auto array = object.value(key).toArray();
    for (const auto &entry : array) {
        const auto text = entry.toString().trimmed();
        if (!text.isEmpty()) {
            values.append(text);
        }
    }
    return values;
}

// AniList reports credits as free text - "Story & Art", "Story", "Art", "Original Story" -
// and a series often lists several people. The first name in each role is the one worth
// recording; the rest are assistants and editors that would only crowd the field.
void applyStaff(const QJsonObject &media, SeriesMetadata &metadata)
{
    const auto edges = media.value(QStringLiteral("staff")).toObject().value(QStringLiteral("edges")).toArray();
    for (const auto &edgeValue : edges) {
        const auto edge = edgeValue.toObject();
        const auto role = jsonString(edge, QStringLiteral("role"));
        const auto name = edge.value(QStringLiteral("node")).toObject().value(QStringLiteral("name")).toObject().value(QStringLiteral("full")).toString().trimmed();
        if (name.isEmpty() || role.isEmpty()) {
            continue;
        }

        const auto mentionsStory = role.contains(QStringLiteral("Story"), Qt::CaseInsensitive);
        const auto mentionsArt = role.contains(QStringLiteral("Art"), Qt::CaseInsensitive);

        if (mentionsStory && metadata.writer.isEmpty()) {
            metadata.writer = name;
        }
        if (mentionsArt && metadata.penciller.isEmpty()) {
            metadata.penciller = name;
        }
    }
}

// AniList's own tag list is long and includes a tail of loosely applicable ones. The rank
// is a percentage of users who agreed the tag fits, so a floor keeps the useful ones. A
// tag flagged as a general spoiler has no business being written into a library.
QStringList relevantTags(const QJsonObject &media)
{
    static constexpr auto kMinimumRank = 60;

    QStringList tags;
    const auto array = media.value(QStringLiteral("tags")).toArray();
    for (const auto &entry : array) {
        const auto tag = entry.toObject();
        if (tag.value(QStringLiteral("isGeneralSpoiler")).toBool()) {
            continue;
        }
        if (jsonInt(tag, QStringLiteral("rank")) < kMinimumRank) {
            continue;
        }
        const auto name = jsonString(tag, QStringLiteral("name")).trimmed();
        if (!name.isEmpty() && !tags.contains(name)) {
            tags.append(name);
        }
    }
    return tags;
}

SeriesMetadata parseMedia(const QJsonObject &media)
{
    SeriesMetadata metadata;

    const auto id = jsonInt(media, QStringLiteral("id"));
    if (id == 0) {
        return metadata;
    }
    metadata.providerId = QString::number(id);
    metadata.providerName = QStringLiteral("AniList");

    const auto title = media.value(QStringLiteral("title")).toObject();
    metadata.romajiTitle = jsonString(title, QStringLiteral("romaji"));
    metadata.nativeTitle = jsonString(title, QStringLiteral("native"));

    // English where there is one, because that is what the folders are named; romaji is
    // the fallback rather than the default, for the same reason.
    const auto english = jsonString(title, QStringLiteral("english"));
    metadata.title = english.isEmpty() ? metadata.romajiTitle : english;

    metadata.synonyms = jsonStringList(media, QStringLiteral("synonyms"));
    metadata.synopsis = AniListClient::stripHtml(jsonString(media, QStringLiteral("description")));
    metadata.genres = jsonStringList(media, QStringLiteral("genres"));
    metadata.tags = relevantTags(media);
    metadata.volumes = jsonInt(media, QStringLiteral("volumes"));
    metadata.chapters = jsonInt(media, QStringLiteral("chapters"));

    const auto startDate = media.value(QStringLiteral("startDate")).toObject();
    metadata.startYear = jsonInt(startDate, QStringLiteral("year"));
    metadata.startMonth = jsonInt(startDate, QStringLiteral("month"));
    metadata.startDay = jsonInt(startDate, QStringLiteral("day"));

    metadata.status = jsonString(media, QStringLiteral("status"));
    metadata.countryOfOrigin = jsonString(media, QStringLiteral("countryOfOrigin"));
    metadata.isAdult = media.value(QStringLiteral("isAdult")).toBool();
    metadata.siteUrl = jsonString(media, QStringLiteral("siteUrl"));
    metadata.coverUrl = media.value(QStringLiteral("coverImage")).toObject().value(QStringLiteral("large")).toString();

    applyStaff(media, metadata);

    return metadata;
}

}

AniListClient::AniListClient(QObject *parent)
    : QObject(parent)
{
    QSettings settings(YACReader::getSettingsPath() + "/YACReaderLibrary.ini", QSettings::IniFormat);
    settings.beginGroup("AniList");
    endpoint = settings.value("endpoint", QStringLiteral("https://graphql.anilist.co")).toString();
    userAgent = settings.value("userAgent", QStringLiteral("YACReaderLibrary")).toString();
}

QByteArray AniListClient::buildRequestBody(const QString &name, int limit)
{
    QJsonObject variables;
    variables.insert(QStringLiteral("search"), name);
    variables.insert(QStringLiteral("perPage"), qBound(1, limit, 25));

    QJsonObject body;
    body.insert(QStringLiteral("query"), kQuery);
    body.insert(QStringLiteral("variables"), variables);

    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

QString AniListClient::stripHtml(const QString &text)
{
    if (text.isEmpty()) {
        return text;
    }

    auto stripped = text;

    // A synopsis comes back with markup in it even when asHtml is false, mostly line
    // breaks and the occasional emphasis. Turned into plain text here so that whatever
    // displays it later does not have to.
    static const QRegularExpression lineBreak(QStringLiteral("<br\\s*/?>"), QRegularExpression::CaseInsensitiveOption);
    stripped.replace(lineBreak, QStringLiteral("\n"));

    static const QRegularExpression anyTag(QStringLiteral("<[^>]*>"));
    stripped.remove(anyTag);

    static const QRegularExpression manyBlankLines(QStringLiteral("\n{3,}"));
    stripped.replace(manyBlankLines, QStringLiteral("\n\n"));

    stripped.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    stripped.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    stripped.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    stripped.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    stripped.replace(QStringLiteral("&#039;"), QStringLiteral("'"));

    return stripped.trimmed();
}

AniListClient::Response AniListClient::parseResponse(const QByteArray &json)
{
    Response response;

    QJsonParseError parseError { };
    const auto document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        response.error = true;
        response.errorString = parseError.errorString();
        return response;
    }

    const auto root = document.object();

    // GraphQL answers 200 with an errors array rather than an HTTP status, so a failure
    // here looks like a success unless it is looked for.
    const auto errors = root.value(QStringLiteral("errors")).toArray();
    if (!errors.isEmpty()) {
        const auto first = errors.first().toObject();
        response.error = true;
        response.errorString = jsonString(first, QStringLiteral("message"));
        if (response.errorString.contains(QStringLiteral("Too Many Requests"), Qt::CaseInsensitive)) {
            response.rateLimited = true;
        }
        // A "not found" is an answer, not a failure: the series simply is not there.
        if (jsonInt(first, QStringLiteral("status")) == 404) {
            response.error = false;
        }
        if (response.error) {
            return response;
        }
    }

    const auto media = root.value(QStringLiteral("data")).toObject().value(QStringLiteral("Page")).toObject().value(QStringLiteral("media")).toArray();
    for (const auto &entry : media) {
        const auto metadata = parseMedia(entry.toObject());
        if (metadata.isValid()) {
            response.candidates.append(metadata);
        }
    }

    return response;
}

AniListClient::Response AniListClient::searchSeries(const QString &name, int limit)
{
    Response response;

    const auto trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return response;
    }

    auto *worker = new HttpWorker(endpoint, userAgent);
    worker->post(buildRequestBody(trimmed, limit));
    worker->wait();

    const auto payload = worker->getResult();
    const auto failed = !worker->wasValid();
    const auto timedOut = worker->wasTimeout();
    const auto status = worker->statusCode();
    const auto errorText = worker->errorString();
    const auto retryAfter = worker->retryAfterSeconds();
    delete worker;

    if (timedOut) {
        response.error = true;
        response.timedOut = true;
        response.errorString = QStringLiteral("Timeout");
        return response;
    }

    if (status == 429) {
        response.error = true;
        response.rateLimited = true;
        // A minute is the window AniList counts in, so it is the right thing to wait when
        // the server did not name a number itself.
        response.retryAfterSeconds = retryAfter > 0 ? retryAfter : 60;
        response.errorString = QStringLiteral("Rate limited");
        return response;
    }

    if (failed && payload.isEmpty()) {
        response.error = true;
        response.errorString = errorText;
        return response;
    }

    response = parseResponse(payload);
    if (response.rateLimited && response.retryAfterSeconds == 0) {
        response.retryAfterSeconds = retryAfter > 0 ? retryAfter : 60;
    }

    return response;
}
