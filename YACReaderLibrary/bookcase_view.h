#ifndef BOOKCASE_VIEW_H
#define BOOKCASE_VIEW_H

#include "themable.h"

#include <QColor>
#include <QList>
#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QQuickWidget>
#include <QStringList>
#include <QUrl>

class FolderModel;

// The library as a wall of shelves curving away around you, books standing spine out.
//
// The scene is QML rather than a 3D engine, so there is no new Qt module and no shaders:
// the curve is per-column geometry, which is enough because a cylinder seen from its axis
// only does two things to what is on it - squeezes it horizontally as the surface turns
// away, and splays the shelf lines apart towards the edges.
//
// This class is deliberately thin. It holds the series list and answers questions about
// it one index at a time, so the scene can build only the columns it can actually see;
// binding the model directly would mean instantiating nineteen hundred books to show
// ninety of them.
class BookcaseView : public QQuickWidget, protected Themable
{
    Q_OBJECT

public:
    explicit BookcaseView(QWidget *parent = nullptr);

    void setFolderModel(FolderModel *model, const QModelIndex &parentIndex = QModelIndex());
    void reload();

    Q_INVOKABLE int seriesCount() const;
    Q_INVOKABLE QString titleAt(int index) const;
    Q_INVOKABLE QUrl coverAt(int index) const;
    Q_INVOKABLE int volumesAt(int index) const;
    // A stable colour for a series' spine. There is no spine artwork anywhere in a comic
    // library - only covers - so the band of colour that makes a shelf look like a shelf
    // has to be invented, and inventing it from the title means it never changes.
    Q_INVOKABLE QColor spineColorAt(int index) const;
    Q_INVOKABLE void openSeries(int index);

signals:
    void folderSelected(const QModelIndex &sourceIndex);
    // The scene rebuilds itself on this rather than being poked by name through the
    // metaobject, which is both type checked and one less string to get wrong.
    void seriesChanged();

protected:
    void applyTheme(const Theme &theme) override;

private:
    FolderModel *folderModel = nullptr;
    QPersistentModelIndex parentFolder;

    QList<QPersistentModelIndex> series;
    QStringList titles;
    QList<QUrl> covers;
    QList<int> counts;
};

#endif // BOOKCASE_VIEW_H
