#include "metadata/filename_metadata.h"

#include <QtTest>

using YACReader::FilenameMetadata;
using YACReader::parseComicFileName;

// These names are taken from a real collection of eighteen thousand files, because the
// convention they follow is loose enough that made up examples would all parse and prove
// nothing. The cases that matter are the ones where a bracket or a parenthesis is NOT what
// it looks like: a tag that would become an artist, a parody that would become a magazine,
// a title with a legitimate parenthesis in it.
class FilenameMetadataTest : public QObject
{
    Q_OBJECT

private slots:
    void readsTheOrdinaryShape();
    void readsAnIssueNumberedMagazine();
    void doesNotTakeATrailingTagAsTheArtist();
    void keepsAnUnrecognisedParenthesisInTheTitle();
    void dropsTheScanResolution();
    void findsTheArtistBehindAnEventMarker();
    void readsPublisherLanguageAndFlags();
    void keepsNestedParenthesesInsideTheArtist();
    void survivesANameWithNothingInIt();
    void survivesANameWithNoTagsAtAll();
    void doesNotInventAMagazineFromAParody();
    void takesOnlyTheFirstMagazine();
};

void FilenameMetadataTest::readsTheOrdinaryShape()
{
    const auto metadata = parseComicFileName(QStringLiteral("[Ohkami Ryosuke] Sweet Dreams, Onee-chan (COMIC Shitsurakuten 2017-02).cbz"));

    QCOMPARE(metadata.artist, QStringLiteral("Ohkami Ryosuke"));
    QCOMPARE(metadata.title, QStringLiteral("Sweet Dreams, Onee-chan"));
    QCOMPARE(metadata.magazine, QStringLiteral("COMIC Shitsurakuten 2017-02"));
    QCOMPARE(metadata.issue, QStringLiteral("2017-02"));
    QCOMPARE(metadata.date, QStringLiteral("01/02/2017"));
}

void FilenameMetadataTest::readsAnIssueNumberedMagazine()
{
    const auto metadata = parseComicFileName(QStringLiteral("[Aiue Oka] Bitch Fuck (COMIC X-Eros #50) [FAKKU].cbz"));

    QCOMPARE(metadata.artist, QStringLiteral("Aiue Oka"));
    QCOMPARE(metadata.title, QStringLiteral("Bitch Fuck"));
    QCOMPARE(metadata.magazine, QStringLiteral("COMIC X-Eros #50"));
    QCOMPARE(metadata.issue, QStringLiteral("50"));
    // An issue number is not a date, and guessing one from it would be worse than none.
    QVERIFY(metadata.date.isEmpty());
    QCOMPARE(metadata.publisher, QStringLiteral("FAKKU"));
}

// The failure this guards against: taking the LAST bracket, or any bracket, as the artist.
// Nearly every file here ends in a bracket that is not one.
void FilenameMetadataTest::doesNotTakeATrailingTagAsTheArtist()
{
    const auto metadata = parseComicFileName(QStringLiteral("[Kurono] Uhanarizuke (Survival Onigokko) [Chinese] [Some Scanlation Group].cbz"));

    QCOMPARE(metadata.artist, QStringLiteral("Kurono"));
    QCOMPARE(metadata.language, QStringLiteral("zh"));
    QVERIFY(!metadata.title.contains(QStringLiteral("Chinese")));
    QVERIFY(!metadata.title.contains(QStringLiteral("Scanlation")));
}

void FilenameMetadataTest::keepsAnUnrecognisedParenthesisInTheTitle()
{
    const auto metadata = parseComicFileName(QStringLiteral("[Setouchi Seiyaku (Setouchi)] Monkue Nabe (Monster Girl Quest) [English] [Digital].cbz"));

    QCOMPARE(metadata.artist, QStringLiteral("Setouchi Seiyaku (Setouchi)"));
    // The parody is part of what this comic is. Nothing here knows enough to file it, so it
    // stays in the title rather than being thrown away.
    QCOMPARE(metadata.title, QStringLiteral("Monkue Nabe (Monster Girl Quest)"));
    QCOMPARE(metadata.language, QStringLiteral("en"));
    QVERIFY(metadata.flags.contains(QStringLiteral("Digital")));
}

