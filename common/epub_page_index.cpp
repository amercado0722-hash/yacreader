#include "epub_page_index.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QUrl>
#include <QXmlStreamReader>

#include <optional>

namespace {

class HtmlEntityResolver : public QXmlStreamEntityResolver
{
public:
    QString resolveUndeclaredEntity(const QString &) override
    {
        return QStringLiteral(" ");
    }
};

struct ManifestItem {
    QString href;
    QString mediaType;
    QString properties;
};

struct SpineItem {
    QString id;
};

struct Package {
    QHash<QString, ManifestItem> manifest;
    QVector<SpineItem> spine;
    bool fixedLayout = false;
    QString coverId;
};

QString normalizedArchiveName(QString name)
{
    name.replace('\\', '/');
    while (name.startsWith('/')) {
        name.remove(0, 1);
    }
    return QDir::cleanPath(name);
}

std::optional<QString> resolvePath(const QString &documentPath, const QString &reference)
{
    const QUrl url = QUrl::fromEncoded(reference.toUtf8());
    if (!url.isRelative() || url.path().isEmpty()) {
        return std::nullopt;
    }

    QString path = url.path(QUrl::FullyDecoded);
    const bool fromArchiveRoot = path.startsWith('/');
    while (path.startsWith('/')) {
        path.remove(0, 1);
    }

    const QString baseDirectory = fromArchiveRoot ? QString() : QFileInfo(documentPath).path();
    const QString resolved = normalizedArchiveName(QDir(baseDirectory).filePath(path));
    if (resolved == QStringLiteral("..") || resolved.startsWith(QStringLiteral("../"))) {
        return std::nullopt;
    }
    return resolved;
}

QString attribute(const QXmlStreamAttributes &attributes, QStringView name)
{
    // Match local names so this handles both unqualified href and xlink:href.
    for (const QXmlStreamAttribute &value : attributes) {
        if (value.name() == name) {
            return value.value().toString();
        }
    }
    return { };
}

bool containsProperty(const QString &properties, QStringView property)
{
    const QStringList values = properties.split(' ', Qt::SkipEmptyParts);
    const QString propertyName = property.toString();
    for (const QString &value : values) {
        if (value == propertyName || value.endsWith(QStringLiteral(":") + propertyName)) {
            return true;
        }
    }
    return false;
}

std::optional<QString> packagePath(const QByteArray &containerXml, QString &error)
{
    QXmlStreamReader reader(containerXml);
    QString firstRootFile;
    QString preferredRootFile;

    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QStringLiteral("rootfile")) {
            const QString path = attribute(reader.attributes(), u"full-path");
            const QString mediaType = attribute(reader.attributes(), u"media-type");
            if (firstRootFile.isEmpty()) {
                firstRootFile = path;
            }
            if (preferredRootFile.isEmpty() && mediaType == QStringLiteral("application/oebps-package+xml")) {
                preferredRootFile = path;
            }
        }
    }

    if (reader.hasError()) {
        error = QStringLiteral("Invalid META-INF/container.xml: %1").arg(reader.errorString());
        return std::nullopt;
    }
    if (firstRootFile.isEmpty()) {
        error = QStringLiteral("META-INF/container.xml does not name a package document");
        return std::nullopt;
    }
    const QString rootFile = preferredRootFile.isEmpty() ? firstRootFile : preferredRootFile;
    const auto resolved = resolvePath(QStringLiteral("package.opf"), rootFile);
    if (!resolved) {
        error = QStringLiteral("Invalid package document path: %1").arg(rootFile);
    }
    return resolved;
}

