#include "series_name_utils.h"

#include <QtTest>

// The cases below are taken from a real downloaded manga library, because that is where
// this cleaning has to work: the folder name is the only title such a library has, and
// the metadata lookup is searching it verbatim.
class SeriesNameTest : public QObject
{
    Q_OBJECT

private slots:
    void displayNameStripsReleaseTags();
    void displayNameStripsReleaseTags_data();
    void displayNameKeepsEditionMarkers();
    void displayNameKeepsEditionMarkers_data();
    void displayNameNeverEmptiesATitle();
    void displayNameNeverEmptiesATitle_data();
    void searchNameStripsEverythingTrailing();
    void searchNameStripsEverythingTrailing_data();
};

void SeriesNameTest::displayNameStripsReleaseTags_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("scene tag") << "A Bride's Story (Digital) (1r0n)" << "A Bride's Story";
    QTest::newRow("two groups") << "Citrus+ (Digital) (1r0n + danke-Empire)" << "Citrus+";
    QTest::newRow("other group") << "Berserk (Digital) (danke-Empire)" << "Berserk";
    QTest::newRow("leading hash") << "#DRCL midnight children (Digital) (1r0n)" << "#DRCL midnight children";
    QTest::newRow("scanner bracket") << "Alfie [InCase]" << "Alfie";
    QTest::newRow("dashes kept") << "Dragon Ball - Digital Colored Comics - Freeza Arc (Digital) (LuCaZ)" << "Dragon Ball - Digital Colored Comics - Freeza Arc";
    QTest::newRow("no tags") << "Akane-banashi" << "Akane-banashi";
}

void SeriesNameTest::displayNameStripsReleaseTags()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);

    QCOMPARE(YACReader::cleanSeriesDisplayName(input), expected);
}

void SeriesNameTest::displayNameKeepsEditionMarkers_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    // Two folders of the same series differ only by these markers, so dropping them for
    // display would leave the user with entries they cannot tell apart.
    QTest::newRow("manga up") << "Are You Okay with a Slightly Older Girlfriend (Manga UP!) (Digital) (1r0n)" << "Are You Okay with a Slightly Older Girlfriend (Manga UP!)";
    QTest::newRow("square enix") << "Are You Okay with a Slightly Older Girlfriend (Square Enix) (Digital) (1r0n)" << "Are You Okay with a Slightly Older Girlfriend (Square Enix)";
    QTest::newRow("viz edition") << "Ayakashi Triangle (VIZ Edition) (Digital) (1r0n)" << "Ayakashi Triangle (VIZ Edition)";
    QTest::newRow("omnibus") << "Break of Dawn (Omnibus Edition) (Digital) (1r0n)" << "Break of Dawn (Omnibus Edition)";

    // A publisher in brackets is the same kind of marker as one in parentheses. A library
    // can hold the manga and the light novel of one series side by side, and the tag is
    // the only thing telling them apart.
    QTest::newRow("j-novel club") << "The Ideal Sponger Life [J-Novel Club]" << "The Ideal Sponger Life [J-Novel Club]";
    QTest::newRow("yen press") << "Strike the Blood [Yen Press]" << "Strike the Blood [Yen Press]";
    QTest::newRow("j-novel club tagged") << "The Ideal Sponger Life [J-Novel Club] (Digital) (1r0n)" << "The Ideal Sponger Life [J-Novel Club]";
    QTest::newRow("decensored kept") << "Some Series [Decensored]" << "Some Series [Decensored]";
}

void SeriesNameTest::displayNameKeepsEditionMarkers()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);

    QCOMPARE(YACReader::cleanSeriesDisplayName(input), expected);
}

void SeriesNameTest::displayNameNeverEmptiesATitle_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    // A title that is entirely brackets or parentheses is a title, not a tag
    QTest::newRow("bracketed title") << "[Oshi No Ko]" << "[Oshi No Ko]";
    QTest::newRow("bracketed and tagged") << "[Oshi No Ko] (Digital) (1r0n)" << "[Oshi No Ko]";
    QTest::newRow("parenthesised title") << "(Abu)Normal (Digital) (1r0n)" << "(Abu)Normal";
    QTest::newRow("empty") << "" << "";
}

void SeriesNameTest::displayNameNeverEmptiesATitle()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);

    QCOMPARE(YACReader::cleanSeriesDisplayName(input), expected);
}

void SeriesNameTest::searchNameStripsEverythingTrailing_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    // The provider is being asked about the series, so the printing does not matter
    QTest::newRow("edition dropped") << "Are You Okay with a Slightly Older Girlfriend (Manga UP!) (Digital) (1r0n)" << "Are You Okay with a Slightly Older Girlfriend";
    QTest::newRow("viz edition dropped") << "Ayakashi Triangle (VIZ Edition) (Digital) (1r0n)" << "Ayakashi Triangle";
    QTest::newRow("brackets unwrapped") << "[Oshi No Ko] (Digital) (1r0n)" << "Oshi No Ko";
    // The display name keeps a publisher tag; the search name must not, or the provider
    // is asked about one printing rather than about the series.
    QTest::newRow("j-novel club dropped") << "The Ideal Sponger Life [J-Novel Club]" << "The Ideal Sponger Life";
    QTest::newRow("yen press dropped") << "Strike the Blood [Yen Press]" << "Strike the Blood";
    QTest::newRow("plain title") << "A Bride's Story (Digital) (1r0n)" << "A Bride's Story";
    QTest::newRow("no tags") << "Akane-banashi" << "Akane-banashi";
    QTest::newRow("empty") << "" << "";
}

void SeriesNameTest::searchNameStripsEverythingTrailing()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);

    QCOMPARE(YACReader::cleanSeriesSearchName(input), expected);
}

QTEST_MAIN(SeriesNameTest)

#include "main.moc"
