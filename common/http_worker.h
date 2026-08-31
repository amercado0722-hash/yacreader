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
    // A GraphQL endpoint answers one POSTed query rather than a URL per question, which
    // is what lets a metadata lookup ask for a search and its full result in one request.
    void post(const QByteArray &body, const QString &contentType = QStringLiteral("application/json"));
    QByteArray getResult();
    bool wasValid();
    bool wasTimeout();
    int statusCode();
    QString errorString();
    // Seconds a rate limited server asked us to wait, from its Retry-After header, or 0
    // when it did not say. A scraper walking a whole library has to honour this or it
    // spends the rest of the run being refused.
    int retryAfterSeconds();

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
    QByteArray _body;
    QString _contentType;
    bool _isPost = false;
    int _retryAfterSeconds = 0;
signals:
    void dataReady(const QByteArray &);
    void timeout();
};

#endif
