#include "metadata/series_match_scorer.h"

#include <QtTest>

using YACReader::SeriesMetadata;

namespace {

SeriesMetadata series(const QString &id, const QString &english, const QString &romaji = { }, const QStringList &synonyms = { })
{
    SeriesMetadata metadata;
    metadata.providerId = id;
    metadata.title = english;
    metadata.romajiTitle = romaji;
    metadata.synonyms = synonyms;
    return metadata;
}

}

// An unattended scrape writes to every volume of a series without anyone looking, so the
// failure that matters here is not "no match found" - that is visible and cheap to fix -
// but a confident wrong match, which looks exactly like a right one. These tests are
// mostly about the cases where the scorer must refuse to decide.
class SeriesMatchTest : public QObject
{
    Q_OBJECT

private slots:
    void normalizesAwayPunctuationAndCase();
    void normalizesAwayPunctuationAndCase_data();
    void matchesAcrossTitleLanguages();
    void refusesWhenTwoCandidatesTie();
    void refusesWhenTheNameOnlyPartlyOverlaps();
    void refusesASpinOffOfTheSameSeries();
    void acceptsASpacingDifference();
    void handlesNoCandidates();
};

void SeriesMatchTest::normalizesAwayPunctuationAndCase_data()
{
    QTest::addColumn<QString>("left");
    QTest::addColumn<QString>("right");
    QTest::addColumn<int>("expected");

    QTest::newRow("identical") << "A Bride's Story" << "A Bride's Story" << 100;
    QTest::newRow("case") << "BEASTARS" << "Beastars" << 100;
    // The apostrophe becomes a space, so these differ by spacing rather than by letters
    QTest::newRow("apostrophe") << "A Bride's Story" << "A Brides Story" << 95;
    QTest::newRow("ampersand") << "A Man & His Cat" << "A Man and His Cat" << 100;
    QTest::newRow("dash for space") << "Akane-banashi" << "Akane Banashi" << 100;
    QTest::newRow("colon") << "Re:Zero" << "ReZero" << 95;
    QTest::newRow("unrelated") << "Berserk" << "Bleach" << 0;
}

void SeriesMatchTest::normalizesAwayPunctuationAndCase()
{
    QFETCH(QString, left);
    QFETCH(QString, right);
    QFETCH(int, expected);

    QCOMPARE(YACReader::SeriesMatchScorer::scoreTitle(left, right), expected);
}

void SeriesMatchTest::matchesAcrossTitleLanguages()
{
    // The folder carries the English name; the provider leads with the romaji one. Either
    // has to be enough, or a manga library matches almost nothing.
    const QList<SeriesMetadata> candidates = {
        series("1", "A Bride's Story", "Otoyomegatari"),
    };

    const auto ranked = YACReader::rankSeriesMatches("A Bride's Story", candidates);

    QCOMPARE(ranked.size(), 1);
    QCOMPARE(ranked.first().score, 100);
    QVERIFY(ranked.first().confident);

    const auto byRomaji = YACReader::rankSeriesMatches("Otoyomegatari", candidates);
    QCOMPARE(byRomaji.first().score, 100);
    QVERIFY(byRomaji.first().confident);
}

void SeriesMatchTest::refusesWhenTwoCandidatesTie()
{
    // A light novel and its manga adaptation share a title exactly. The name alone cannot
    // separate them, so the scraper must hand this one back rather than pick.
    const QList<SeriesMetadata> candidates = {
        series("1", "The Ideal Sponger Life"),
        series("2", "The Ideal Sponger Life"),
    };

    const auto ranked = YACReader::rankSeriesMatches("The Ideal Sponger Life", candidates);

    QCOMPARE(ranked.first().score, 100);
    QVERIFY(!ranked.first().confident);
}

void SeriesMatchTest::refusesWhenTheNameOnlyPartlyOverlaps()
{
    const QList<SeriesMetadata> candidates = {
        series("1", "Insomniacs After School", "Kimi wa Houkago Insomnia"),
    };

    const auto ranked = YACReader::rankSeriesMatches("Insomniacs After", candidates);

    QVERIFY(ranked.first().score < 95);
    QVERIFY(!ranked.first().confident);
}

void SeriesMatchTest::refusesASpinOffOfTheSameSeries()
{
    // Spin-offs, side stories and sequels differ from their parent by a few words, which
    // is exactly the shape word overlap scores highly. It must not reach the confident band.
    const QList<SeriesMetadata> candidates = {
        series("1", "I've Been Killing Slimes for 300 Years and Maxed Out My Level"),
        series("2", "I've Been Killing Slimes for 300 Years and Maxed Out My Level Spin-off - The Red Dragon Academy for Girls"),
    };

    const auto ranked = YACReader::rankSeriesMatches("I've Been Killing Slimes for 300 Years and Maxed Out My Level", candidates);

    QCOMPARE(ranked.first().series.providerId, QStringLiteral("1"));
    QCOMPARE(ranked.first().score, 100);
    // The parent wins outright here, and by a wide enough margin to be taken
    QVERIFY(ranked.first().confident);

    const auto forSpinOff = YACReader::rankSeriesMatches("I've Been Killing Slimes for 300 Years and Maxed Out My Level Spin-off - The Red Dragon Academy for Girls", candidates);
    QCOMPARE(forSpinOff.first().series.providerId, QStringLiteral("2"));
    QVERIFY(forSpinOff.first().confident);
}

void SeriesMatchTest::acceptsASpacingDifference()
{
    const QList<SeriesMetadata> candidates = {
        series("1", "Re:Zero kara Hajimeru Isekai Seikatsu"),
        series("2", "Something Else Entirely"),
    };

    const auto ranked = YACReader::rankSeriesMatches("ReZero kara Hajimeru Isekai Seikatsu", candidates);

    QVERIFY(ranked.first().score >= 95);
    QVERIFY(ranked.first().confident);
}

void SeriesMatchTest::handlesNoCandidates()
{
    const auto ranked = YACReader::rankSeriesMatches("Anything", { });
    QVERIFY(ranked.isEmpty());
}

QTEST_MAIN(SeriesMatchTest)

#include "main.moc"
