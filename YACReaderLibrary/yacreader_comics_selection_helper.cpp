#include "yacreader_comics_selection_helper.h"

#include "comic_model.h"

YACReaderComicsSelectionHelper::YACReaderComicsSelectionHelper(QObject *parent)
    : QObject(parent)
{
}

void YACReaderComicsSelectionHelper::setModel(ComicModel *model)
{
    if (model == nullptr)
        return;

    this->model = model;

    delete itemSelectionModel;

    itemSelectionModel = new QItemSelectionModel(model, this);
    connect(itemSelectionModel, &QItemSelectionModel::selectionChanged, this, [this]() {
        ++revision;
        emit selectionChanged();
    });

    ++revision;
    emit selectionChanged();
}

void YACReaderComicsSelectionHelper::selectIndex(int index)
{
    if (itemSelectionModel != nullptr && model != nullptr && index >= 0 && index < model->rowCount())
        itemSelectionModel->select(model->index(index, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
}

void YACReaderComicsSelectionHelper::selectOnly(int index)
{
    if (itemSelectionModel != nullptr && model != nullptr && index >= 0 && index < model->rowCount())
        itemSelectionModel->select(model->index(index, 0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void YACReaderComicsSelectionHelper::deselectIndex(int index)
{
    if (itemSelectionModel != nullptr && model != nullptr && index >= 0 && index < model->rowCount())
        itemSelectionModel->select(model->index(index, 0), QItemSelectionModel::Deselect | QItemSelectionModel::Rows);
}

bool YACReaderComicsSelectionHelper::isSelectedIndex(int index) const
{
    if (itemSelectionModel != nullptr && model != nullptr) {
        QModelIndex mi = model->index(index, 0);
        return itemSelectionModel->isSelected(mi);
    }
    return false;
}

void YACReaderComicsSelectionHelper::clear()
{
    if (itemSelectionModel != nullptr)
        itemSelectionModel->clear();
}

QModelIndex YACReaderComicsSelectionHelper::currentIndex()
{
    if (!itemSelectionModel)
        return QModelIndex();

    QModelIndexList indexes = itemSelectionModel->selectedRows();
    if (indexes.length() > 0)
        return indexes[0];

    return QModelIndex();
}

void YACReaderComicsSelectionHelper::selectAll()
{
    if (!itemSelectionModel || !model || model->rowCount() == 0)
        return;

    QModelIndex top = model->index(0, 0);
    QModelIndex bottom = model->index(model->rowCount() - 1, 0);
    QItemSelection selection(top, bottom);
    itemSelectionModel->select(selection, QItemSelectionModel::Select | QItemSelectionModel::Rows);
}

QModelIndexList YACReaderComicsSelectionHelper::selectedRows(int column) const
{
    return itemSelectionModel ? itemSelectionModel->selectedRows(column) : QModelIndexList();
}

QList<QModelIndex> YACReaderComicsSelectionHelper::selectedIndexes() const
{
    return itemSelectionModel ? itemSelectionModel->selectedIndexes() : QModelIndexList();
}

int YACReaderComicsSelectionHelper::numItemsSelected() const
{
    if (itemSelectionModel != nullptr) {
        return itemSelectionModel->selectedRows().length();
    }

    return 0;
}

int YACReaderComicsSelectionHelper::lastSelectedIndex() const
{
    if (itemSelectionModel != nullptr) {
        const auto selectedRows = itemSelectionModel->selectedRows();
        return selectedRows.isEmpty() ? -1 : selectedRows.last().row();
    }

    return -1;
}

QItemSelectionModel *YACReaderComicsSelectionHelper::selectionModel()
{
    return itemSelectionModel;
}

qulonglong YACReaderComicsSelectionHelper::selectionRevision() const
{
    return revision;
}
