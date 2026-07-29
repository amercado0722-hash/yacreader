#include "epub_page_index.h"

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QTest>

class EpubPageIndexTest : public QObject
{
    Q_OBJECT

private slots:
    void followsSpineOrderAndMetadata();
    void resolvesRelativeAndEncodedPaths();
    void readsObjectWrapper();
    void toleratesHtmlNamedEntities();
    void preservesDuplicateSpineReferences();
    void skipsBrokenSpineItems();
    void rejectsWrappersWithMultipleImages();
    void rejectsPathsOutsideTheArchive();
};

namespace {

YACReaderEpub::PageIndex readIndex(const QStringList &fileNames, const QHash<QString, QByteArray> &files)
{
    return YACReaderEpub::readPageIndex(fileNames, [&](int index) { return files.value(fileNames.at(index)); });
}

QByteArray containerXml()
{
    return R"(<?xml version="1.0"?>
        <container xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
          <rootfiles>
            <rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>
          </rootfiles>
        </container>)";
}

}

void EpubPageIndexTest::followsSpineOrderAndMetadata()
{
    const QHash<QString, QByteArray> files {
        { "META-INF/container.xml", containerXml() },
        { "OEBPS/content.opf", R"(<package xmlns="http://www.idpf.org/2007/opf">
            <metadata>
              <meta property="rendition:layout">pre-paginated</meta>
            </metadata>
            <manifest>
              <item id="second" href="pages/b.xhtml" media-type="application/xhtml+xml"/>
              <item id="first" href="pages/a.xhtml" media-type="application/xhtml+xml"/>
              <item id="notes" href="pages/notes.xhtml" media-type="application/xhtml+xml"/>
              <item id="cover" href="images/cover.jpg" media-type="image/jpeg" properties="cover-image"/>
            </manifest>
            <spine>
              <itemref idref="first"/>
              <itemref idref="notes" linear="no"/>
              <itemref idref="second"/>
            </spine>
          </package>)" },
        { "OEBPS/pages/a.xhtml", R"(<html><body><img src="../images/z.jpg"/></body></html>)" },
        { "OEBPS/pages/b.xhtml", R"(<html><body><img src="../images/a.jpg"/></body></html>)" },
        { "OEBPS/pages/notes.xhtml", R"(<html><body><img src="../images/notes.jpg"/></body></html>)" },
        { "OEBPS/images/a.jpg", "a" },
        { "OEBPS/images/z.jpg", "z" },
        { "OEBPS/images/notes.jpg", "notes" },
        { "OEBPS/images/cover.jpg", "cover" },
    };
    const QStringList fileNames = files.keys();

    const auto result = readIndex(fileNames, files);

    QVERIFY2(result.isValid(), qPrintable(result.error));
    QCOMPARE(result.pages.size(), 2);
    QCOMPARE(result.pages.at(0).fileName, QString("OEBPS/images/z.jpg"));
    QCOMPARE(result.pages.at(1).fileName, QString("OEBPS/images/a.jpg"));
    QVERIFY(result.fixedLayout);
    QCOMPARE(result.coverPath, QString("OEBPS/images/cover.jpg"));
}

void EpubPageIndexTest::resolvesRelativeAndEncodedPaths()
{
    const QHash<QString, QByteArray> files {
        { "META-INF/container.xml", containerXml() },
        { "OEBPS/content.opf", R"(<package><manifest>
              <item id="page" href="pages/page.svg" media-type="image/svg+xml"/>
            </manifest><spine><itemref idref="page"/></spine></package>)" },
        { "OEBPS/pages/page.svg", R"(<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink">
              <image xlink:href="../images/Page%2001.jpg#image"/>
            </svg>)" },
        { "OEBPS/images/Page 01.jpg", "page" },
    };
    const QStringList fileNames = files.keys();

    const auto result = readIndex(fileNames, files);

    QVERIFY2(result.isValid(), qPrintable(result.error));
    QCOMPARE(result.pages.constFirst().fileName, QString("OEBPS/images/Page 01.jpg"));
}

void EpubPageIndexTest::readsObjectWrapper()
{
    const QHash<QString, QByteArray> files {
        { "META-INF/container.xml", containerXml() },
        { "OEBPS/content.opf", R"(<package><manifest>
              <item id="page" href="page.xhtml" media-type="application/xhtml+xml"/>
            </manifest><spine><itemref idref="page"/></spine></package>)" },
        { "OEBPS/page.xhtml", R"(<html><body><object data="images/page.jpg"/></body></html>)" },
        { "OEBPS/images/page.jpg", "page" },
    };
    const QStringList fileNames = files.keys();

    const auto result = readIndex(fileNames, files);

    QVERIFY2(result.isValid(), qPrintable(result.error));
    QCOMPARE(result.pages.constFirst().fileName, QString("OEBPS/images/page.jpg"));
}

