#include "series_review_dialog.h"

#include "anilist_client.h"
#include "data_base_management.h"
#include "series_name_utils.h"

#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPalette>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QThread>
#include <QVBoxLayout>

namespace YACReader {

// Searching on the user's behalf, off the interface thread. AniListClient blocks by design -
// the scraper it was written for is already on a worker - so it needs one here too, or typing
// a name into the box freezes the window for the length of a network round trip.
class SeriesSearchWorker : public QObject
{
    Q_OBJECT

public slots:
    void search(const QString &wanted)
    {
        AniListClient client;
        const auto response = client.searchSeries(wanted, 8);

        if (response.error) {
            emit results(wanted, { }, response.errorString.isEmpty() ? tr("the search could not be completed") : response.errorString);
            return;
        }

        emit results(wanted, rankSeriesMatches(wanted, response.candidates), QString());
    }

signals:
    void results(const QString &wanted, const QList<YACReader::SeriesMatch> &matches, const QString &error);
};

namespace {

// One line of the facts that separate two series with the same name. Deliberately the ones a
// reader can check against the book in front of them: when it ran, how long it is, and how
// many people have it on a list - which is what tells the famous series from the parody.
QString candidateFacts(const SeriesMetadata &series)
{
    QStringList parts;

    if (!series.format.isEmpty()) {
        parts.append(series.format);
    }
    if (series.startYear > 0) {
        parts.append(QString::number(series.startYear));
    }
    if (!series.status.isEmpty()) {
        parts.append(series.status.toLower());
    }
    if (series.volumes > 0) {
        parts.append(SeriesReviewDialog::tr("%n volume(s)", "", series.volumes));
    } else if (series.chapters > 0) {
        parts.append(SeriesReviewDialog::tr("%n chapter(s)", "", series.chapters));
    }
    if (series.popularity > 0) {
        parts.append(SeriesReviewDialog::tr("%L1 readers").arg(series.popularity));
    }

    return parts.join(QStringLiteral("  ·  "));
}

QString otherTitles(const SeriesMetadata &series)
{
    QStringList others;
    for (const auto &title : series.primaryTitles()) {
        if (title != series.title) {
            others.append(title);
        }
    }
    for (const auto &synonym : series.synonyms) {
        if (others.size() >= 3) {
            break;
        }
        if (synonym != series.title && !others.contains(synonym)) {
            others.append(synonym);
        }
    }
    return others.join(QStringLiteral("  ·  "));
}

QString excerpt(const QString &synopsis, int limit = 300)
{
    auto text = synopsis.simplified();
    if (text.size() <= limit) {
        return text;
    }

    // Cut at a word rather than mid-syllable; an excerpt that ends in the middle of a name is
    // harder to read than a shorter one.
    auto cut = text.left(limit).lastIndexOf(QLatin1Char(' '));
    if (cut < limit / 2) {
        cut = limit;
    }
    return text.left(cut) + QStringLiteral("…");
}

}

SeriesReviewDialog::SeriesReviewDialog(const QString &databasePath, QWidget *parent)
    : QDialog(parent), databasePath(databasePath)
{
    setWindowTitle(tr("Identify series"));

    network = new QNetworkAccessManager(this);

    workerThread = new QThread(this);
    worker = new SeriesSearchWorker;
    worker->moveToThread(workerThread);
    connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(worker, &SeriesSearchWorker::results, this, &SeriesReviewDialog::onSearchResults);
    workerThread->start();

    doLayout();
}

SeriesReviewDialog::~SeriesReviewDialog()
{
    if (workerThread != nullptr) {
        workerThread->quit();
        workerThread->wait();
    }
}

void SeriesReviewDialog::doLayout()
{
    auto *outer = new QVBoxLayout(this);

    progressLabel = new QLabel;
    progressBar = new QProgressBar;
    progressBar->setTextVisible(false);
    progressBar->setFixedHeight(4);

    auto *top = new QVBoxLayout;
    top->addWidget(progressLabel);
    top->addWidget(progressBar);
    outer->addLayout(top);

    // The folder, on the left, is the evidence. The file names matter more than the folder
    // name does: a folder called "[Danke-Empire] SNK v01-34" is unreadable and the volume
    // inside it is called "Shingeki no Kyojin v01".
    auto *left = new QVBoxLayout;

    folderLabel = new QLabel;
    folderLabel->setWordWrap(true);
    auto folderFont = folderLabel->font();
    folderFont.setBold(true);
    folderFont.setPointSize(folderFont.pointSize() + 2);
    folderLabel->setFont(folderFont);

    volumesLabel = new QLabel;
    filesLabel = new QLabel;
    filesLabel->setWordWrap(true);
    filesLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    searchEdit = new QLineEdit;
    searchEdit->setPlaceholderText(tr("Search for a different name"));
    searchButton = new QPushButton(tr("Find"));
    connect(searchEdit, &QLineEdit::returnPressed, this, &SeriesReviewDialog::research);
    connect(searchButton, &QPushButton::clicked, this, &SeriesReviewDialog::research);

    auto *searchRow = new QHBoxLayout;
    searchRow->addWidget(searchEdit);
    searchRow->addWidget(searchButton);

    left->addWidget(folderLabel);
    left->addWidget(volumesLabel);
    left->addSpacing(8);
    left->addWidget(new QLabel(tr("Volumes in this folder")));
    left->addWidget(filesLabel, 1);
    left->addSpacing(8);
    left->addLayout(searchRow);

    auto *leftHost = new QWidget;
    leftHost->setLayout(left);
    leftHost->setFixedWidth(320);

    // The candidates, on the right.
    candidateHost = new QWidget;
    candidateLayout = new QVBoxLayout(candidateHost);
    candidateLayout->setAlignment(Qt::AlignTop);

    candidateArea = new QScrollArea;
    candidateArea->setWidgetResizable(true);
    candidateArea->setWidget(candidateHost);
    candidateArea->setFrameShape(QFrame::NoFrame);

    auto *middle = new QHBoxLayout;
    middle->addWidget(leftHost);
    middle->addWidget(candidateArea, 1);
    outer->addLayout(middle, 1);

    statusLabel = new QLabel;
    outer->addWidget(statusLabel);

    backButton = new QPushButton(tr("Back"));
    skipButton = new QPushButton(tr("Skip"));
    useButton = new QPushButton(tr("Use this series"));
    useButton->setDefault(true);
    doneButton = new QPushButton(tr("Finish"));

    connect(backButton, &QPushButton::clicked, this, &SeriesReviewDialog::goBack);
    connect(skipButton, &QPushButton::clicked, this, &SeriesReviewDialog::skip);
    connect(useButton, &QPushButton::clicked, this, &SeriesReviewDialog::useSelected);
    connect(doneButton, &QPushButton::clicked, this, &QDialog::accept);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(backButton);
    buttons->addWidget(skipButton);
    buttons->addStretch(1);
    buttons->addWidget(doneButton);
    buttons->addWidget(useButton);
    outer->addLayout(buttons);
}

QSize SeriesReviewDialog::sizeHint() const
{
    return { 1020, 680 };
}

void SeriesReviewDialog::setOutcomes(const QList<ScrapeOutcome> &outcomes)
{
    this->outcomes = outcomes;
    current = 0;
    identified = 0;
    showCurrent();
}

void SeriesReviewDialog::setOverwriteExisting(bool overwrite)
{
    overwriteExisting = overwrite;
}

int SeriesReviewDialog::identifiedCount() const
{
    return identified;
}

// The volume files, read straight from the library. This is the part that makes a hopeless
// folder name answerable: the folder may be named after the release group, but the files
// inside it are usually named after the series.
QStringList SeriesReviewDialog::volumeFileNames(qulonglong folderId) const
{
    QStringList names;
    if (databasePath.isEmpty()) {
        return names;
    }

    QString connectionName;
    {
        auto db = DataBaseManagement::loadDatabase(databasePath);
        if (!db.open()) {
            return names;
        }
        connectionName = db.connectionName();

        QSqlQuery query(db);
        query.prepare("select fileName from comic where parentId = :parentId order by fileName limit 8");
        query.bindValue(":parentId", folderId);
        query.exec();
        while (query.next()) {
            names.append(query.value(0).toString());
        }
    }
    QSqlDatabase::removeDatabase(connectionName);

    return names;
}

void SeriesReviewDialog::clearCandidates()
{
    for (auto *card : std::as_const(candidateCards)) {
        card->deleteLater();
    }
    candidateCards.clear();
    selected = -1;
}

void SeriesReviewDialog::showCurrent()
{
    clearCandidates();

    if (current >= outcomes.size()) {
        progressLabel->setText(tr("All done - %n series identified.", "", identified));
        progressBar->setRange(0, 1);
        progressBar->setValue(1);
        folderLabel->setText(QString());
        volumesLabel->setText(QString());
        filesLabel->setText(QString());
        statusLabel->setText(QString());
        backButton->setEnabled(current > 0);
        skipButton->setEnabled(false);
        useButton->setEnabled(false);
        searchEdit->setEnabled(false);
        searchButton->setEnabled(false);
        return;
    }

    const auto &outcome = outcomes.at(current);

    progressLabel->setText(tr("Series %1 of %2  ·  %n identified so far", "", identified).arg(current + 1).arg(outcomes.size()));
    progressBar->setRange(0, outcomes.size());
    progressBar->setValue(current);

    folderLabel->setText(outcome.target.folderName);

    const auto files = volumeFileNames(outcome.target.folderId);
    volumesLabel->setText(tr("in the library as \"%1\"").arg(outcome.target.searchName));
    filesLabel->setText(files.isEmpty() ? tr("(none listed)") : files.join(QStringLiteral("\n")));

    searchEdit->setEnabled(true);
    searchButton->setEnabled(true);
    searchEdit->setText(outcome.target.searchName);
    backButton->setEnabled(current > 0);
    skipButton->setEnabled(true);

    shown = outcome.candidates;
    for (auto index = 0; index < shown.size(); ++index) {
        addCandidateCard(index, shown.at(index));
    }

    if (shown.isEmpty()) {
        statusLabel->setText(tr("Nothing was found for this name. Try one of the volume file names in the search box."));
        useButton->setEnabled(false);
    } else {
        statusLabel->setText(tr("Pick the series this folder is, or skip it."));
        useButton->setEnabled(false);
    }
}

// One candidate, as a card that can be clicked. A radio button in a row of names would fit
// more on screen and answer nothing; the cover is what makes this a two second decision.
void SeriesReviewDialog::addCandidateCard(int index, const SeriesMatch &match)
{
    auto *card = new QFrame(candidateHost);
    card->setFrameShape(QFrame::StyledPanel);
    card->setCursor(Qt::PointingHandCursor);

    auto *grid = new QGridLayout(card);

    auto *cover = new QLabel;
    cover->setFixedSize(90, 128);
    cover->setAlignment(Qt::AlignCenter);
    cover->setText(tr("no\ncover"));
    grid->addWidget(cover, 0, 0, 5, 1);

    auto *title = new QLabel(match.series.title);
    title->setWordWrap(true);
    auto titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    title->setFont(titleFont);
    grid->addWidget(title, 0, 1);

    const auto others = otherTitles(match.series);
    if (!others.isEmpty()) {
        auto *alt = new QLabel(others);
        alt->setWordWrap(true);
        grid->addWidget(alt, 1, 1);
    }

    auto *facts = new QLabel(candidateFacts(match.series));
    facts->setWordWrap(true);
    grid->addWidget(facts, 2, 1);

    if (!match.series.genres.isEmpty()) {
        auto *genres = new QLabel(match.series.genres.join(QStringLiteral(", ")));
        genres->setWordWrap(true);
        grid->addWidget(genres, 3, 1);
    }

    if (!match.series.synopsis.isEmpty()) {
        auto *synopsis = new QLabel(excerpt(match.series.synopsis));
        synopsis->setWordWrap(true);
        grid->addWidget(synopsis, 4, 1);
    }

    grid->setColumnStretch(1, 1);

    card->installEventFilter(this);
    card->setProperty("candidateIndex", index);

    candidateLayout->addWidget(card);
    candidateCards.append(card);

    if (!match.series.coverUrl.isEmpty()) {
        requestCover(match.series.coverUrl, cover);
    }
}

bool SeriesReviewDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        const auto index = watched->property("candidateIndex");
        if (index.isValid()) {
            selectCandidate(index.toInt());
            return true;
        }
    }

    // A double click is the same choice made impatiently, and taking it as "yes, that one"
    // saves a trip to the button four hundred times.
    if (event->type() == QEvent::MouseButtonDblClick) {
        const auto index = watched->property("candidateIndex");
        if (index.isValid()) {
            selectCandidate(index.toInt());
            useSelected();
            return true;
        }
    }

    return QDialog::eventFilter(watched, event);
}

