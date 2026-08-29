#include "http_worker.h"

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QtGlobal>

#define PREVIOUS_VERSION "6.0.0"

HttpWorker::HttpWorker(const QString &urlString, const QString &userAgent)
    : QThread(), url(urlString), userAgent(userAgent), _error(false), _timeout(false), _statusCode(0)
{
}

void HttpWorker::get()
{
    this->start();
}

QByteArray HttpWorker::getResult()
{
    return result;
}

bool HttpWorker::wasValid()
{
    return !_error;
}

bool HttpWorker::wasTimeout()
{
    return _timeout;
}

int HttpWorker::statusCode()
{
    return _statusCode;
}

QString HttpWorker::errorString()
{
    return _errorString;
}

bool HttpWorker::isWorthRetrying(int networkError, int httpStatus)
{
    // Server side hiccups and dropped connections usually succeed on a second try.
    // A rate limit (429, and Comic Vine's 420) will not clear within seconds, and any
    // other 4xx is an answer rather than a failure, so neither is retried.
    if (httpStatus >= 500 && httpStatus <= 599) {
        return true;
    }

    if (httpStatus != 0) {
        return false;
    }

    switch (networkError) {
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::RemoteHostClosedError:
    case QNetworkReply::TimeoutError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::ProxyConnectionRefusedError:
    case QNetworkReply::ProxyConnectionClosedError:
    case QNetworkReply::ProxyTimeoutError:
    case QNetworkReply::InternalServerError:
    case QNetworkReply::ServiceUnavailableError:
    case QNetworkReply::UnknownNetworkError:
    case QNetworkReply::UnknownServerError:
        return true;
    default:
        return false;
    }
}

void HttpWorker::run()
{
    for (int attempt = 1; attempt <= kMaxAttempts; ++attempt) {
        QNetworkAccessManager manager;
        QEventLoop q;
        QTimer tT;

        tT.setSingleShot(true);
        connect(&tT, &QTimer::timeout, &q, &QEventLoop::quit);
        connect(&manager, &QNetworkAccessManager::finished, &q, &QEventLoop::quit);

        auto request = QNetworkRequest(url);

        request.setHeader(QNetworkRequest::UserAgentHeader,
                          userAgent);

        QNetworkReply *reply = manager.get(request);

        tT.start(kTimeoutMs);
        q.exec();

        if (!tT.isActive()) {
            // the timer won the race, so nothing came back in time
            reply->abort();
            _timeout = true;
            _error = true;
            _statusCode = 0;
            _errorString = QStringLiteral("Timeout");

            if (attempt < kMaxAttempts) {
                QThread::msleep(kRetryBaseDelayMs * attempt);
                continue;
            }

            emit timeout();
            return;
        }

        tT.stop();

        _timeout = false;
        _error = !(reply->error() == QNetworkReply::NoError);
        _statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        _errorString = _error ? reply->errorString() : QString();
        result = reply->readAll();

        if (_error && attempt < kMaxAttempts && isWorthRetrying(reply->error(), _statusCode)) {
            QThread::msleep(kRetryBaseDelayMs * attempt);
            continue;
        }

        emit dataReady(result);
        return;
    }
}
