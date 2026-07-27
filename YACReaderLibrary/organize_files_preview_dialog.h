#ifndef ORGANIZE_FILES_PREVIEW_DIALOG_H
#define ORGANIZE_FILES_PREVIEW_DIALOG_H

#include <QDialog>
#include <QList>
#include <QString>

class QAction;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

class OrganizeFilesPreviewDialog : public QDialog
{
    Q_OBJECT
public:
    struct Move {
        QString source;
        QString destination;
    };

    OrganizeFilesPreviewDialog(const QString &baseRoot,
                               const QString &libraryRoot,
                               const QList<Move> &moves,
                               QWidget *parent = nullptr);

    QList<Move> moves() const;

private slots:
    void removeSelectedItems();
    void updateActionsState();

private:
    QString baseRoot;
    QString libraryRoot;
    QTreeWidget *tree;
    QAction *removeAction;
    QPushButton *removeButton;
    QPushButton *okButton;

    void setupUI(const QList<Move> &moves);
    void buildTree(const QList<Move> &moves);
    QString relativePathForItem(QTreeWidgetItem *item) const;
    bool isFileItem(QTreeWidgetItem *item) const;
    void pruneEmptyAncestors(QTreeWidgetItem *item);

    static constexpr int SourceRole = Qt::UserRole + 1;
};

#endif // ORGANIZE_FILES_PREVIEW_DIALOG_H
