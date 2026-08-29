#include "compressed_archive.h"

#include "extract_delegate.h"

#include <QDebug>
#include <QFileInfo>

#include <unarr.h>

CompressedArchive::CompressedArchive(const QString &filePath, QObject *parent)
    : QObject(parent), tools(true), valid(false), numFiles(0), ar(NULL), stream(NULL)
{
    // open file
#ifdef Q_OS_WIN
    stream = ar_open_file_w((wchar_t *)filePath.utf16());
#else
    stream = ar_open_file(filePath.toLocal8Bit().constData());
#endif
    if (!stream) {
        return;
    }

    // open archive
    ar = ar_open_rar_archive(stream);
    // TODO: build unarr with 7z support and test this!
    if (!ar)
        ar = ar_open_7z_archive(stream);
    if (!ar)
        ar = ar_open_tar_archive(stream);
    // zip detection is costly, so it comes last...
    if (!ar)
        ar = ar_open_zip_archive(stream, false);
    if (!ar) {
        return;
    }

    // initial parse
    while (ar_parse_entry(ar)) {
        // make sure we really got a file header
        if (ar_entry_get_size(ar) > 0) {
            fileNames.append(ar_entry_get_name(ar));
            offsets.append(ar_entry_get_offset(ar));
            numFiles++;
        }
    }
    if (!ar_at_eof(ar)) {
        // fail if the initial parse didn't reach EOF
        // this might be a bit too drastic
        qDebug() << "Error while parsing archive";
        return;
    }
    if (numFiles > 0) {
        valid = true;
    }
}

CompressedArchive::~CompressedArchive()
{
    ar_close_archive(ar);
    ar_close(stream);
}

QList<QString> CompressedArchive::getFileNames()
{
    return fileNames;
}

bool CompressedArchive::isValid()
{
    return valid;
}

bool CompressedArchive::toolsLoaded()
{
    // for backwards compatibilty
    return tools;
}

int CompressedArchive::getNumFiles()
{
    return numFiles;
}

void CompressedArchive::getAllData(const QVector<quint32> &indexes, ExtractDelegate *delegate)
{
    if (indexes.isEmpty())
        return;

    QByteArray buffer;

    int i = 0;
    while (i < indexes.count()) {
        if (delegate == nullptr || delegate->isCancelled()) {
            return;
        }

        const quint32 pageIndex = indexes.at(i);

        // offsets.at() on an out of range index is undefined behaviour in a release
        // build, so a bad index has to be rejected before it gets there.
        if (pageIndex >= static_cast<quint32>(offsets.size())) {
            qDebug() << "getAllData called with out of range index:" << pageIndex;
            delegate->crcError(pageIndex);
            i++;
            continue;
        }

        // use the offset list so we generated so we're not getting any non-page files
        if (!ar_parse_entry_at(ar, offsets.at(pageIndex))) { // set ar_entry to start of indexes
            qDebug() << "unable to parse entry at index:" << pageIndex;
            delegate->crcError(pageIndex);
            i++;
            continue;
        }

        const size_t entrySize = ar_entry_get_size(ar);
        if (entrySize == 0 || entrySize > kMaxEntrySize) {
            qDebug() << "refusing to read entry with implausible size:" << entrySize;
            delegate->crcError(pageIndex);
            i++;
            continue;
        }

        buffer.resize(static_cast<qsizetype>(entrySize));
        if (ar_entry_uncompress(ar, buffer.data(), buffer.size())) // did we extract it?
        {
            delegate->fileExtracted(pageIndex, buffer); // return extracted file
        } else {
            delegate->crcError(pageIndex); // we could not extract it...
        }
        i++;
    }
}

QByteArray CompressedArchive::getRawDataAtIndex(int index)
{
    QByteArray buffer;
    if (index >= 0 && index < getNumFiles() && index < offsets.size()) {
        if (!ar_parse_entry_at(ar, offsets.at(index))) {
            qDebug() << "unable to parse entry at index:" << index;
            return QByteArray();
        }

        const size_t entrySize = ar_entry_get_size(ar);
        if (entrySize == 0 || entrySize > kMaxEntrySize) {
            qDebug() << "refusing to read entry with implausible size:" << entrySize;
            return QByteArray();
        }

        buffer.resize(static_cast<qsizetype>(entrySize));
        if (ar_entry_uncompress(ar, buffer.data(), buffer.size())) {
            return buffer;
        } else {
            return QByteArray();
        }
    }
    return buffer;
}