void SeriesReviewDialog::selectCandidate(int index)
{
    if (index < 0 || index >= candidateCards.size()) {
        return;
    }

    selected = index;

    for (auto position = 0; position < candidateCards.size(); ++position) {
        auto *card = qobject_cast<QFrame *>(candidateCards.at(position));
        if (card == nullptr) {
            continue;
        }
        // The palette rather than a hard coded colour, because this application is themed
        // and a blue box that stays blue on a dark theme is worse than no highlight.
        card->setFrameShadow(position == selected ? QFrame::Raised : QFrame::Plain);
        card->setLineWidth(position == selected ? 2 : 1);
        card->setBackgroundRole(position == selected ? QPalette::AlternateBase : QPalette::Base);
        card->setAutoFillBackground(position == selected);
    }

    useButton->setEnabled(true);
}

void SeriesReviewDialog::requestCover(const QString &url, QLabel *target)
{
    if (coverCache.contains(url)) {
        target->setPixmap(coverCache.value(url));
        return;
    }

    QNetworkRequest request { QUrl(url) };
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *reply = network->get(request);
    reply->setProperty("coverUrl", url);
    pendingCovers.insert(reply, target);
    connect(reply, &QNetworkReply::finished, this, &SeriesReviewDialog::onCoverDownloaded);
}

