#ifndef FILENAME_TAGGER_H
#define FILENAME_TAGGER_H

#include <QList>
#include <QObject>
#include <QString>

#include <atomic>

namespace YACReader {

// One comic's file name and what it would become, for the preview.
struct FilenameTagPreview {
    QString fileName;
    QString title;
    QString artist;
    QString magazine;
};

// Fills a library's tags from the file names it already has.
//
// This is the answer for a collection no metadata provider knows: seventeen thousand
// one-shots by seventeen hundred artists, where looking each one up returns nothing and
// would take a day doing it. The names were written to a convention, so the convention is
// the source.
//
// It writes only what a name actually says - artist, title, magazine, issue, publisher -
// and never a synopsis, because it has none and inventing one would be worse than the empty
// field. Existing tags are left alone unless told otherwise, on the same grounds as the
// online scraper: a bulk run nobody is watching must not overwrite work done by hand.
class FilenameTagger : public QObject
{
    Q_OBJECT

public:
    explicit FilenameTagger(const QString &databasePath, QObject *parent = nullptr);

    // How many comics a run would look at, and a handful of worked examples. Shown before
    // anything is written, because "what will this do to my library" is the only question
    // worth answering first and a count on its own does not answer it.
    static int countComics(const QString &databasePath, bool onlyUntitled);
    static QList<FilenameTagPreview> preview(const QString &databasePath, bool onlyUntitled, int limit);

    // Off by default. On, it rewrites tags that are already there.
    void setOverwriteExisting(bool overwrite);
    // On by default: a comic that already has a title has been tagged by something, and
    // going over it again is work for no gain.
    void setOnlyUntitled(bool onlyUntitled);

public slots:
    // Blocking. Meant to be run on a worker thread.
    void run();
    void cancel();

signals:
    void progress(int done, int total, const QString &currentName);
    void finished(int comicsTagged, int foldersVisited);

private:
    QString databasePath;
    bool overwriteExisting = false;
    bool onlyUntitled = true;
    std::atomic<bool> cancelled { false };
};

}

#endif // FILENAME_TAGGER_H
