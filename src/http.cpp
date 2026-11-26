#include "http.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrlQuery>

QNetworkAccessManager *Http::manager() {
    static QNetworkAccessManager *instance = [] {
        auto *m = new QNetworkAccessManager(QCoreApplication::instance());
        m->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
        m->setTransferTimeout(30000);
        return m;
    }();
    return instance;
}

QByteArray Http::userAgent() {
    return "lode/" + QCoreApplication::applicationVersion().toUtf8() + " (Minecraft launcher)";
}

static QNetworkRequest request(const QUrl &url, const Http::Headers &headers) {
    QNetworkRequest r(url);
    r.setHeader(QNetworkRequest::UserAgentHeader, Http::userAgent());
    r.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
    for (const auto &[name, value] : headers)
        r.setRawHeader(name, value);
    return r;
}

QNetworkReply *Http::get(const QUrl &url, const Headers &headers) {
    return manager()->get(request(url, headers));
}

QString Http::describe(QNetworkReply *reply, const QByteArray &body) {
    const QJsonObject o = QJsonDocument::fromJson(body).object();
    for (const char *key : {"error_description", "errorMessage", "description", "message", "error"})
        if (o.value(key).isString() && !o.value(key).toString().isEmpty())
            return o.value(key).toString();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status >= 400)
        return QCoreApplication::translate("Http", "The server answered %1 for %2")
            .arg(status).arg(reply->url().host());
    return reply->errorString();
}

static void finishData(QObject *context, QNetworkReply *reply, Http::DataCallback done) {
    QPointer<QObject> guard(context);
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, guard, done] {
        reply->deleteLater();
        if (!guard)
            return;
        const QByteArray body = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool ok = reply->error() == QNetworkReply::NoError;
        done(ok, body, ok ? QString() : Http::describe(reply, body), status);
    });
}

static void finishJson(QObject *context, QNetworkReply *reply, Http::JsonCallback done) {
    finishData(context, reply, [done](bool ok, const QByteArray &body, const QString &error, int status) {
        QJsonParseError parse{};
        const QJsonDocument json = QJsonDocument::fromJson(body, &parse);
        if (ok && parse.error != QJsonParseError::NoError && !body.isEmpty())
            return done(false, {}, QCoreApplication::translate("Http", "The server sent something that isn't JSON"), status);
        done(ok, json, error, status);
    });
}

void Http::getJson(QObject *context, const QUrl &url, JsonCallback done, const Headers &headers) {
    finishJson(context, get(url, headers), done);
}

void Http::getData(QObject *context, const QUrl &url, DataCallback done, const Headers &headers) {
    finishData(context, get(url, headers), done);
}

void Http::postJson(QObject *context, const QUrl &url, const QJsonDocument &body, JsonCallback done,
                    const Headers &headers) {
    QNetworkRequest r = request(url, headers);
    r.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    r.setRawHeader("Accept", "application/json");
    finishJson(context, manager()->post(r, body.toJson(QJsonDocument::Compact)), done);
}

void Http::postForm(QObject *context, const QUrl &url, const QList<QPair<QString, QString>> &fields,
                    JsonCallback done) {
    QUrlQuery query;
    for (const auto &[key, value] : fields)
        query.addQueryItem(key, QString::fromUtf8(QUrl::toPercentEncoding(value)));
    QNetworkRequest r = request(url, {});
    r.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    finishJson(context, manager()->post(r, query.toString(QUrl::FullyEncoded).toUtf8()), done);
}
