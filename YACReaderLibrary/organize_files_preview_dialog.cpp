#include "organize_files_preview_dialog.h"

#include "organize_files_dialog.h"

#include <QAction>
#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QHash>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
// Only the "New location" column (0) may be edited; the source column is
// informational and must stay read-only.
class FirstColumnEditableDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (index.column() != 0)
            return nullptr;
        return QStyledItemDelegate::createEditor(parent, option, index);
    }
};
}

OrganizeFilesPreviewDialog::OrganizeFilesPreviewDialog(const QString &baseRoot,
                                                       const QString &libraryRoot,
                                                       const QList<Move> &moves,
                                                       QWidget *parent)
    : QDialog(parent), baseRoot(QDir::cleanPath(baseRoot)), libraryRoot(QDir::cleanPath(libraryRoot))
{
    setupUI(moves);
}

void OrganizeFilesPreviewDialog::setupUI(const QList<Move> &moves)
{
    auto description = new QLabel(tr("%n file(s) will be moved as shown below. Double-click an item in the "
                                     "\"New location\" column to rename a folder or file, or remove items to leave "
                                     "them where they are, before applying the changes.",
                                     "", moves.size()));
    description->setWordWrap(true);

    tree = new QTreeWidget;
    tree->setColumnCount(2);
    tree->setHeaderLabels({ tr("New location"), tr("Current location") });
    tree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    tree->setItemDelegate(new FirstColumnEditableDelegate(tree));
    tree->setUniformRowHeights(true);
    tree->setAlternatingRowColors(true);
    tree->setSelectionMode(QAbstractItemView::ExtendedSelection);

    removeAction = new QAction(tr("Remove from list"), this);
    removeAction->setShortcut(QKeySequence::Delete);
    removeAction->setShortcutContext(Qt::WidgetShortcut);
    connect(removeAction, &QAction::triggered, this, &OrganizeFilesPreviewDialog::removeSelectedItems);
    tree->addAction(removeAction);
    tree->setContextMenuPolicy(Qt::ActionsContextMenu);
    connect(tree, &QTreeWidget::itemSelectionChanged, this, &OrganizeFilesPreviewDialog::updateActionsState);

    buildTree(moves);

    tree->expandAll();
    tree->resizeColumnToContents(0);
    tree->header()->setStretchLastSection(true);

    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    okButton = buttonBox->button(QDialogButtonBox::Ok);
    okButton->setText(tr("Move files"));
    removeButton = buttonBox->addButton(tr("Remove selected"), QDialogButtonBox::ActionRole);
    connect(removeButton, &QPushButton::clicked, this, &OrganizeFilesPreviewDialog::removeSelectedItems);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(description);
    mainLayout->addWidget(tree);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
    setModal(true);
    setWindowTitle(tr("Organize files"));
    resize(680, 520);

    updateActionsState();
}

void OrganizeFilesPreviewDialog::buildTree(const QList<Move> &moves)
{
    const QDir base(baseRoot);
    const QIcon folderIcon = qApp->style()->standardIcon(QStyle::SP_DirIcon);
    const QIcon fileIcon = qApp->style()->standardIcon(QStyle::SP_FileIcon);

    // Sort moves by destination so the tree is built in a stable, readable order.
    QList<Move> sortedMoves = moves;
    std::sort(sortedMoves.begin(), sortedMoves.end(), [&base](const Move &a, const Move &b) {
        return base.relativeFilePath(a.destination).compare(base.relativeFilePath(b.destination), Qt::CaseInsensitive) < 0;
    });

    // Maps a cumulative relative directory path to its folder item.
    QHash<QString, QTreeWidgetItem *> folders;

    for (const Move &move : sortedMoves) {
        const QString relative = base.relativeFilePath(move.destination);
        const QStringList segments = relative.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        if (segments.isEmpty())
            continue;

        QTreeWidgetItem *parent = nullptr;
        QString cumulative;
        // Build/reuse the folder nodes for every segment except the last (the file).
        for (int i = 0; i < segments.size() - 1; ++i) {
            cumulative += (cumulative.isEmpty() ? QString() : QStringLiteral("/")) + segments.at(i);
            QTreeWidgetItem *&folderItem = folders[cumulative];
            if (folderItem == nullptr) {
                folderItem = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree);
                folderItem->setText(0, segments.at(i));
                folderItem->setIcon(0, folderIcon);
                folderItem->setFlags(folderItem->flags() | Qt::ItemIsEditable);
            }
            parent = folderItem;
        }

        QTreeWidgetItem *fileItem = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree);
        fileItem->setText(0, segments.last());
        fileItem->setIcon(0, fileIcon);
        fileItem->setFlags(fileItem->flags() | Qt::ItemIsEditable);
        fileItem->setData(0, SourceRole, move.source);

        const QString sourceRelative = libraryRoot.isEmpty() ? move.source : QDir(libraryRoot).relativeFilePath(move.source);
        fileItem->setText(1, sourceRelative);
        fileItem->setToolTip(1, move.source);
    }
}