std::optional<Package> readPackage(const QByteArray &packageXml, QString &error)
{
    QXmlStreamReader reader(packageXml);
    Package package;
    bool inManifest = false;
    bool inSpine = false;

    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isEndElement()) {
            inManifest = inManifest && reader.name() != QStringLiteral("manifest");
            inSpine = inSpine && reader.name() != QStringLiteral("spine");
            continue;
        }
        if (!reader.isStartElement()) {
            continue;
        }

        if (reader.name() == QStringLiteral("manifest")) {
            inManifest = true;
        } else if (reader.name() == QStringLiteral("spine")) {
            inSpine = true;
        } else if (inManifest && reader.name() == QStringLiteral("item")) {
            const QString id = attribute(reader.attributes(), u"id");
            package.manifest.insert(id, { attribute(reader.attributes(), u"href"), attribute(reader.attributes(), u"media-type"), attribute(reader.attributes(), u"properties") });
        } else if (inSpine && reader.name() == QStringLiteral("itemref")) {
            if (attribute(reader.attributes(), u"linear") != QStringLiteral("no")) {
                package.spine.append({ attribute(reader.attributes(), u"idref") });
            }
        } else if (reader.name() == QStringLiteral("meta")) {
            const QString property = attribute(reader.attributes(), u"property");
            const QString name = attribute(reader.attributes(), u"name");
            if (property == QStringLiteral("rendition:layout")) {
                package.fixedLayout = reader.readElementText().trimmed() == QStringLiteral("pre-paginated");
            } else if (name == QStringLiteral("fixed-layout")) {
                package.fixedLayout = attribute(reader.attributes(), u"content") == QStringLiteral("true");
            } else if (name == QStringLiteral("cover")) {
                package.coverId = attribute(reader.attributes(), u"content");
            }
        }
    }

    if (reader.hasError()) {
        error = QStringLiteral("Invalid package document: %1").arg(reader.errorString());
        return std::nullopt;
    }
    if (package.manifest.isEmpty() || package.spine.isEmpty()) {
        error = QStringLiteral("Package document has no manifest or linear spine items");
        return std::nullopt;
    }
    return package;
}

std::optional<QString> imageFromWrapper(const QByteArray &document, const QString &documentPath, const QHash<QString, int> &archiveIndexes)
{
    HtmlEntityResolver entityResolver;
    QXmlStreamReader reader(document);
    reader.setEntityResolver(&entityResolver);
    QSet<QString> images;

    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement()) {
            continue;
        }

        QString reference;
        if (reader.name() == QStringLiteral("img")) {
            reference = attribute(reader.attributes(), u"src");
        } else if (reader.name() == QStringLiteral("image")) {
            reference = attribute(reader.attributes(), u"href");
        } else if (reader.name() == QStringLiteral("object")) {
            reference = attribute(reader.attributes(), u"data");
        }
        if (reference.isEmpty()) {
            continue;
        }

        const auto resolved = resolvePath(documentPath, reference);
        if (resolved && archiveIndexes.contains(*resolved)) {
            images.insert(*resolved);
        }
    }

    if (reader.hasError()) {
        return std::nullopt;
    }
    if (images.size() != 1) {
        return std::nullopt;
    }
    return *images.constBegin();
}

struct Book {
    QHash<QString, int> archiveIndexes;
    QString packagePath;
    Package package;
};

std::optional<Book> readBook(const QStringList &fileNames, const YACReaderEpub::FileReader &readFile, QString &error)
{
    Book book;
    for (int index = 0; index < fileNames.size(); ++index) {
        book.archiveIndexes.insert(normalizedArchiveName(fileNames.at(index)), index);
    }

    const int containerIndex = book.archiveIndexes.value(QStringLiteral("META-INF/container.xml"), -1);
    if (containerIndex < 0) {
        error = QStringLiteral("Missing META-INF/container.xml");
        return std::nullopt;
    }

    const auto opfPath = packagePath(readFile(containerIndex), error);
    if (!opfPath) {
        return std::nullopt;
    }
    const int opfIndex = book.archiveIndexes.value(*opfPath, -1);
    if (opfIndex < 0) {
        error = QStringLiteral("Package document not found: %1").arg(*opfPath);
        return std::nullopt;
    }

    const auto package = readPackage(readFile(opfIndex), error);
    if (!package) {
        return std::nullopt;
    }
    book.packagePath = *opfPath;
    book.package = *package;
    return book;
}

std::optional<QString> packageCoverPath(const Book &book)
{
    for (auto item = book.package.manifest.cbegin(); item != book.package.manifest.cend(); ++item) {
        if (containsProperty(item->properties, u"cover-image") || item.key() == book.package.coverId) {
            return resolvePath(book.packagePath, item->href);
        }
    }
    return std::nullopt;
}