void EpubPageIndexTest::toleratesHtmlNamedEntities()
{
    const QHash<QString, QByteArray> files {
        { "META-INF/container.xml", containerXml() },
        { "OEBPS/content.opf", R"(<package><manifest>
              <item id="page" href="page.xhtml" media-type="application/xhtml+xml"/>
            </manifest><spine><itemref idref="page"/></spine></package>)" },
        { "OEBPS/page.xhtml", R"(<html><body><p>&nbsp;</p><img src="images/page.jpg"/></body></html>)" },
        { "OEBPS/images/page.jpg", "page" },
    };
    const QStringList fileNames = files.keys();

    const auto result = readIndex(fileNames, files);

    QVERIFY2(result.isValid(), qPrintable(result.error));
    QCOMPARE(result.pages.constFirst().fileName, QString("OEBPS/images/page.jpg"));
}

void EpubPageIndexTest::preservesDuplicateSpineReferences()
{
    const QHash<QString, QByteArray> files {
        { "META-INF/container.xml", containerXml() },
        { "OEBPS/content.opf", R"(<package><manifest>
              <item id="page" href="images/page.jpg" media-type="image/jpeg"/>
            </manifest><spine><itemref idref="page"/><itemref idref="page"/></spine></package>)" },
        { "OEBPS/images/page.jpg", "page" },
    };
    const QStringList fileNames = files.keys();

    const auto result = readIndex(fileNames, files);

    QVERIFY2(result.isValid(), qPrintable(result.error));
    QCOMPARE(result.pages.size(), 2);
    QCOMPARE(result.pages.at(0).archiveIndex, result.pages.at(1).archiveIndex);
}

void EpubPageIndexTest::skipsBrokenSpineItems()
{
    const QHash<QString, QByteArray> files {
        { "META-INF/container.xml", containerXml() },
        { "OEBPS/content.opf", R"(<package><manifest>
              <item id="broken" href="broken.xhtml" media-type="application/xhtml+xml"/>
              <item id="valid" href="valid.xhtml" media-type="application/xhtml+xml"/>
            </manifest><spine><itemref idref="broken"/><itemref idref="valid"/></spine></package>)" },
        { "OEBPS/broken.xhtml", R"(<html><body><img src="one.jpg"/><img src="two.jpg"/></body></html>)" },
        { "OEBPS/valid.xhtml", R"(<html><body><img src="valid.jpg"/></body></html>)" },
        { "OEBPS/one.jpg", "one" },
        { "OEBPS/two.jpg", "two" },
        { "OEBPS/valid.jpg", "valid" },
    };
    const QStringList fileNames = files.keys();

    const auto result = readIndex(fileNames, files);

    QVERIFY2(result.isValid(), qPrintable(result.error));
    QCOMPARE(result.pages.size(), 1);
    QCOMPARE(result.pages.constFirst().fileName, QString("OEBPS/valid.jpg"));
}

void EpubPageIndexTest::rejectsWrappersWithMultipleImages()
{
    const QHash<QString, QByteArray> files {
        { "META-INF/container.xml", containerXml() },
        { "OEBPS/content.opf", R"(<package><manifest>
              <item id="page" href="page.xhtml" media-type="application/xhtml+xml"/>
            </manifest><spine><itemref idref="page"/></spine></package>)" },
        { "OEBPS/page.xhtml", R"(<html><body><img src="one.jpg"/><img src="two.jpg"/></body></html>)" },
        { "OEBPS/one.jpg", "one" },
        { "OEBPS/two.jpg", "two" },
    };
    const QStringList fileNames = files.keys();

    const auto result = readIndex(fileNames, files);

    QVERIFY(!result.isValid());
    QVERIFY(result.error.contains("no usable image pages"));
}

void EpubPageIndexTest::rejectsPathsOutsideTheArchive()
{
    const QHash<QString, QByteArray> files {
        { "META-INF/container.xml", containerXml() },
        { "OEBPS/content.opf", R"(<package><manifest>
              <item id="page" href="../../page.jpg" media-type="image/jpeg"/>
            </manifest><spine><itemref idref="page"/></spine></package>)" },
        { "page.jpg", "page" },
    };
    const QStringList fileNames = files.keys();

    const auto result = readIndex(fileNames, files);

    QVERIFY(!result.isValid());
    QVERIFY(result.error.contains("no usable image pages"));
}

QTEST_GUILESS_MAIN(EpubPageIndexTest)

#include "main.moc"
