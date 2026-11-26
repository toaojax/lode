#pragma once

#include <QJsonDocument>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPair>
#include <QUrl>
#include <functional>

// One network manager for the whole app, with HTTP/2 and a polite user agent.
// Every call takes a context object, so a reply that lands after its owner is
// gone is simply dropped.
namespace Http {
using Headers = QList<QPair<QByteArray, QByteArray>>;
using JsonCallback = std::function<void(bool ok, const QJsonDocument &json, const QString &error, int status)>;
using DataCallback = std::function<void(bool ok, const QByteArray &data, const QString &error, int status)>;

QNetworkAccessManager *manager();
QByteArray userAgent();

QNetworkReply *get(const QUrl &url, const Headers &headers = {});
void getJson(QObject *context, const QUrl &url, JsonCallback done, const Headers &headers = {});
void getData(QObject *context, const QUrl &url, DataCallback done, const Headers &headers = {});
void postJson(QObject *context, const QUrl &url, const QJsonDocument &body, JsonCallback done,
              const Headers &headers = {});
void postForm(QObject *context, const QUrl &url, const QList<QPair<QString, QString>> &fields, JsonCallback done);

// The best message for a failed reply: the server's own text if it sent one.
QString describe(QNetworkReply *reply, const QByteArray &body);
}
