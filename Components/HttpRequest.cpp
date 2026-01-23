#include "HttpRequest.h"
#include <QFile>
#include <QTextStream>

HttpRequest::HttpRequest(QObject* parent) : QObject(parent) {
    manager = new QNetworkAccessManager(this);
    connect(manager, &QNetworkAccessManager::finished, this, &HttpRequest::onFinished);
}

void HttpRequest::sendRequest() {
    qDebug() << "sendRequest";
    QUrl url("https://www.baidu.com"); // 固定网址
    QNetworkRequest request(url);
    manager->get(request); // 发送GET请求
}

void HttpRequest::onFinished(QNetworkReply* reply) {
    if (reply->error() == QNetworkReply::NoError) {
        // 获取网页数据并输出
        QString response = reply->readAll();
        //qDebug() << "Response:" << response;

        // 创建并写入文件
        QFile file("response.html");
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << response;  // 将网页内容写入文件
            file.close();  // 关闭文件
            qDebug() << "File saved as response.html";
        } else {
            qDebug() << "Error: Unable to open file for writing.";
        }

    } else {
        // 输出错误信息
        qDebug() << "Error:" << reply->errorString();
    }
    reply->deleteLater();
}
