#include "bookcase_magazines.h"

#include <QtTest>

using namespace YACReader;

// The story arc strings here are the real ones from a library of eighteen thousand works.
class BookcaseMagazinesTest : public QObject
{
    Q_OBJECT

private slots:
    void splitsADatedIssue();
    void splitsANumberedIssue();
    void splitsAVolume();
    void leavesAMagazineWithNoIssueAlone();
    void sortsDatesBeforeNumbers();
    void sortsNumbersNumericallyRatherThanAsText();
    void putsAnUnreadableIssueAtTheEnd();
    void picksTheCommonestSpellingOfAName();
    void doesNotConfuseTwoMagazinesWithSimilarNames();
    void repairsTwoNamesRunTogether();
    void repairsADoubledMagazineWord();
    void treatsAnOptionalLeadingComicAsTheSameMagazine();
};

void BookcaseMagazinesTest::splitsADatedIssue()
{
    QCOMPARE(magazineTitleFrom(QStringLiteral("COMIC Kairakuten 2016-08")), QStringLiteral("COMIC Kairakuten"));
    QCOMPARE(magazineIssueFrom(QStringLiteral("COMIC Kairakuten 2016-08")), QStringLiteral("2016-08"));
}

void BookcaseMagazinesTest::splitsANumberedIssue()
{
    QCOMPARE(magazineTitleFrom(QStringLiteral("COMIC X-Eros #50")), QStringLiteral("COMIC X-Eros"));
    QCOMPARE(magazineIssueFrom(QStringLiteral("COMIC X-Eros #50")), QStringLiteral("50"));
}

void BookcaseMagazinesTest::splitsAVolume()
{
    QCOMPARE(magazineTitleFrom(QStringLiteral("Isekairakuten Vol.6")), QStringLiteral("Isekairakuten"));
    QCOMPARE(magazineIssueFrom(QStringLiteral("Isekairakuten Vol.6")), QStringLiteral("6"));
}

void BookcaseMagazinesTest::leavesAMagazineWithNoIssueAlone()
{
    QCOMPARE(magazineTitleFrom(QStringLiteral("Girls forM")), QStringLiteral("Girls forM"));
    QVERIFY(magazineIssueFrom(QStringLiteral("Girls forM")).isEmpty());
}

void BookcaseMagazinesTest::sortsDatesBeforeNumbers()
{
    QVERIFY(magazineIssueSortKey(QStringLiteral("COMIC Kairakuten 2016-08")) < magazineIssueSortKey(QStringLiteral("COMIC X-Eros #50")));
}

// Issue 9 must come before issue 89, which plain text comparison gets wrong.
void BookcaseMagazinesTest::sortsNumbersNumericallyRatherThanAsText()
{
    QVERIFY(magazineIssueSortKey(QStringLiteral("COMIC X-Eros #9")) < magazineIssueSortKey(QStringLiteral("COMIC X-Eros #89")));
    QVERIFY(magazineIssueSortKey(QStringLiteral("COMIC Kairakuten 2016-08")) < magazineIssueSortKey(QStringLiteral("COMIC Kairakuten 2016-12")));
    QVERIFY(magazineIssueSortKey(QStringLiteral("COMIC Kairakuten 2019-01")) < magazineIssueSortKey(QStringLiteral("COMIC Kairakuten 2022-03")));
}

void BookcaseMagazinesTest::putsAnUnreadableIssueAtTheEnd()
{
    QVERIFY(magazineIssueSortKey(QStringLiteral("COMIC Aoha 2021 Winter")) > magazineIssueSortKey(QStringLiteral("COMIC Aoha 2021-01")));
}

