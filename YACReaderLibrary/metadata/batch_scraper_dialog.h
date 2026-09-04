#ifndef BATCH_SCRAPER_DIALOG_H
#define BATCH_SCRAPER_DIALOG_H

#include "batch_scraper.h"

#include <QDialog>
#include <QList>

class QCheckBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QThread;

// Runs a scrape over many series at once and, when it is done, collects everything it
// refused to decide into a single table the user works through in one sitting.
//
// The split is the point. A scrape that stops to ask a question every few series cannot be
// left alone, which is the whole complaint about scraping a large library. Here the
// questions are saved up.
namespace YACReader {

class BatchScraperDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BatchScraperDialog(QWidget *parent = nullptr);
    ~BatchScraperDialog() override;

    void setLibrary(const QString &databasePath);
    void setTargets(const QList<ScrapeTarget> &targets);

    QSize sizeHint() const override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void start();
    void stop();
    void onProgress(int done, int total, const QString &currentName);
    void onFolderFinished(const YACReader::ScrapeOutcome &outcome);
    void onWaiting(int seconds, const QString &reason);
    void onFinished(int applied, int needsReview, int notFound, int failed);

private:
    void doLayout();
    void showReview();
    void appendResultRow(const ScrapeOutcome &outcome);
    void teardownWorker();

    QString databasePath;
    QList<ScrapeTarget> targets;
    QList<ScrapeOutcome> unresolved;

    QStackedWidget *pages = nullptr;

    // running page
    QLabel *headline = nullptr;
    QLabel *currentLabel = nullptr;
    QProgressBar *progressBar = nullptr;
    QTableWidget *resultsTable = nullptr;
    QCheckBox *overwriteCheck = nullptr;
    QPushButton *startButton = nullptr;
    QPushButton *stopButton = nullptr;
    QPushButton *closeButton = nullptr;

    // review page
    QLabel *reviewHeadline = nullptr;
    QPushButton *reviewCloseButton = nullptr;

    BatchScraper *scraper = nullptr;
    QThread *workerThread = nullptr;
};

}

#endif // BATCH_SCRAPER_DIALOG_H