void SeriesReviewDialog::onCoverDownloaded()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply == nullptr) {
        return;
    }

    auto *target = pendingCovers.take(reply);
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        return;
    }

    QPixmap pixmap;
    if (!pixmap.loadFromData(reply->readAll())) {
        return;
    }

    const auto scaled = pixmap.scaled(90, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    coverCache.insert(reply->property("coverUrl").toString(), scaled);

    // The card may have been thrown away while the picture was in the air.
    if (target != nullptr) {
        target->setPixmap(scaled);
    }
}

void SeriesReviewDialog::research()
{
    const auto wanted = searchEdit->text().trimmed();
    if (wanted.isEmpty()) {
        return;
    }

    searchButton->setEnabled(false);
    statusLabel->setText(tr("Searching for \"%1\"…").arg(wanted));
    QMetaObject::invokeMethod(worker, "search", Qt::QueuedConnection, Q_ARG(QString, wanted));
}

void SeriesReviewDialog::onSearchResults(const QString &wanted, const QList<SeriesMatch> &matches, const QString &error)
{
    searchButton->setEnabled(true);

    if (!error.isEmpty()) {
        statusLabel->setText(tr("Could not search for \"%1\": %2").arg(wanted, error));
        return;
    }

    clearCandidates();
    shown = matches;
    for (auto index = 0; index < shown.size(); ++index) {
        addCandidateCard(index, shown.at(index));
    }

    useButton->setEnabled(false);
    statusLabel->setText(shown.isEmpty()
                                 ? tr("Nothing found for \"%1\".").arg(wanted)
                                 : tr("%n result(s) for \"%1\". Pick the right one, or search again.", "", static_cast<int>(shown.size())).arg(wanted));
}

// Written to the library now rather than collected for a final Apply. Four hundred of these
// is not one sitting, and work that is only saved at the end is work that gets lost.
void SeriesReviewDialog::useSelected()
{
    if (selected < 0 || selected >= shown.size() || current >= outcomes.size()) {
        return;
    }

    BatchScraper applier(databasePath);
    applier.setOverwriteExisting(overwriteExisting);
    const auto result = applier.applyToFolder(outcomes.at(current).target, shown.at(selected).series);

    if (result.result == ScrapeOutcome::Applied) {
        identified++;
    } else {
        statusLabel->setText(tr("Could not write that to the library: %1").arg(result.message));
        return;
    }

    current++;
    showCurrent();
}

void SeriesReviewDialog::skip()
{
    current++;
    showCurrent();
}

void SeriesReviewDialog::goBack()
{
    if (current > 0) {
        current--;
        showCurrent();
    }
}

}

#include "series_review_dialog.moc"
