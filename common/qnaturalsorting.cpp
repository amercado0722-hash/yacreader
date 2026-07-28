#include "qnaturalsorting.h"

#include <QCollator>

static QCollator collatorCI = [] {
    QCollator c;
    c.setNumericMode(true);
    c.setIgnorePunctuation(false);
    c.setCaseSensitivity(Qt::CaseInsensitive);
    return c;
}();

static QCollator collatorCS = [] {
    QCollator c;
    c.setNumericMode(true);
    c.setIgnorePunctuation(false);
    c.setCaseSensitivity(Qt::CaseSensitive);
    return c;
}();

int naturalCompare(const QString &s1, const QString &s2, Qt::CaseSensitivity caseSensitivity)
{
    QCollator &c = (caseSensitivity == Qt::CaseSensitive) ? collatorCS : collatorCI;
    return c.compare(s1, s2);
}
bool naturalSortLessThanCS(const QString &left, const QString &right)
{
    return (naturalCompare(left, right, Qt::CaseSensitive) < 0);
}

bool naturalSortLessThanCI(const QString &left, const QString &right)
{
    return (naturalCompare(left, right, Qt::CaseInsensitive) < 0);
}

bool comicNumberLessThan(const QVariant &leftNumber, const QString &leftName,
                         const QVariant &rightNumber, const QString &rightName)
{
    if (leftNumber.isNull() && rightNumber.isNull())
        return naturalSortLessThanCI(leftName, rightName);

    if (!leftNumber.isNull() && !rightNumber.isNull())
        return naturalSortLessThanCI(leftNumber.toString(), rightNumber.toString());

    return rightNumber.isNull();
}

bool naturalSortLessThanCIFileInfo(const QFileInfo &left, const QFileInfo &right)
{
    return naturalSortLessThanCI(left.fileName(), right.fileName());
}

bool naturalSortLessThanCILibraryItem(LibraryItem *left, LibraryItem *right)
{
    return naturalSortLessThanCI(left->name, right->name);
}