void FilenameMetadataTest::dropsTheScanResolution()
{
    const auto metadata = parseComicFileName(QStringLiteral("[Nanigawa Rui] Don't Avert Your Eyes. (Comic Bavel 2022-02) (x3200) [FAKKU & 2D Market].cbz"));

    QCOMPARE(metadata.title, QStringLiteral("Don't Avert Your Eyes."));
    QCOMPARE(metadata.magazine, QStringLiteral("Comic Bavel 2022-02"));
    QCOMPARE(metadata.publisher, QStringLiteral("FAKKU & 2D Market"));
    QVERIFY(!metadata.title.contains(QStringLiteral("x3200")));
}

void FilenameMetadataTest::findsTheArtistBehindAnEventMarker()
{
    const auto metadata = parseComicFileName(QStringLiteral("(C85) [Hyakki Yakou (Various)] Hyakki Yakou Lv.3 [English] [fmko].cbz"));

    QCOMPARE(metadata.artist, QStringLiteral("Hyakki Yakou (Various)"));
    QCOMPARE(metadata.title, QStringLiteral("Hyakki Yakou Lv.3"));
    QCOMPARE(metadata.language, QStringLiteral("en"));
}

void FilenameMetadataTest::readsPublisherLanguageAndFlags()
{
    const auto metadata = parseComicFileName(QStringLiteral("[Horitomo] Shippo Stories Ch. 1-5 [English] [Decensored] [Irodori Comics].cbz"));

    QCOMPARE(metadata.artist, QStringLiteral("Horitomo"));
    QCOMPARE(metadata.title, QStringLiteral("Shippo Stories Ch. 1-5"));
    QCOMPARE(metadata.publisher, QStringLiteral("Irodori Comics"));
    QCOMPARE(metadata.language, QStringLiteral("en"));
    QCOMPARE(metadata.flags, QStringList { QStringLiteral("Decensored") });
}

void FilenameMetadataTest::keepsNestedParenthesesInsideTheArtist()
{
    const auto metadata = parseComicFileName(QStringLiteral("[Astronauts Alya (Marushin)] Grimoire no Shisho (Color).cbz"));

    QCOMPARE(metadata.artist, QStringLiteral("Astronauts Alya (Marushin)"));
    QCOMPARE(metadata.title, QStringLiteral("Grimoire no Shisho"));
    QVERIFY(metadata.flags.contains(QStringLiteral("Color")));
}

void FilenameMetadataTest::survivesANameWithNothingInIt()
{
    const auto metadata = parseComicFileName(QStringLiteral(".cbz"));
    QVERIFY(metadata.isEmpty());

    const auto blank = parseComicFileName(QString());
    QVERIFY(blank.isEmpty());
}

void FilenameMetadataTest::survivesANameWithNoTagsAtAll()
{
    const auto metadata = parseComicFileName(QStringLiteral("Centaur Girls.cbz"));

    QVERIFY(metadata.artist.isEmpty());
    QCOMPARE(metadata.title, QStringLiteral("Centaur Girls"));
    QVERIFY(metadata.magazine.isEmpty());
}

// Every doujin in the collection carries its parody source in parentheses, and treating
// that as a magazine would give the library four hundred magazines that do not exist.
void FilenameMetadataTest::doesNotInventAMagazineFromAParody()
{
    const auto metadata = parseComicFileName(QStringLiteral("[Getsusekai (Motoe Hiroya)] Foxtrot (DOG DAYS) [Digital].cbz"));

    QVERIFY(metadata.magazine.isEmpty());
    QCOMPARE(metadata.title, QStringLiteral("Foxtrot (DOG DAYS)"));
}

void FilenameMetadataTest::takesOnlyTheFirstMagazine()
{
    const auto metadata = parseComicFileName(QStringLiteral("[Mizone] Jitsuroku (Comic Anthology QooPA Vol. 09) (COMIC Kairakuten 2016-08).cbz"));

    QCOMPARE(metadata.magazine, QStringLiteral("Comic Anthology QooPA Vol. 09"));
    // Both come off the title even though only one can be the magazine.
    QCOMPARE(metadata.title, QStringLiteral("Jitsuroku"));
}

QTEST_MAIN(FilenameMetadataTest)

#include "main.moc"
