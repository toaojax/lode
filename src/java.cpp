#include "java.h"

#include "downloads.h"
#include "http.h"
#include "mojang.h"
#include "paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>

static const char *kRuntimesUrl =
    "https://launchermeta.mojang.com/v1/products/java-runtime/2ec0cc96c44e5a76b9c8b7c39df7210883d12871/all.json";

JavaRuntimes::JavaRuntimes(QObject *parent) : QObject(parent) {}

QString JavaRuntimes::javaPath(const QString &component) { return Paths::java() + "/" + component + "/bin/java"; }

static QString stampPath(const QString &component) { return Paths::java() + "/" + component + "/.lode-installed"; }

bool JavaRuntimes::installed(const QString &component) {
    return QFileInfo(javaPath(component)).isExecutable() && QFileInfo::exists(stampPath(component));
}

static QString platformKey() {
    return Mojang::archName() == "arm64" ? "linux-arm64" : Mojang::archName() == "x86" ? "linux-i386" : "linux";
}

void JavaRuntimes::ensure(const QString &component, Callback done, Progress progress) {
    if (installed(component))
        return done(true, javaPath(component), {});
    if (progress)
        progress(0, tr("Looking up Java"));
    Http::getJson(this, QUrl(kRuntimesUrl), [=](bool ok, const QJsonDocument &json, const QString &error, int) {
        if (!ok)
            return done(false, {}, tr("Couldn't look up Java runtimes: %1").arg(error));
        const QJsonArray builds = json.object().value(platformKey()).toObject().value(component).toArray();
        if (builds.isEmpty())
            return done(false, {}, tr("Mojang doesn't publish %1 for this computer").arg(component));
        const QJsonObject manifest = builds.first().toObject().value("manifest").toObject();
        install(component, QUrl(manifest.value("url").toString()), manifest.value("sha1").toString(), done, progress);
    });
}

void JavaRuntimes::install(const QString &component, const QUrl &manifestUrl, const QString &manifestSha1,
                           Callback done, Progress progress) {
    Http::getJson(this, manifestUrl, [=](bool ok, const QJsonDocument &json, const QString &error, int) {
        if (!ok)
            return done(false, {}, tr("Couldn't fetch %1: %2").arg(component, error));
        const QString root = Paths::java() + "/" + component;
        const QJsonObject files = json.object().value("files").toObject();
        auto *batch = new DownloadBatch(this);
        QList<QPair<QString, QString>> links;
        for (auto it = files.begin(); it != files.end(); ++it) {
            const QJsonObject file = it.value().toObject();
            const QString path = root + "/" + it.key();
            // A runtime path that climbs out of its folder is not something to write.
            if (it.key().contains("..")) continue;
            const QString type = file.value("type").toString();
            if (type == "directory") {
                QDir().mkpath(path);
            } else if (type == "link") {
                links.append({path, file.value("target").toString()});
            } else if (type == "file") {
                const QJsonObject raw = file.value("downloads").toObject().value("raw").toObject();
                DownloadBatch::Item item;
                item.url = QUrl(raw.value("url").toString());
                item.path = path;
                item.sha1 = raw.value("sha1").toString();
                item.size = raw.value("size").toInteger(-1);
                item.executable = file.value("executable").toBool();
                batch->add(item);
            }
        }
        connect(batch, &DownloadBatch::progress, this, [=](qint64 doneBytes, qint64 total, int, int) {
            if (progress && total > 0)
                progress(double(doneBytes) / total, tr("Downloading Java (%1)").arg(component));
        });
        connect(batch, &DownloadBatch::finished, this, [=](bool ok, const QString &error) {
            batch->deleteLater();
            if (!ok)
                return done(false, {}, error);
            for (const auto &[path, target] : links) {
                QFile::remove(path);
                QDir().mkpath(QFileInfo(path).absolutePath());
                QFile::link(target, path);
            }
            if (!QFileInfo(javaPath(component)).isExecutable())
                return done(false, {}, tr("%1 installed without a java binary").arg(component));
            Paths::writeAtomically(stampPath(component), manifestSha1.toUtf8());
            done(true, javaPath(component), {});
        });
        batch->start();
    });
}
