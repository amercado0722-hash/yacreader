#include "series_sorter.h"

#include <QtTest>

using namespace YACReader;

// The sorter moves folders on disk without anybody watching, which makes the question of
// what it will NOT touch the important one. These are the cases where a wrong answer moves
// somebody's files somewhere they did not put them.
class SeriesSorterTest : public QObject
{
    Q_OBJECT

private slots:
    void movesASeriesLooseAtTheTop();
    void movesASeriesWaitingToBeIdentified();
    void leavesASeriesAlreadyInASection();
    void leavesAFolderThatIsNotAGenreAlone();
    void leavesADeeplyNestedVolumeFolderAlone();
    void copesWithALeadingSlashOrAnEmptyPath();
    void needsMostVolumesToAgreeOnAGenre();
    void needsMostVolumesToAgreeOnAPublisher();
    void takesTheCommonestPublisherWhenSeveralAreNamed();
};

void SeriesSorterTest::movesASeriesLooseAtTheTop()
{
    QVERIFY(isLooseSeriesPath(QStringLiteral("Some New Series")));
}

void SeriesSorterTest::movesASeriesWaitingToBeIdentified()
{
    QVERIFY(isLooseSeriesPath(QStringLiteral("Not yet identified/Some New Series")));
}

void SeriesSorterTest::leavesASeriesAlreadyInASection()
{
    QVERIFY(!isLooseSeriesPath(QStringLiteral("Action/One Piece")));
    QVERIFY(!isLooseSeriesPath(QStringLiteral("Romance/Nana")));
}

// The one that matters most. "Light Novels" is a top level folder the reader made on
// purpose, and a rule of "anything not already in a genre" would empty it into the genres.
void SeriesSorterTest::leavesAFolderThatIsNotAGenreAlone()
{
    QVERIFY(!isLooseSeriesPath(QStringLiteral("Light Novels/Goblin Slayer [Yen Press]")));
    QVERIFY(!isLooseSeriesPath(QStringLiteral("_Needs a look/Something Odd")));
    QVERIFY(!isLooseSeriesPath(QStringLiteral("_Duplicates/Whatever")));
}

void SeriesSorterTest::leavesADeeplyNestedVolumeFolderAlone()
{
    QVERIFY(!isLooseSeriesPath(QStringLiteral("Action/Berserk/Chapters")));
    QVERIFY(!isLooseSeriesPath(QStringLiteral("Not yet identified/Series/Replaced")));
}

void SeriesSorterTest::copesWithALeadingSlashOrAnEmptyPath()
{
    // The database stores paths with a leading slash; splitting skips the empty part, so a
    // caller that forgets to strip it gets the same answer rather than a wrong one.
    QVERIFY(isLooseSeriesPath(QStringLiteral("/Some New Series")));
    QVERIFY(!isLooseSeriesPath(QString()));
    QVERIFY(!isLooseSeriesPath(QStringLiteral("/")));
}

// One volume of fourteen carrying "Slice of Life" from an embedded ComicInfo.xml is not
// fourteen volumes of Hellboy being a slice of life story. Measured against the real
// library, sampling any volume's tags shelved it there.
void SeriesSorterTest::needsMostVolumesToAgreeOnAGenre()
{
    SeriesTally hellboy;
    hellboy.volumes = 14;
    hellboy.genreCounts.insert(QStringLiteral("Slice of Life"), 1);
    QVERIFY(agreedGenres(hellboy).isEmpty());

    SeriesTally scraped;
    scraped.volumes = 14;
    scraped.genreCounts.insert(QStringLiteral("Horror"), 14);
    scraped.genreCounts.insert(QStringLiteral("Slice of Life"), 1);
    QCOMPARE(agreedGenres(scraped), QStringList { QStringLiteral("Horror") });

    // Exactly half is agreement: a series split down the middle between two genres should
    // still be shelved under the more specific of them rather than left out.
    SeriesTally halved;
    halved.volumes = 10;
    halved.genreCounts.insert(QStringLiteral("Action"), 5);
    QCOMPARE(agreedGenres(halved), QStringList { QStringLiteral("Action") });
}

void SeriesSorterTest::needsMostVolumesToAgreeOnAPublisher()
{
    SeriesTally specials;
    specials.volumes = 71;
    specials.publisherCounts.insert(QStringLiteral("Zenescope Entertainment"), 1);
    QVERIFY(agreedPublisher(specials).isEmpty());

    SeriesTally scraped;
    scraped.volumes = 17;
    scraped.publisherCounts.insert(QStringLiteral("Magic Press"), 17);
    QCOMPARE(agreedPublisher(scraped), QStringLiteral("Magic Press"));

    SeriesTally nothing;
    nothing.volumes = 9;
    QVERIFY(agreedPublisher(nothing).isEmpty());
}

void SeriesSorterTest::takesTheCommonestPublisherWhenSeveralAreNamed()
{
    SeriesTally mixed;
    mixed.volumes = 20;
    mixed.publisherCounts.insert(QStringLiteral("Dark Horse Comics"), 15);
    mixed.publisherCounts.insert(QStringLiteral("Magic Press"), 3);
    QCOMPARE(agreedPublisher(mixed), QStringLiteral("Dark Horse Comics"));

    // Neither reaches half, so neither speaks for the series.
    SeriesTally split;
    split.volumes = 20;
    split.publisherCounts.insert(QStringLiteral("Dark Horse Comics"), 6);
    split.publisherCounts.insert(QStringLiteral("Magic Press"), 5);
    QVERIFY(agreedPublisher(split).isEmpty());
}

QTEST_MAIN(SeriesSorterTest)

#include "main.moc"
