#ifndef __HTTP_WORKER_H
#define __HTTP_WORKER_H

#include <QByteArray>
#include <QThread>
#include <QUrl>

class HttpWorker : public QThread
{
    Q_OBJECT
public:
    HttpWorker(const QString &urlString, const QString &userAgent);
public slots:
    void get();
    QByteArray getResult();
    bool wasValid();
    bool wasTimeout();
    int statusCode();
    QString errorString();

private:
    // Comic Vine regularly takes longer than a couple of seconds to answer, and a
    // scrape of a large library used to give up on every request that was merely slow.
    static constexpr int kTimeoutMs = 20000;
    static constexpr int kMaxAttempts = 3;
    static constexpr int kRetryBaseDelayMs = 1000;

    static bool isWorthRetrying(int networkError, int httpStatus);

    void run();
    QUrl url;
    QString userAgent;
    int httpGetId;
    QByteArray result;
    bool _error;
    bool _timeout;
    int _statusCode;
    QString _errorString;
signals:
    void dataReady(const QByteArray &);
    void timeout();
};

#endif