bool OrganizeFilesPreviewDialog::isFileItem(QTreeWidgetItem *item) const
{
    return item != nullptr && item->data(0, SourceRole).isValid();
}

void OrganizeFilesPreviewDialog::pruneEmptyAncestors(QTreeWidgetItem *item)
{
    // Delete folder nodes that no longer hold any files, walking up the tree.
    while (item != nullptr && item->childCount() == 0 && !isFileItem(item)) {
        QTreeWidgetItem *parent = item->parent();
        delete item;
        item = parent;
    }
}

void OrganizeFilesPreviewDialog::removeSelectedItems()
{
    const QList<QTreeWidgetItem *> selected = tree->selectedItems();
    if (selected.isEmpty())
        return;

    const QSet<QTreeWidgetItem *> selectedSet(selected.begin(), selected.end());

    // Only delete the top-most selected items; children of an already-selected
    // item would be deleted along with their parent.
    QList<QTreeWidgetItem *> toDelete;
    QList<QTreeWidgetItem *> parents;
    for (QTreeWidgetItem *item : selected) {
        bool ancestorSelected = false;
        for (QTreeWidgetItem *ancestor = item->parent(); ancestor != nullptr; ancestor = ancestor->parent()) {
            if (selectedSet.contains(ancestor)) {
                ancestorSelected = true;
                break;
            }
        }
        if (!ancestorSelected) {
            toDelete.append(item);
            parents.append(item->parent());
        }
    }

    for (QTreeWidgetItem *item : toDelete)
        delete item;

    for (QTreeWidgetItem *parent : parents)
        pruneEmptyAncestors(parent);

    updateActionsState();
}

void OrganizeFilesPreviewDialog::updateActionsState()
{
    const bool hasSelection = !tree->selectedItems().isEmpty();
    removeAction->setEnabled(hasSelection);
    if (removeButton != nullptr)
        removeButton->setEnabled(hasSelection);

    bool hasFiles = false;
    QTreeWidgetItemIterator it(tree);
    while (*it) {
        if (isFileItem(*it)) {
            hasFiles = true;
            break;
        }
        ++it;
    }
    if (okButton != nullptr)
        okButton->setEnabled(hasFiles);
}

QString OrganizeFilesPreviewDialog::relativePathForItem(QTreeWidgetItem *item) const
{
    QStringList segments;
    for (QTreeWidgetItem *node = item; node != nullptr; node = node->parent()) {
        const QString clean = OrganizeFilesDialog::sanitizeSegment(node->text(0));
        if (!clean.isEmpty())
            segments.prepend(clean);
    }
    return segments.join(QLatin1Char('/'));
}

QList<OrganizeFilesPreviewDialog::Move> OrganizeFilesPreviewDialog::moves() const
{
    QList<Move> result;

    QTreeWidgetItemIterator it(tree);
    while (*it) {
        QTreeWidgetItem *item = *it;
        ++it;

        // Leaves (files) carry the source path.
        if (item->childCount() != 0)
            continue;
        const QVariant sourceData = item->data(0, SourceRole);
        if (!sourceData.isValid())
            continue;

        const QString relative = relativePathForItem(item);
        if (relative.isEmpty())
            continue;

        Move move;
        move.source = sourceData.toString();
        move.destination = QDir::cleanPath(baseRoot + QLatin1Char('/') + relative);
        result.append(move);
    }

    return result;
}
