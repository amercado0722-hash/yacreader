#ifndef SERIES_REVIEW_DIALOG_H
#define SERIES_REVIEW_DIALOG_H

#include "batch_scraper.h"

#include <QDialog>
#include <QHash>
#include <QList>
#include <QString>

class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QProgressBar;
class QPushButton;
class QScrollArea;
class QThread;
class QVBoxLayout;

namespace YACReader {

class SeriesSearchWorker;

// Working through the series the scrape would not guess at, one at a time.
//
// The table this replaces asked the question in the wrong shape. A row per series with a
// drop-down of candidate names is quick to build and impossible to answer: the name is the
// one thing already known to be ambiguous, which is precisely why the series is in this list.
// Against a real library it left a quarter of it - four hundred and ninety one series -
// unidentified, because nobody can settle four hundred and ninety one questions from a
// drop-down of names they have already been told are inconclusive.
//
// So this shows one series and everything there is to know about it: the folder, the volume
// files inside it, and each candidate with its cover, its other titles, when it ran, how long
// it is and how many people read it. Deciding takes a few seconds and is nearly always
// obvious once the cover is on screen. Where the folder name is hopeless there is a search
// box, because the reader can read the cover of volume one and the matcher cannot.
//
// A choice is written to the library the moment it is made rather than collected for the end,
// so four hundred of these can be done over a week in whatever order suits, and stopping is
// free.
class SeriesReviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SeriesReviewDialog(const QString &databasePath, QWidget *parent = nullptr);
    ~SeriesReviewDialog() override;

    void setOutcomes(const QList<ScrapeOutcome> &outcomes);
    void setOverwriteExisting(bool overwrite);

    int identifiedCount() const;

    QSize sizeHint() const override;

protected:
    // Selection is a click anywhere on a candidate card. Putting a radio button inside
    // something that already looks clickable just adds a small target to miss.
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void showCurrent();
    void useSelected();
    void skip();
    void goBack();
    void research();
    void onSearchResults(const QString &wanted, const QList<YACReader::SeriesMatch> &matches, const QString &error);
    void onCoverDownloaded();

private:
    void doLayout();
    void clearCandidates();
    void selectCandidate(int index);
    void addCandidateCard(int index, const SeriesMatch &match);
    QStringList volumeFileNames(qulonglong folderId) const;
    void requestCover(const QString &url, QLabel *target);

    QString databasePath;
    bool overwriteExisting = false;

    QList<ScrapeOutcome> outcomes;
    int current = 0;
    int identified = 0;
    int selected = -1;
    // What is on screen now: the outcome's own candidates, or whatever the search box last
    // turned up for this folder.
    QList<SeriesMatch> shown;

    QLabel *progressLabel = nullptr;
    QProgressBar *progressBar = nullptr;
    QLabel *folderLabel = nullptr;
    QLabel *volumesLabel = nullptr;
    QLabel *filesLabel = nullptr;
    QLineEdit *searchEdit = nullptr;
    QPushButton *searchButton = nullptr;
    QLabel *statusLabel = nullptr;

    QScrollArea *candidateArea = nullptr;
    QWidget *candidateHost = nullptr;
    QVBoxLayout *candidateLayout = nullptr;
    QList<QWidget *> candidateCards;

    QPushButton *backButton = nullptr;
    QPushButton *skipButton = nullptr;
    QPushButton *useButton = nullptr;
    QPushButton *doneButton = nullptr;

    QNetworkAccessManager *network = nullptr;
    // Covers are small and often repeat between candidates; a library this size is reviewed
    // over several sittings and re-fetching the same picture each time is rude to a service
    // that is answering for free.
    QHash<QString, QPixmap> coverCache;
    QHash<QObject *, QLabel *> pendingCovers;

    SeriesSearchWorker *worker = nullptr;
    QThread *workerThread = nullptr;
};

}

#endif // SERIES_REVIEW_DIALOG_H
