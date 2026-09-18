#include "filename_tagger_dialog.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QThread>
#include <QVBoxLayout>

using namespace YACReader;

namespace {

constexpr auto kPreviewRows = 20;

}

FilenameTaggerDialog::FilenameTaggerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Read tags from file names"));
    doLayout();
}

FilenameTaggerDialog::~FilenameTaggerDialog()
{
    teardownWorker();
}

void FilenameTaggerDialog::doLayout()
{
    headline = new QLabel;
    headline->setWordWrap(true);

    previewTable = new QTableWidget(0, 4);
    previewTable->setHorizontalHeaderLabels({ tr("File name"), tr("Title"), tr("Artist"), tr("Magazine") });
    previewTable->horizontalHeader()->setStretchLastSection(true);
    previewTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    previewTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    previewTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    previewTable->verticalHeader()->setVisible(false);
    previewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    previewTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    untitledOnlyCheck = new QCheckBox(tr("Only comics that have no title yet"));
    untitledOnlyCheck->setChecked(true);
    untitledOnlyCheck->setToolTip(tr("A comic that already has a title has been tagged by something else, and going over it again gains nothing."));

    overwriteCheck = new QCheckBox(tr("Replace tags that are already there"));
    overwriteCheck->setToolTip(tr("Off by default, so anything you have tagged by hand is left as it is."));

    progressBar = new QProgressBar;
    progressBar->setRange(0, 100);
    progressBar->setValue(0);

    currentLabel = new QLabel;
    currentLabel->setWordWrap(true);

    startButton = new QPushButton(tr("Start"));
    stopButton = new QPushButton(tr("Stop"));
    stopButton->setEnabled(false);
    closeButton = new QPushButton(tr("Close"));

    connect(untitledOnlyCheck, &QCheckBox::toggled, this, &FilenameTaggerDialog::refreshPreview);
    connect(startButton, &QAbstractButton::clicked, this, &FilenameTaggerDialog::start);
    connect(stopButton, &QAbstractButton::clicked, this, &FilenameTaggerDialog::stop);
    connect(closeButton, &QAbstractButton::clicked, this, &QDialog::accept);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(startButton);
    buttons->addWidget(stopButton);
    buttons->addWidget(closeButton);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(headline);
    layout->addWidget(previewTable);
    layout->addWidget(untitledOnlyCheck);
    layout->addWidget(overwriteCheck);
    layout->addWidget(progressBar);
    layout->addWidget(currentLabel);
    layout->addLayout(buttons);
}

QSize FilenameTaggerDialog::sizeHint() const
{
    return QSize(900, 560);
}

void FilenameTaggerDialog::setLibrary(const QString &databasePath)
{
    this->databasePath = databasePath;
    refreshPreview();
}

void FilenameTaggerDialog::refreshPreview()
{
    previewTable->setRowCount(0);

    if (databasePath.isEmpty()) {
        return;
    }

    const auto onlyUntitled = untitledOnlyCheck->isChecked();
    const auto total = FilenameTagger::countComics(databasePath, onlyUntitled);
    const auto rows = FilenameTagger::preview(databasePath, onlyUntitled, kPreviewRows);

    for (const auto &row : rows) {
        const auto index = previewTable->rowCount();
        previewTable->insertRow(index);
        previewTable->setItem(index, 0, new QTableWidgetItem(row.fileName));
        previewTable->setItem(index, 1, new QTableWidgetItem(row.title));
        previewTable->setItem(index, 2, new QTableWidgetItem(row.artist));
        previewTable->setItem(index, 3, new QTableWidgetItem(row.magazine));
    }
    previewTable->resizeColumnToContents(0);

    if (total == 0) {
        headline->setText(tr("Nothing to do: every comic here already has a title. Untick the box below to go over them again anyway."));
    } else {
        headline->setText(tr("%n comic(s) will have their tags read out of their file names. No tags are downloaded and nothing goes online - the names are the source. Here is what it would do to the first few:", "", total));
    }

    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    currentLabel->clear();
    startButton->setEnabled(total > 0);
}

void FilenameTaggerDialog::start()
{
    if (databasePath.isEmpty() || workerThread != nullptr) {
        return;
    }

    tagger = new FilenameTagger(databasePath);
    tagger->setOnlyUntitled(untitledOnlyCheck->isChecked());
    tagger->setOverwriteExisting(overwriteCheck->isChecked());

    workerThread = new QThread;
    tagger->moveToThread(workerThread);

    connect(workerThread, &QThread::started, tagger, &FilenameTagger::run);
    connect(tagger, &FilenameTagger::progress, this, &FilenameTaggerDialog::onProgress);
    connect(tagger, &FilenameTagger::finished, this, &FilenameTaggerDialog::onFinished);

    startButton->setEnabled(false);
    stopButton->setEnabled(true);
    closeButton->setEnabled(false);
    untitledOnlyCheck->setEnabled(false);
    overwriteCheck->setEnabled(false);

    workerThread->start();
}

void FilenameTaggerDialog::stop()
{
    if (tagger != nullptr) {
        tagger->cancel();
        stopButton->setEnabled(false);
        currentLabel->setText(tr("Stopping after the folder in flight..."));
    }
}

void FilenameTaggerDialog::onProgress(int done, int total, const QString &currentName)
{
    progressBar->setRange(0, qMax(1, total));
    progressBar->setValue(done);
    if (!currentName.isEmpty()) {
        currentLabel->setText(tr("%1 of %2 - %3").arg(done).arg(total).arg(currentName));
    }
}

void FilenameTaggerDialog::onFinished(int comicsTagged, int foldersVisited)
{
    teardownWorker();

    startButton->setEnabled(true);
    stopButton->setEnabled(false);
    closeButton->setEnabled(true);
    untitledOnlyCheck->setEnabled(true);
    overwriteCheck->setEnabled(true);

    currentLabel->setText(tr("Done. %n comic(s) tagged", "", comicsTagged) + tr(", across %n folder(s).", "", foldersVisited));

    refreshPreview();
}

void FilenameTaggerDialog::teardownWorker()
{
    if (workerThread == nullptr) {
        return;
    }

    if (tagger != nullptr) {
        tagger->cancel();
    }

    workerThread->quit();
    workerThread->wait();

    delete tagger;
    tagger = nullptr;

    delete workerThread;
    workerThread = nullptr;
}

void FilenameTaggerDialog::closeEvent(QCloseEvent *event)
{
    teardownWorker();
    QDialog::closeEvent(event);
}
