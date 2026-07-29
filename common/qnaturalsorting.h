

#ifndef __QNATURALSORTING_H
#define __QNATURALSORTING_H

#include "library_item.h"

#include <QFileInfo>
#include <QString>
#include <QVariant>

int naturalCompare(const QString &s1, const QString &s2, Qt::CaseSensitivity caseSensitivity);
bool naturalSortLessThanCS(const QString &left, const QString &right);
bool naturalSortLessThanCI(const QString &left, const QString &right);
bool naturalSortLessThanCIFileInfo(const QFileInfo &left, const QFileInfo &right);
bool naturalSortLessThanCILibraryItem(LibraryItem *left, LibraryItem *right);

/* The order comics are read in. Issue number wins when both comics have one,
 * numbered comics come before unnumbered ones, and file name breaks the tie
 * otherwise. Every place that lists the comics of a folder must use this, so
 * that what YACReaderLibrary shows and what YACReader walks with next/previous
 * are the same sequence.
 **/
bool comicNumberLessThan(const QVariant &leftNumber, const QString &leftName,
                         const QVariant &rightNumber, const QString &rightName);

/* Name-only ordering for server responses that mix folders and comics in a single
 * list, where there is no issue number to lean on. Reading order is not this: for
 * the comics of a folder use DBHelper::getFolderComicsFromLibraryForReading, which
 * sorts by issue number the way the clients do.
 **/
struct LibraryItemSorter {
    bool operator()(const LibraryItem *a, const LibraryItem *b) const
    {
        return naturalSortLessThanCI(a->name, b->name);
    }
};

#endif
