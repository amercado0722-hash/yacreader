#include "volume_number_utils.h"

#include <QtTest>

// The cases below are file names from a real downloaded manga library. In such a library
// the volume number exists only in the file name, so a scraper writing "this is volume 7"
// has to read it back out of the name, and reading it wrong tags the whole series wrongly.
class VolumeNumberTest : public QObject
{
    Q_OBJECT

private slots:
    void readsVolumeMarkers();
    void readsVolumeMarkers_data();
    void readsBareChapterNumbers();
    void readsBareChapterNumbers_data();
    void ignoresPublicationYears();
    void ignoresPublicationYears_data();
    void returnsNothingForOneShots();
    void returnsNothingForOneShots_data();
    void readsTheSeriesNameBackOut();
    void readsTheSeriesNameBackOut_data();
    void stripsOnlyMarkedVolumeNumbers();
    void stripsOnlyMarkedVolumeNumbers_data();
    void normalizesLeadingZeros();
    void normalizesLeadingZeros_data();
};

void VolumeNumberTest::readsVolumeMarkers_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("padded") << "A Bride's Story v07 (2024).cbz" << "07";
    QTest::newRow("unpadded") << "Some Series v7 (2024).cbz" << "7";
    QTest::newRow("dotted word") << "Some Series Vol. 3.cbz" << "3";
    QTest::newRow("word") << "Some Series Vol 12.cbz" << "12";
    // An omnibus covers a span; it is filed under the first volume it contains.
    QTest::newRow("omnibus span") << "Booty Royale - Never Go Down Without a Fight! v01-02 (2021).cbz" << "01";
    QTest::newRow("whole run") << "Bleach - v01-v74 COMPLETE (VIZ Digital).cbz" << "01";
    // A title that is itself numeric must not swallow the volume marker
    QTest::newRow("numeric title") << "86--EIGHTY-SIX - Operation High School v03 (2024).cbz" << "03";
    QTest::newRow("hash title") << "#DRCL midnight children v01 (2023).cbz" << "01";
    QTest::newRow("epub") << "Strike the Blood v01 - The Right Arm of the Saint.epub" << "01";
}

void VolumeNumberTest::readsVolumeMarkers()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);

    QCOMPARE(YACReader::volumeNumberFromFileName(input), expected);
}

void VolumeNumberTest::readsBareChapterNumbers_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("three digits") << "One-Punch Man 207 (2025).cbz" << "207";
    QTest::newRow("zero padded") << "My Clueless First Friend 001 (2022).cbz" << "001";
    // Half chapters are real and are not integers, which is why the number is kept as text
    QTest::newRow("half chapter") << "Insomniacs After School 125.1 (2023).cbz" << "125.1";
}

void VolumeNumberTest::readsBareChapterNumbers()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);

    QCOMPARE(YACReader::volumeNumberFromFileName(input), expected);
}

void VolumeNumberTest::ignoresPublicationYears_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    // The year is the trap: it is a number, it sits at the end of almost every file in the
    // library, and taking it would tag a whole series as volume 2024.
    QTest::newRow("year only") << "A Vampire in the Bathhouse (2025).cbz" << "";
    QTest::newRow("year after volume") << "A Bride's Story v07 (2024).cbz" << "07";
    QTest::newRow("year range") << "Some Collection (2019-2021).cbz" << "";
    QTest::newRow("chapter then year") << "One-Punch Man 207 (2025).cbz" << "207";
}

void VolumeNumberTest::ignoresPublicationYears()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);

    QCOMPARE(YACReader::volumeNumberFromFileName(input), expected);
}

void VolumeNumberTest::returnsNothingForOneShots_data()
{
    QTest::addColumn<QString>("input");

    // A single-volume work has no number, and inventing one for it is worse than leaving
    // the field empty.
    QTest::newRow("one shot") << "About a Love Song (2025).cbz";
    QTest::newRow("art book") << "Alya Sometimes Hides Her Feelings in Russian - Momoco's Art Book (2025).cbz";
    QTest::newRow("collection") << "Alley - Junji Ito Story Collection (2024).cbz";
    QTest::newRow("empty") << "";
}

void VolumeNumberTest::returnsNothingForOneShots()
{
    QFETCH(QString, input);

    QCOMPARE(YACReader::volumeNumberFromFileName(input), QString(""));
}

void VolumeNumberTest::normalizesLeadingZeros_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("padded") << "07" << "7";
    QTest::newRow("triple padded") << "001" << "1";
    QTest::newRow("unpadded") << "12" << "12";
    QTest::newRow("fraction kept") << "125.1" << "125.1";
    QTest::newRow("padded fraction") << "005.5" << "5.5";
    QTest::newRow("zero stays") << "0" << "0";
    QTest::newRow("empty") << "" << "";
}

void VolumeNumberTest::normalizesLeadingZeros()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);

    QCOMPARE(YACReader::normalizedVolumeNumber(input), expected);
}

void VolumeNumberTest::readsTheSeriesNameBackOut_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    // A loose volume dropped into the library has to find its own series folder, and the
    // only thing it knows about itself is its file name.
    QTest::newRow("volume marker") << "A Bride's Story v07 (2024)" << "A Bride's Story";
    QTest::newRow("padded marker") << "Berserk - Deluxe Edition v03" << "Berserk - Deluxe Edition";
    QTest::newRow("long marker") << "Some Series Vol. 3" << "Some Series";
    QTest::newRow("bare chapter") << "One-Punch Man 207 (2025)" << "One-Punch Man";
    QTest::newRow("no number at all") << "Sayuri" << "Sayuri";
    // A title that is only a number must survive whole, or "86" files itself as nothing.
    QTest::newRow("number is the title") << "86" << "86";
    QTest::newRow("number starts the title") << "12 Beast" << "12 Beast";
}

void VolumeNumberTest::readsTheSeriesNameBackOut()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);

    QCOMPARE(YACReader::seriesNameFromVolumeFileName(input), expected);
}

void VolumeNumberTest::stripsOnlyMarkedVolumeNumbers_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    // Folder names get this rather than the full reading above, because a folder name that
    // ends in a number is far more likely to be a title than a volume.
    QTest::newRow("range") << "A Silent Voice v01-07 (Digital) (danke-Empire)" << "A Silent Voice";
    QTest::newRow("single") << "A Bride's Story v07" << "A Bride's Story";
    QTest::newRow("long form") << "Some Series Vol. 3" << "Some Series";
    QTest::newRow("number is the title") << "Zom 100" << "Zom 100";
    QTest::newRow("number starts the title") << "12 Beast" << "12 Beast";
    QTest::newRow("bare trailing number kept") << "A Couple of Cuckoos 208" << "A Couple of Cuckoos 208";
    QTest::newRow("no number") << "Berserk" << "Berserk";
}

void VolumeNumberTest::stripsOnlyMarkedVolumeNumbers()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);

    QCOMPARE(YACReader::withoutVolumeMarker(input), expected);
}

QTEST_MAIN(VolumeNumberTest)

#include "main.moc"
