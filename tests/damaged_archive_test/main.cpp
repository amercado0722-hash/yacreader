#include "compressed_archive.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

// Regression tests for damaged and truncated comic archives.
//
// A large library almost always contains a few of these: an interrupted download, a
// file that was still being written when it was copied, a .cbz that is not really an
// archive at all. None of them should be able to crash the reader, and none of them
// should be able to hand back a buffer that is passed off as a comic page.
//
// These run against whichever decompression backend the build was configured with, so
// the whole matrix (7zip, libarchive, unarr) is covered by CI.
class DamagedArchiveTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void emptyFileIsNotAValidArchive();
    void truncatedSignatureIsNotAValidArchive();
    void archiveWithGarbageBodyDoesNotYieldPages();
    void outOfRangeIndexesReturnNoData();
    void textFileNamedAsComicIsRejected();

private:
    QString writeFile(const QString &name, const QByteArray &contents);

    QTemporaryDir dir;
};

void DamagedArchiveTest::initTestCase()
{
    QVERIFY2(dir.isValid(), "unable to create a temporary directory for the test files");
}

QString DamagedArchiveTest::writeFile(const QString &name, const QByteArray &contents)
{
    const QString path = QDir(dir.path()).filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return QString();
    }
    file.write(contents);
    file.close();
    return path;
}

void DamagedArchiveTest::emptyFileIsNotAValidArchive()
{
    const QString path = writeFile(QStringLiteral("empty.cbz"), QByteArray());
    QVERIFY(!path.isEmpty());

    CompressedArchive archive(path);

    QVERIFY(!archive.isValid());
    QCOMPARE(archive.getNumFiles(), 0);
    QVERIFY(archive.getFileNames().isEmpty());
    QVERIFY(archive.getRawDataAtIndex(0).isEmpty());
}

void DamagedArchiveTest::truncatedSignatureIsNotAValidArchive()
{
    // Shorter than every signature the format sniffer compares against. Reading past
    // the end of the buffer while sniffing is exactly what an interrupted download used
    // to provoke.
    const QList<QByteArray> truncated = {
        QByteArray("P"),
        QByteArray("PK"),
        QByteArray("PK\x03", 3),
        QByteArray("Rar"),
        QByteArray("Rar!\x1A", 5),
        QByteArray("7z\xBC", 3),
    };

    int n = 0;
    for (const QByteArray &contents : truncated) {
        const QString path = writeFile(QStringLiteral("truncated%1.cbz").arg(n++), contents);
        QVERIFY(!path.isEmpty());

        CompressedArchive archive(path);

        QVERIFY(!archive.isValid());
        QVERIFY(archive.getRawDataAtIndex(0).isEmpty());
    }
}

void DamagedArchiveTest::archiveWithGarbageBodyDoesNotYieldPages()
{
    // A convincing ZIP signature followed by nothing usable. Whether the backend calls
    // this valid or not, it must not produce page data.
    QByteArray contents("PK\x03\x04", 4);
    contents.append(QByteArray(4096, '\xA5'));

    const QString path = writeFile(QStringLiteral("garbage.cbz"), contents);
    QVERIFY(!path.isEmpty());

    CompressedArchive archive(path);

    for (int i = 0; i < archive.getNumFiles(); ++i) {
        QVERIFY(archive.getRawDataAtIndex(i).isEmpty());
    }
}

void DamagedArchiveTest::outOfRangeIndexesReturnNoData()
{
    const QString path = writeFile(QStringLiteral("range.cbz"), QByteArray("PK\x03\x04", 4));
    QVERIFY(!path.isEmpty());

    CompressedArchive archive(path);

    QVERIFY(archive.getRawDataAtIndex(-1).isEmpty());
    QVERIFY(archive.getRawDataAtIndex(1000000).isEmpty());
}

void DamagedArchiveTest::textFileNamedAsComicIsRejected()
{
    const QString path = writeFile(QStringLiteral("not_a_comic.cbz"),
                                   QByteArray("This is a plain text file that someone renamed to .cbz\n"));
    QVERIFY(!path.isEmpty());

    CompressedArchive archive(path);

    QVERIFY(!archive.isValid());
    QVERIFY(archive.getRawDataAtIndex(0).isEmpty());
}

QTEST_MAIN(DamagedArchiveTest)

#include "main.moc"
