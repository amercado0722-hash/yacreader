#ifndef SERIES_CAROUSEL_VIEW_H
#define SERIES_CAROUSEL_VIEW_H

#include "themable.h"

#include <QList>
#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QWidget>

class ComicFlowWidget;
class FolderModel;
class QLabel;
class QSettings;

// A carousel of the series in a library, turning on its axis.
//
// This is a browsing view rather than a finding view, and the distinction matters at this
// library's size: a carousel shows you eight things at a time, so reaching a named series
// out of nineteen hundred is the search box's job, not this one's. What it is good at is
// the thing a shelf is good at - moving along and seeing what is there.
//
// It reuses the flow engine the reader already ships. That engine takes a list of image
// paths and hands back indexes, and knows nothing about comics, so pointing it at series
// covers costs a list of paths rather than a rewrite.
class SeriesCarouselView : public QWidget, protected Themable
{
    Q_OBJECT

public:
    explicit SeriesCarouselView(QWidget *parent = nullptr);

    // The folders shown are the children of parentIndex, so the carousel follows you into
    // a folder rather than always showing the top of the library.
    void setFolderModel(FolderModel *model, const QModelIndex &parentIndex = QModelIndex());
    void reload();
    int seriesCount() const;

signals:
    // The source model index of the chosen series, which is what the navigation
    // controller already knows how to open.
    void folderSelected(const QModelIndex &sourceIndex);

protected:
    void applyTheme(const Theme &theme) override;
    // The engine only answers the keyboard while it holds focus, and a view living in a
    // stack does not get it merely by being constructed.
    void showEvent(QShowEvent *event) override;

private:
    void showCaptionFor(int index);

    ComicFlowWidget *flow;
    QSettings *settings;
    QLabel *titleLabel;
    QLabel *countLabel;

    FolderModel *folderModel = nullptr;
    QPersistentModelIndex parentFolder;
    QList<QPersistentModelIndex> series;
    QStringList titles;
    QList<int> counts;
};

#endif // SERIES_CAROUSEL_VIEW_H
