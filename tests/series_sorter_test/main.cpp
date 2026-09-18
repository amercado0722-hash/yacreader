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

QTEST_MAIN(SeriesSorterTest)

#include "main.moc"
