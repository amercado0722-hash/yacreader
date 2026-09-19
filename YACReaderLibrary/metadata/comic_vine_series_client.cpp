#include "comic_vine_series_client.h"

#include "http_worker.h"
#include "yacreader_global_gui.h"

#include <QSettings>
#include <QUrl>

using namespace YACReader;

namespace {

// The key YACReader ships with, used when the user has not entered their own. Same default
// the interactive Comic Vine scraper falls back to.
const QString kDefaultApiKey = QStringLiteral("46680bebb358f1de690a5a365e15d325f9649f91");

// A series title routinely contains characters that mean something in a URL query - &, +,
// #, spaces - and dropped in raw they silently truncate the search.
QString encoded(const QString &text)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(text));
}

}

ComicVineSeriesClient::ComicVineSeriesClient(QObject *parent)
    : QObject(parent)
{
    QSettings settings(YACReader::getSettingsPath() + "/YACReaderLibrary.ini", QSettings::IniFormat);
    settings.beginGroup("ComicVine");
    baseUrl = settings.value(COMIC_VINE_BASE_URL, QStringLiteral("https://comicvine.gamespot.com/api")).toString();
    apiKey = settings.value(COMIC_VINE_API_KEY, kDefaultApiKey).toString();
    userAgent = settings.value(COMIC_VINE_USER_AGENT, DEFAULT_USER_AGENT).toString();
}

QString ComicVineSeriesClient::requestUrl(const QString &path) const
{
    return baseUrl + path;
}

ComicVineSeriesClient::Response ComicVineSeriesClient::searchSeries(const QString &name, int limit)
{
    Response response;

    const auto trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return response;
    }

    const auto url = requestUrl(QStringLiteral("/search/?api_key=%1&format=json&resources=volume&limit=%2&field_list=name,start_year,publisher,id,count_of_issues,deck&query=%3")
                                        .arg(apiKey, QString::number(limit), encoded(trimmed)));

    auto *worker = new HttpWorker(url, userAgent);
    worker->get();
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

    if (status == 420 || status == 429) {
        response.error = true;
        response.rateLimited = true;
        response.retryAfterSeconds = retryAfter > 0 ? retryAfter : 60;
        response.errorString = QStringLiteral("Rate limited");
        return response;
    }

    if (failed && payload.isEmpty()) {
        response.error = true;
        response.errorString = errorText;
        return response;
    }

    static_cast<ComicVineResponse &>(response) = parseComicVineSearch(payload);
    if (response.rateLimited && response.retryAfterSeconds == 0) {
        response.retryAfterSeconds = retryAfter > 0 ? retryAfter : 60;
    }

    return response;
}

QString ComicVineSeriesClient::fetchDescription(const QString &volumeId)
{
    if (volumeId.isEmpty()) {
        return { };
    }

    // 4050 is Comic Vine's type prefix for a volume; the detail endpoint wants it.
    const auto url = requestUrl(QStringLiteral("/volume/4050-%1/?api_key=%2&format=json&field_list=description")
                                        .arg(volumeId, apiKey));

    auto *worker = new HttpWorker(url, userAgent);
    worker->get();
    worker->wait();

    const auto payload = worker->getResult();
    const auto ok = worker->wasValid();
    delete worker;

    if (!ok) {
        return { };
    }

    return parseComicVineDescription(payload);
}
