#include "batch_scraper_dialog.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMetaType>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QThread>
#include <QVBoxLayout>

#include <utility>

using namespace YACReader;

namespace {

QString describe(const ScrapeOutcome &outcome)
{
    switch (outcome.result) {
    case ScrapeOutcome::Applied:
        return QObject::tr("%n comic(s) tagged", "", outcome.comicsUpdated);
    case ScrapeOutcome::NeedsReview:
        return QObject::tr("needs a choice");
    case ScrapeOutcome::NotFound:
        return QObject::tr("no match");
    case ScrapeOutcome::Failed:
        return QObject::tr("failed: %1").arg(outcome.message);
    case ScrapeOutcome::Cancelled:
        return QObject::tr("stopped");
    }
    return { };
}

// What the matched series was, in one line, so a row in the results table says which
// series was written and not merely that something was.
QString matchedTitle(const ScrapeOutcome &outcome)
{
    if (outcome.result != ScrapeOutcome::Applied || outcome.candidates.isEmpty()) {
        return { };
    }
    return outcome.candidates.first().series.title;
}

QString candidateLabel(const SeriesMatch &match)
{
    auto label = match.series.title;
    if (!match.series.romajiTitle.isEmpty() && match.series.romajiTitle != match.series.title) {
        label += QStringLiteral(" / ") + match.series.romajiTitle;
    }
    if (match.series.startYear > 0) {
        label += QStringLiteral(" (") + QString::number(match.series.startYear) + QStringLiteral(")");
    }
    if (match.series.volumes > 0) {
        label += QObject::tr(" - %n volume(s)", "", match.series.volumes);
    }
    return label;
}

}

BatchScraperDialog::BatchScraperDialog(QWidget *parent)
    : QDialog(parent)
{
    // The worker reports outcomes across a thread boundary, so the type has to be known
    // to the metaobject system before the first queued signal carries one.
    qRegisterMetaType<YACReader::ScrapeOutcome>("YACReader::ScrapeOutcome");

    setWindowTitle(tr("Download tags"));
    doLayout();
}

BatchScraperDialog::~BatchScraperDialog()
{
    teardownWorker();
}

void BatchScraperDialog::doLayout()
{
    pages = new QStackedWidget;

    // ---- running page
    auto *runningPage = new QWidget;
    auto *runningLayout = new QVBoxLayout(runningPage);

    headline = new QLabel;
    headline->setWordWrap(true);

    overwriteCheck = new QCheckBox(tr("Replace tags that are already there"));
    overwriteCheck->setToolTip(tr("Off by default, so anything you have tagged by hand is left as it is."));

    progressBar = new QProgressBar;
    progressBar->setRange(0, 100);
    progressBar->setValue(0);

    currentLabel = new QLabel;
    currentLabel->setWordWrap(true);

    resultsTable = new QTableWidget(0, 3);
    resultsTable->setHorizontalHeaderLabels({ tr("Series"), tr("Matched"), tr("Result") });
    resultsTable->horizontalHeader()->setStretchLastSection(true);
    resultsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    resultsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    resultsTable->verticalHeader()->setVisible(false);
    resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    startButton = new QPushButton(tr("Start"));
    stopButton = new QPushButton(tr("Stop"));
    stopButton->setEnabled(false);
    closeButton = new QPushButton(tr("Close"));

    connect(startButton, &QAbstractButton::clicked, this, &BatchScraperDialog::start);
    connect(stopButton, &QAbstractButton::clicked, this, &BatchScraperDialog::stop);
    connect(closeButton, &QAbstractButton::clicked, this, &QDialog::accept);

    auto *runningButtons = new QHBoxLayout;
    runningButtons->addStretch();
    runningButtons->addWidget(startButton);
    runningButtons->addWidget(stopButton);
    runningButtons->addWidget(closeButton);

    runningLayout->addWidget(headline);
    runningLayout->addWidget(overwriteCheck);
    runningLayout->addWidget(progressBar);
    runningLayout->addWidget(currentLabel);
    runningLayout->addWidget(resultsTable);
    runningLayout->addLayout(runningButtons);

    // ---- review page
    auto *reviewPage = new QWidget;
    auto *reviewLayout = new QVBoxLayout(reviewPage);

    reviewHeadline = new QLabel;
    reviewHeadline->setWordWrap(true);

    reviewTable = new QTableWidget(0, 2);
    reviewTable->setHorizontalHeaderLabels({ tr("Folder"), tr("Series") });
    reviewTable->horizontalHeader()->setStretchLastSection(true);
    reviewTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    reviewTable->verticalHeader()->setVisible(false);
    reviewTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    applyButton = new QPushButton(tr("Apply choices"));
    reviewCloseButton = new QPushButton(tr("Close"));

    connect(applyButton, &QAbstractButton::clicked, this, &BatchScraperDialog::applyReviewChoices);
    connect(reviewCloseButton, &QAbstractButton::clicked, this, &QDialog::accept);

    auto *reviewButtons = new QHBoxLayout;
    reviewButtons->addStretch();
    reviewButtons->addWidget(applyButton);
    reviewButtons->addWidget(reviewCloseButton);

    reviewLayout->addWidget(reviewHeadline);
    reviewLayout->addWidget(reviewTable);
    reviewLayout->addLayout(reviewButtons);

    pages->addWidget(runningPage);
    pages->addWidget(reviewPage);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(pages);
}