YACReaderEpub::PageIndex pageIndexFromBook(const Book &book, const YACReaderEpub::FileReader &readFile, const YACReaderEpub::ImageFilter &acceptImage = { })
{
    YACReaderEpub::PageIndex result;
    result.fixedLayout = book.package.fixedLayout;
    if (const auto coverPath = packageCoverPath(book)) {
        result.coverPath = *coverPath;
    }

    for (const SpineItem &spineItem : book.package.spine) {
        const auto manifestItem = book.package.manifest.constFind(spineItem.id);
        if (manifestItem == book.package.manifest.cend()) {
            continue;
        }

        const auto contentPath = resolvePath(book.packagePath, manifestItem->href);
        if (!contentPath) {
            continue;
        }

        QString imagePath;
        if (manifestItem->mediaType.startsWith(QStringLiteral("image/")) && manifestItem->mediaType != QStringLiteral("image/svg+xml")) {
            imagePath = *contentPath;
        } else if (manifestItem->mediaType == QStringLiteral("application/xhtml+xml") || manifestItem->mediaType == QStringLiteral("image/svg+xml")) {
            const int wrapperIndex = book.archiveIndexes.value(*contentPath, -1);
            if (wrapperIndex < 0) {
                continue;
            }
            const auto wrapperImage = imageFromWrapper(readFile(wrapperIndex), *contentPath, book.archiveIndexes);
            if (!wrapperImage) {
                continue;
            }
            imagePath = *wrapperImage;
        } else {
            continue;
        }

        const int imageIndex = book.archiveIndexes.value(imagePath, -1);
        if (imageIndex < 0 || (acceptImage && !acceptImage(imagePath))) {
            continue;
        }
        result.pages.append({ imagePath, imageIndex });
    }

    if (result.pages.isEmpty()) {
        result.error = QStringLiteral("Package spine contains no usable image pages");
    }
    return result;
}

}

namespace YACReaderEpub {

PageIndex readPageIndex(const QStringList &fileNames, const FileReader &readFile)
{
    QString error;
    const auto book = readBook(fileNames, readFile, error);
    if (!book) {
        PageIndex result;
        result.error = error;
        return result;
    }
    return pageIndexFromBook(*book, readFile);
}

ScanInfo readScanInfo(const QStringList &fileNames, const FileReader &readFile, int coverPage, const ImageFilter &acceptImage)
{
    ScanInfo result;
    const auto book = readBook(fileNames, readFile, result.error);
    if (!book) {
        return result;
    }

    const auto scanAllPages = [&] {
        ScanInfo fullResult;
        const PageIndex pages = pageIndexFromBook(*book, readFile, acceptImage);
        fullResult.error = pages.error;
        fullResult.pageCount = static_cast<int>(pages.pages.size());
        if (fullResult.pageCount > 0) {
            const int coverIndex = coverPage > 0 && coverPage <= fullResult.pageCount ? coverPage - 1 : 0;
            fullResult.coverArchiveIndex = pages.pages.at(coverIndex).archiveIndex;
        }
        return fullResult;
    };

    if (!book->package.fixedLayout) {
        return scanAllPages();
    }

    struct Candidate {
        QString path;
        bool wrapper = false;
    };
    QVector<Candidate> candidates;
    for (const SpineItem &spineItem : book->package.spine) {
        const auto manifestItem = book->package.manifest.constFind(spineItem.id);
        if (manifestItem == book->package.manifest.cend()) {
            continue;
        }
        const auto contentPath = resolvePath(book->packagePath, manifestItem->href);
        if (!contentPath || !book->archiveIndexes.contains(*contentPath)) {
            continue;
        }

        if (manifestItem->mediaType.startsWith(QStringLiteral("image/")) && manifestItem->mediaType != QStringLiteral("image/svg+xml")) {
            if (!acceptImage || acceptImage(*contentPath)) {
                candidates.append({ *contentPath, false });
            }
        } else if (manifestItem->mediaType == QStringLiteral("application/xhtml+xml") || manifestItem->mediaType == QStringLiteral("image/svg+xml")) {
            candidates.append({ *contentPath, true });
        }
    }

    result.pageCount = static_cast<int>(candidates.size());
    if (result.pageCount == 0) {
        result.error = QStringLiteral("Package spine contains no usable image pages");
        return result;
    }

    if (coverPage <= 1) {
        const auto coverPath = packageCoverPath(*book);
        if (coverPath && book->archiveIndexes.contains(*coverPath) && (!acceptImage || acceptImage(*coverPath))) {
            result.coverArchiveIndex = book->archiveIndexes.value(*coverPath);
            return result;
        }
    }

    const int coverIndex = coverPage > 0 && coverPage <= result.pageCount ? coverPage - 1 : 0;
    const Candidate &cover = candidates.at(coverIndex);
    QString imagePath = cover.path;
    if (cover.wrapper) {
        const int wrapperIndex = book->archiveIndexes.value(cover.path);
        const auto wrapperImage = imageFromWrapper(readFile(wrapperIndex), cover.path, book->archiveIndexes);
        if (!wrapperImage || (acceptImage && !acceptImage(*wrapperImage))) {
            return scanAllPages();
        }
        imagePath = *wrapperImage;
    }
    result.coverArchiveIndex = book->archiveIndexes.value(imagePath, -1);
    return result;
}

}
