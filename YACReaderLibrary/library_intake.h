#ifndef LIBRARY_INTAKE_H
#define LIBRARY_INTAKE_H

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

// Watches the top of a library folder and files whatever is dropped there.
//
// The library is one folder per series and nothing else at the top level, which is what
// makes this possible: anything appearing at the top that is not already a series is
// something new that has just arrived. A loose volume goes into the series it belongs to; a
// whole folder of a series that is not here yet keeps its own place under a cleaned up name.
//
// The rule for everything else is that it is left alone somewhere obvious rather than
// guessed at. The matcher in this application was confidently wrong about four hundred and
// seventy two series in one afternoon, and a wrong guess here does not write a wrong
// synopsis - it moves somebody's files somewhere they will not think to look. So a drop is
// only filed when the name matches exactly, once, and everything else goes to a folder
// called _Needs a look with a line in the log saying why.
class LibraryIntake : public QObject
{
    Q_OBJECT

public:
    explicit LibraryIntake(QObject *parent = nullptr);

    // The name of the folder ambiguous arrivals are put in, and the log written beside it.
    static QString quarantineFolderName();

    void watch(const QString &libraryPath, const QString &databasePath);
    void stop();

signals:
    // Something was actually filed, so the library wants rescanning. Carries the number of
    // items moved, which is what the window has to tell the user.
    void imported(int filed, int setAside);

private slots:
    void onDirectoryChanged();
    void settle();

private:
    struct Arrival {
        QString path;
        bool isFolder = false;
    };

    QStringList seriesFolderNames() const;
    QList<Arrival> arrivalsAtTop() const;
    bool hasSettled(const Arrival &arrival);
    void process();

    bool fileInto(const QString &sourceFile, const QString &seriesFolder);
    bool setAside(const QString &path, const QString &reason);
    void note(const QString &line);

    QFileSystemWatcher watcher;
    // Two timers, because a drop is not one event. Copying a folder of forty volumes fires
    // the watcher continuously for as long as it takes, and acting on the first of those
    // would file a half written file.
    QTimer debounce;
    QTimer settleTimer;

    QString libraryPath;
    QString databasePath;

    // Sizes seen last time round, so a file still being written can be told from one that
    // has finished arriving.
    QHash<QString, qint64> lastSeenSize;
};

#endif // LIBRARY_INTAKE_H
