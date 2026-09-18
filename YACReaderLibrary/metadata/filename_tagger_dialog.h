#ifndef FILENAME_TAGGER_DIALOG_H
#define FILENAME_TAGGER_DIALOG_H

#include "filename_tagger.h"

#include <QDialog>
#include <QString>

class QCheckBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QTableWidget;
class QThread;

// Reading the tags out of the file names, with a look at what that will do first.
//
// The whole dialog is arranged around one question: a bulk write over eighteen thousand
// comics is not something to agree to from a description of it, so the first thing shown is
// twenty real rows from this library - the file name on the left and what it becomes on the
// right. The count and the buttons come after that, because by then the answer is obvious.
class FilenameTaggerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FilenameTaggerDialog(QWidget *parent = nullptr);
    ~FilenameTaggerDialog() override;

    void setLibrary(const QString &databasePath);

    QSize sizeHint() const override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void refreshPreview();
    void start();
    void stop();
    void onProgress(int done, int total, const QString &currentName);
    void onFinished(int comicsTagged, int foldersVisited);

private:
    void doLayout();
    void teardownWorker();

    QString databasePath;

    QLabel *headline = nullptr;
    QTableWidget *previewTable = nullptr;
    QCheckBox *untitledOnlyCheck = nullptr;
    QCheckBox *overwriteCheck = nullptr;
    QProgressBar *progressBar = nullptr;
    QLabel *currentLabel = nullptr;
    QPushButton *startButton = nullptr;
    QPushButton *stopButton = nullptr;
    QPushButton *closeButton = nullptr;

    YACReader::FilenameTagger *tagger = nullptr;
    QThread *workerThread = nullptr;
};

#endif // FILENAME_TAGGER_DIALOG_H