QSize BatchScraperDialog::sizeHint() const
{
    return QSize(760, 520);
}

void BatchScraperDialog::setLibrary(const QString &databasePath)
{
    this->databasePath = databasePath;
}

void BatchScraperDialog::setTargets(const QList<ScrapeTarget> &targets)
{
    this->targets = targets;
    unresolved.clear();
    resultsTable->setRowCount(0);
    reviewTable->setRowCount(0);
    pages->setCurrentIndex(0);

    progressBar->setRange(0, qMax(1, static_cast<int>(targets.size())));
    progressBar->setValue(0);
    currentLabel->clear();

    headline->setText(tr("%n series will be looked up on AniList. Anything that cannot be matched with confidence is set aside for you to settle at the end, so this can be left running.", "", static_cast<int>(targets.size())));

    startButton->setEnabled(!targets.isEmpty());
    stopButton->setEnabled(false);
}

void BatchScraperDialog::start()
{
    if (targets.isEmpty() || workerThread != nullptr) {
        return;
    }

    unresolved.clear();
    resultsTable->setRowCount(0);

    scraper = new BatchScraper(databasePath);
    scraper->setTargets(targets);
    scraper->setOverwriteExisting(overwriteCheck->isChecked());

    workerThread = new QThread;
    scraper->moveToThread(workerThread);

    connect(workerThread, &QThread::started, scraper, &BatchScraper::run);
    connect(scraper, &BatchScraper::progress, this, &BatchScraperDialog::onProgress);
    connect(scraper, &BatchScraper::folderFinished, this, &BatchScraperDialog::onFolderFinished);
    connect(scraper, &BatchScraper::waiting, this, &BatchScraperDialog::onWaiting);
    connect(scraper, &BatchScraper::finished, this, &BatchScraperDialog::onFinished);

    startButton->setEnabled(false);
    stopButton->setEnabled(true);
    closeButton->setEnabled(false);
    overwriteCheck->setEnabled(false);

    workerThread->start();
}

void BatchScraperDialog::stop()
{
    if (scraper != nullptr) {
        scraper->cancel();
        stopButton->setEnabled(false);
        currentLabel->setText(tr("Stopping after the series in flight..."));
    }
}

void BatchScraperDialog::onProgress(int done, int total, const QString &currentName)
{
    progressBar->setRange(0, qMax(1, total));
    progressBar->setValue(done);
    currentLabel->setText(tr("%1 of %2 - %3").arg(done).arg(total).arg(currentName));
}

void BatchScraperDialog::appendResultRow(const ScrapeOutcome &outcome)
{
    const auto row = resultsTable->rowCount();
    resultsTable->insertRow(row);
    resultsTable->setItem(row, 0, new QTableWidgetItem(outcome.target.folderName));
    resultsTable->setItem(row, 1, new QTableWidgetItem(matchedTitle(outcome)));
    resultsTable->setItem(row, 2, new QTableWidgetItem(describe(outcome)));
    resultsTable->scrollToBottom();
}

