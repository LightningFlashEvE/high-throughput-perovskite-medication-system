#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>

class HttpRequest : public QObject
{
    Q_OBJECT
public:
    explicit HttpRequest(QObject* parent = nullptr);

    void sendRequest();

private slots:
    void onFinished(QNetworkReply* reply);

private:
    QNetworkAccessManager* manager;
};

#endif // HTTPREQUEST_H
