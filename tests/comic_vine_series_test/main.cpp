#include "metadata/comic_vine_parsing.h"

#include <QtTest>

using namespace YACReader;

// Comic Vine answers with HTTP 200 and its own status code in the body, so "did this work"
// is a question about the JSON rather than about the request. These are the shapes that
// actually come back.
class ComicVineSeriesTest : public QObject
{
    Q_OBJECT

private slots:
    void readsAVolumeSearch();
    void takesThePublisherName();
    void neverInventsAGenre();
    void readsAnErrorReturnedWithStatusTwoHundred();
    void readsARateLimit();
    void survivesRubbish();
    void stripsHtmlOutOfTheDescription();
};

void ComicVineSeriesTest::readsAVolumeSearch()
{
    const QByteArray json = R"({
        "error": "OK", "status_code": 1, "number_of_total_results": 2,
        "results": [
            { "id": 1234, "name": "Hellboy", "start_year": "1994", "count_of_issues": 14,
              "deck": "Mike Mignola's paranormal investigator.",
              "publisher": { "id": 364, "name": "Dark Horse Comics" } },
            { "id": 5678, "name": "B.P.R.D.", "start_year": "2003", "count_of_issues": 17,
              "deck": "", "publisher": { "id": 364, "name": "Dark Horse Comics" } }
        ]})";

    const auto response = parseComicVineSearch(json);

    QVERIFY(!response.error);
    QCOMPARE(response.candidates.size(), 2);
    QCOMPARE(response.candidates.first().title, QStringLiteral("Hellboy"));
    QCOMPARE(response.candidates.first().providerId, QStringLiteral("1234"));
    QCOMPARE(response.candidates.first().startYear, 1994);
    QCOMPARE(response.candidates.first().volumes, 14);
    QCOMPARE(response.candidates.first().providerName, QStringLiteral("Comic Vine"));
}

void ComicVineSeriesTest::takesThePublisherName()
{
    const QByteArray json = R"({"status_code": 1, "results": [
        { "id": 1, "name": "Grimm Fairy Tales", "publisher": { "name": "Zenescope Entertainment" } }]})";

    const auto response = parseComicVineSearch(json);

    QCOMPARE(response.candidates.size(), 1);
    QCOMPARE(response.candidates.first().publisher, QStringLiteral("Zenescope Entertainment"));
}

// The whole reason publisher is the shelf for these: there is no genre to shelve them by,
// and a parser that quietly produced one would put western comics on manga shelves.
void ComicVineSeriesTest::neverInventsAGenre()
{
    const QByteArray json = R"({"status_code": 1, "results": [
        { "id": 1, "name": "Hellboy", "deck": "Horror comic about a demon detective.",
          "publisher": { "name": "Dark Horse Comics" } }]})";

    const auto response = parseComicVineSearch(json);

    QCOMPARE(response.candidates.size(), 1);
    QVERIFY(response.candidates.first().genres.isEmpty());
    QVERIFY(response.candidates.first().tags.isEmpty());
}

void ComicVineSeriesTest::readsAnErrorReturnedWithStatusTwoHundred()
{
    const QByteArray json = R"({"error": "Invalid API Key", "status_code": 100, "results": []})";

    const auto response = parseComicVineSearch(json);

    QVERIFY(response.error);
    QCOMPARE(response.errorString, QStringLiteral("Invalid API Key"));
    QVERIFY(!response.rateLimited);
}

void ComicVineSeriesTest::readsARateLimit()
{
    const QByteArray json = R"({"error": "Rate Limit Exceeded", "status_code": 107, "results": []})";

    const auto response = parseComicVineSearch(json);

    QVERIFY(response.error);
    QVERIFY(response.rateLimited);
    QVERIFY(response.retryAfterSeconds > 0);
}

void ComicVineSeriesTest::survivesRubbish()
{
    QVERIFY(parseComicVineSearch("not json at all").error);
    QVERIFY(parseComicVineSearch(QByteArray()).error);

    // Valid JSON, nothing usable in it: not an error, just no candidates.
    const auto empty = parseComicVineSearch(R"({"status_code": 1, "results": []})");
    QVERIFY(!empty.error);
    QVERIFY(empty.candidates.isEmpty());

    // A result with no name cannot be matched against anything and is dropped.
    const auto nameless = parseComicVineSearch(R"({"status_code": 1, "results": [{ "id": 9 }]})");
    QVERIFY(nameless.candidates.isEmpty());
}

void ComicVineSeriesTest::stripsHtmlOutOfTheDescription()
{
    const QByteArray json = R"({"status_code": 1, "results":
        { "description": "<p>Hellboy was summoned to Earth as an <i>infant</i>.</p>" }})";

    const auto description = parseComicVineDescription(json);

    QVERIFY(!description.contains(QStringLiteral("<p>")));
    QVERIFY(!description.contains(QStringLiteral("<i>")));
    QVERIFY(description.contains(QStringLiteral("summoned to Earth")));
}

QTEST_MAIN(ComicVineSeriesTest)

#include "main.moc"