void BatchScraperDialog::onFolderFinished(const YACReader::ScrapeOutcome &outcome)
{
    appendResultRow(outcome);

    // Anything not settled goes on the pile for the review pass. A failure is included on
    // purpose: a series that could not be reached is still a series with no tags, and
    // hiding it would leave the user believing the run covered everything.
    if (outcome.result == ScrapeOutcome::NeedsReview || outcome.result == ScrapeOutcome::NotFound || outcome.result == ScrapeOutcome::Failed) {
        unresolved.append(outcome);
    }
}

void BatchScraperDialog::onWaiting(int seconds, const QString &reason)
{
    currentLabel->setText(tr("Pausing for %1s - %2").arg(seconds).arg(reason));
}

void BatchScraperDialog::onFinished(int applied, int needsReview, int notFound, int failed)
{
    teardownWorker();

    startButton->setEnabled(true);
    stopButton->setEnabled(false);
    closeButton->setEnabled(true);
    overwriteCheck->setEnabled(true);

    currentLabel->setText(tr("Done. %1 tagged, %2 need a choice, %3 not found, %4 failed.").arg(applied).arg(needsReview).arg(notFound).arg(failed));

    if (!unresolved.isEmpty()) {
        showReview();
    }
}

void BatchScraperDialog::showReview()
{
    reviewTable->setRowCount(0);

    for (const auto &outcome : std::as_const(unresolved)) {
        const auto row = reviewTable->rowCount();
        reviewTable->insertRow(row);

        auto *nameItem = new QTableWidgetItem(outcome.target.folderName);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        reviewTable->setItem(row, 0, nameItem);

        auto *chooser = new QComboBox;
        // Leaving it alone is the default, so working down the list and only touching the
        // ones you recognise does nothing to the rest.
        chooser->addItem(tr("Leave alone"), -1);
        for (auto index = 0; index < outcome.candidates.size(); ++index) {
            chooser->addItem(candidateLabel(outcome.candidates.at(index)), index);
        }
        if (outcome.candidates.isEmpty()) {
            chooser->setEnabled(false);
            chooser->setItemText(0, tr("Nothing found - rename the folder and try again"));
        }
        reviewTable->setCellWidget(row, 1, chooser);
    }

    reviewHeadline->setText(tr("%n series could not be matched on their own. Pick the right one where you recognise it and leave the rest alone.", "", static_cast<int>(unresolved.size())));
    reviewTable->resizeRowsToContents();
    pages->setCurrentIndex(1);
}

void BatchScraperDialog::applyReviewChoices()
{
    if (databasePath.isEmpty()) {
        return;
    }

    BatchScraper applier(databasePath);
    applier.setOverwriteExisting(overwriteCheck->isChecked());

    auto applied = 0;
    for (auto row = 0; row < reviewTable->rowCount() && row < unresolved.size(); ++row) {
        auto *chooser = qobject_cast<QComboBox *>(reviewTable->cellWidget(row, 1));
        if (chooser == nullptr) {
            continue;
        }

        const auto choice = chooser->currentData().toInt();
        if (choice < 0) {
            continue;
        }

        const auto &outcome = unresolved.at(row);
        if (choice >= outcome.candidates.size()) {
            continue;
        }

        const auto result = applier.applyToFolder(outcome.target, outcome.candidates.at(choice).series);
        if (result.result == ScrapeOutcome::Applied) {
            applied++;
            chooser->setEnabled(false);
        }
    }

    reviewHeadline->setText(tr("%n series tagged. Anything still listed was left alone.", "", applied));
}

void BatchScraperDialog::teardownWorker()
{
    if (workerThread == nullptr) {
        return;
    }

    if (scraper != nullptr) {
        scraper->cancel();
    }

    workerThread->quit();
    workerThread->wait();

    delete scraper;
    scraper = nullptr;

    delete workerThread;
    workerThread = nullptr;
}

void BatchScraperDialog::closeEvent(QCloseEvent *event)
{
    teardownWorker();
    QDialog::closeEvent(event);
}