void BookcaseMagazinesTest::picksTheCommonestSpellingOfAName()
{
    QHash<QString, int> counts;
    counts.insert(QStringLiteral("COMIC Kairakuten"), 2000);
    counts.insert(QStringLiteral("Comic Kairakuten"), 265);
    counts.insert(QStringLiteral("COMIC Bavel"), 1769);

    const auto canonical = canonicalMagazineTitles(counts);

    QCOMPARE(canonical.value(QStringLiteral("Comic Kairakuten")), QStringLiteral("COMIC Kairakuten"));
    QCOMPARE(canonical.value(QStringLiteral("COMIC Kairakuten")), QStringLiteral("COMIC Kairakuten"));
    QCOMPARE(canonical.value(QStringLiteral("COMIC Bavel")), QStringLiteral("COMIC Bavel"));
}

// "Kairakuten", "Kairakuten BEAST", "Weekly Kairakuten" and "Isekairakuten" are four
// different magazines, and the collection has all four.
void BookcaseMagazinesTest::doesNotConfuseTwoMagazinesWithSimilarNames()
{
    QCOMPARE(magazineTitleFrom(QStringLiteral("COMIC Kairakuten BEAST 2022-01")), QStringLiteral("COMIC Kairakuten BEAST"));
    QCOMPARE(magazineTitleFrom(QStringLiteral("Weekly Kairakuten 2019-03")), QStringLiteral("Weekly Kairakuten"));
    QCOMPARE(magazineTitleFrom(QStringLiteral("Isekairakuten Vol.6")), QStringLiteral("Isekairakuten"));
    QVERIFY(magazineTitleFrom(QStringLiteral("COMIC Kairakuten 2016-08")) != magazineTitleFrom(QStringLiteral("COMIC Kairakuten BEAST 2022-01")));
}

// Left alone, each of these is a section of the wall - a sign, a colour and a shelf - for
// the single book whose file name was mistyped.
void BookcaseMagazinesTest::repairsTwoNamesRunTogether()
{
    QCOMPARE(magazineTitleFrom(QStringLiteral("COMIC Bavel 2022-01, Bavel 2022-01")), QStringLiteral("COMIC Bavel"));
    QCOMPARE(magazineTitleFrom(QStringLiteral("Comic Bavel , Comic Bavel")), QStringLiteral("Comic Bavel"));
}

void BookcaseMagazinesTest::repairsADoubledMagazineWord()
{
    QCOMPARE(magazineTitleFrom(QStringLiteral("COMIC Comic Kairakuten 2016-08")), QStringLiteral("COMIC Kairakuten"));
    QCOMPARE(magazineTitleFrom(QStringLiteral("COMIC Comic X-Eros #50")), QStringLiteral("COMIC X-Eros"));
    // A magazine that legitimately begins with the word keeps it.
    QCOMPARE(magazineTitleFrom(QStringLiteral("COMIC Kairakuten 2016-08")), QStringLiteral("COMIC Kairakuten"));
}

void BookcaseMagazinesTest::treatsAnOptionalLeadingComicAsTheSameMagazine()
{
    QHash<QString, int> counts;
    counts.insert(QStringLiteral("Weekly Kairakuten"), 429);
    counts.insert(QStringLiteral("COMIC Weekly Kairakuten"), 25);
    counts.insert(QStringLiteral("COMIC Kairakuten"), 2267);
    counts.insert(QStringLiteral("COMIC Kairakuten BEAST"), 1294);

    const auto canonical = canonicalMagazineTitles(counts);

    QCOMPARE(canonical.value(QStringLiteral("COMIC Weekly Kairakuten")), QStringLiteral("Weekly Kairakuten"));
    QCOMPARE(canonical.value(QStringLiteral("Weekly Kairakuten")), QStringLiteral("Weekly Kairakuten"));
    // Dropping the leading word must not merge a magazine with its spin-off.
    QCOMPARE(canonical.value(QStringLiteral("COMIC Kairakuten BEAST")), QStringLiteral("COMIC Kairakuten BEAST"));
    QCOMPARE(canonical.value(QStringLiteral("COMIC Kairakuten")), QStringLiteral("COMIC Kairakuten"));
}

QTEST_MAIN(BookcaseMagazinesTest)

#include "main.moc"
