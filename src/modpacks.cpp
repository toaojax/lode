#include "modpacks.h"

#include "catalog.h"
#include "content.h"
#include "downloads.h"
#include "http.h"
#include "instances.h"
#include "paths.h"
#include "settings.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QUrl>
#include <QtConcurrent>

static const char *kCurse = "https://api.curseforge.com/v1";

Modpacks::Modpacks(Settings *settings, Instances *instances, QObject *parent)
    : QObject(parent), m_settings(settings), m_instances(instances) {}

void Modpacks::setState(bool busy, const QString &status, double progress) {
    m_busy = busy;
    m_status = status;
    m_progress = progress;
    emit changed();
}

bool Modpacks::safeRelative(const QString &path) {
    if (path.isEmpty() || path.startsWith('/') || path.contains('\\') || path.contains(':'))
        return false;
    for (const QString &part : path.split('/'))
        if (part == ".." || part.isEmpty())
            return false;
    return true;
}

QPair<QString, QString> Modpacks::curseLoader(const QString &id, const QString &mc) {
    const QString name = id.section('-', 0, 0).toLower(), version = id.section('-', 1);
    if (name == "forge") return {"forge", mc + "-" + version};
    if (name == "neoforge") return {"neoforge", version};
    if (name == "fabric") return {"fabric", version};
    if (name == "quilt") return {"quilt", version};
    return {"vanilla", {}};
}

static QStringList archiveEntries(const QString &archive) {
    QProcess p;
    p.start("bsdtar", {"-tf", archive});
    p.waitForFinished(30000);
    return QString::fromUtf8(p.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
}

static QByteArray archiveMember(const QString &archive, const QString &member) {
    QProcess p;
    p.start("bsdtar", {"-xOf", archive, member});
    p.waitForFinished(30000);
    return p.exitCode() == 0 ? p.readAllStandardOutput() : QByteArray();
}

void Modpacks::installVersion(const QVariantMap &version, const QVariantMap &project) {
    if (m_busy) return;
    const QString name = project.value("title").toString();
    if (!version.value("downloadable").toBool())
        return emit failed(name, tr("This pack's author only allows downloading it from CurseForge's website."), {});
    const QString file = version.value("file").toString();
    if (!safeRelative(file))
        return emit failed(name, tr("The pack has an odd file name"), {});
    const QString archive = Paths::cache() + "/packs/" + file;
    auto *batch = new DownloadBatch(this);
    batch->add(QUrl(version.value("url").toString()), archive, version.value("sha1").toString());
    setState(true, tr("Downloading %1").arg(name), 0);
    connect(batch, &DownloadBatch::progress, this, [=](qint64 done, qint64 total, int, int) {
        if (total > 0) setState(true, tr("Downloading %1").arg(name), double(done) / total);
    });
    connect(batch, &DownloadBatch::finished, this, [=](bool ok, const QString &error) {
        batch->deleteLater();
        if (!ok) {
            setState(false);
            return emit failed(name, error, {});
        }
        importArchive(archive, name, project.value("icon").toString(),
                      {{"provider", version.value("provider")}, {"project", version.value("project")},
                       {"version", version.value("id")}, {"number", version.value("number")}});
    });
    batch->start();
}

void Modpacks::importFile(const QString &pathOrUrl) {
    if (m_busy) return;
    const QString path = pathOrUrl.startsWith("file:") ? QUrl(pathOrUrl).toLocalFile() : pathOrUrl;
    if (!QFileInfo(path).isFile())
        return emit failed(QFileInfo(path).fileName(), tr("That isn't a file"), {});
    importArchive(path, QFileInfo(path).completeBaseName(), {}, {});
}

void Modpacks::importArchive(const QString &archive, const QString &name, const QString &icon, const QVariantMap &source) {
    setState(true, tr("Reading %1").arg(name));
    const QStringList entries = archiveEntries(archive);
    if (entries.contains("modrinth.index.json"))
        return importModrinth(archive, name, icon, source, archiveMember(archive, "modrinth.index.json"));
    if (entries.contains("manifest.json"))
        return importCurse(archive, name, icon, source, archiveMember(archive, "manifest.json"));
    setState(false);
    emit failed(name, tr("This isn't a Modrinth or CurseForge modpack"), {});
}

void Modpacks::importModrinth(const QString &archive, const QString &fallbackName, const QString &icon,
                              const QVariantMap &source, const QByteArray &indexJson) {
    const QJsonObject index = QJsonDocument::fromJson(indexJson).object();
    const QJsonObject deps = index.value("dependencies").toObject();
    const QString mc = deps.value("minecraft").toString();
    const QString name = index.value("name").toString(fallbackName);
    if (mc.isEmpty()) {
        setState(false);
        return emit failed(name, tr("The pack doesn't say which Minecraft it's for"), {});
    }
    QString loader = "vanilla", loaderVersion;
    if (deps.contains("fabric-loader")) { loader = "fabric"; loaderVersion = deps.value("fabric-loader").toString(); }
    else if (deps.contains("quilt-loader")) { loader = "quilt"; loaderVersion = deps.value("quilt-loader").toString(); }
    else if (deps.contains("neoforge")) { loader = "neoforge"; loaderVersion = deps.value("neoforge").toString(); }
    else if (deps.contains("forge")) { loader = "forge"; loaderVersion = mc + "-" + deps.value("forge").toString(); }

    const QString id = m_instances->create(name, mc, loader, loaderVersion, icon, QJsonObject::fromVariantMap(source));
    Instance instance;
    if (id.isEmpty() || !m_instances->find(id, &instance)) {
        setState(false);
        return emit failed(name, tr("Couldn't create the instance folder"), {});
    }

    auto *batch = new DownloadBatch(this);
    QJsonObject content;
    for (const QJsonValue &value : index.value("files").toArray()) {
        const QJsonObject f = value.toObject();
        const QString path = f.value("path").toString();
        if (f.value("env").toObject().value("client").toString() == "unsupported" || !safeRelative(path))
            continue;
        const QJsonArray urls = f.value("downloads").toArray();
        if (urls.isEmpty()) continue;
        const QString sha1 = f.value("hashes").toObject().value("sha1").toString();
        batch->add(QUrl(urls.first().toString()), instance.gameDir() + "/" + path, sha1, f.value("fileSize").toInteger(-1));
        // Names and icons are filled in by Content the first time it's shown.
        content.insert(path, QJsonObject{});
    }
    setState(true, tr("Downloading %1").arg(name), 0);
    connect(batch, &DownloadBatch::progress, this, [=](qint64, qint64, int files, int total) {
        if (total > 0) setState(true, tr("Downloading %1 (%2 of %3)").arg(name).arg(files).arg(total), double(files) / total);
    });
    connect(batch, &DownloadBatch::finished, this, [=](bool ok, const QString &error) {
        batch->deleteLater();
        if (!ok) {
            setState(false);
            return emit failed(name, error, {});
        }
        finishImport(id, archive, {"overrides", "client-overrides"}, name, {});
    });
    batch->start();
}

void Modpacks::importCurse(const QString &archive, const QString &fallbackName, const QString &icon,
                           const QVariantMap &source, const QByteArray &manifestJson) {
    const QJsonObject manifest = QJsonDocument::fromJson(manifestJson).object();
    const QJsonObject minecraft = manifest.value("minecraft").toObject();
    const QString mc = minecraft.value("version").toString();
    const QString name = manifest.value("name").toString(fallbackName);
    if (mc.isEmpty()) {
        setState(false);
        return emit failed(name, tr("The pack doesn't say which Minecraft it's for"), {});
    }
    if (!m_settings->hasCurseForge()) {
        setState(false);
        return emit failed(name, tr("Add a CurseForge API key in Settings to install CurseForge packs."), {});
    }
    QPair<QString, QString> loader{"vanilla", {}};
    for (const QJsonValue &l : minecraft.value("modLoaders").toArray())
        if (l.toObject().value("primary").toBool() || loader.first == "vanilla")
            loader = curseLoader(l.toObject().value("id").toString(), mc);

    QJsonArray fileIds, modIds;
    for (const QJsonValue &f : manifest.value("files").toArray()) {
        fileIds.append(f.toObject().value("fileID"));
        modIds.append(f.toObject().value("projectID"));
    }
    const QString overrides = manifest.value("overrides").toString("overrides");
    setState(true, tr("Looking up %1's files").arg(name));

    Http::postJson(this, QUrl(QString(kCurse) + "/mods/files"), QJsonDocument(QJsonObject{{"fileIds", fileIds}}),
                   [=](bool ok, const QJsonDocument &files, const QString &error, int) {
        if (!ok) {
            setState(false);
            return emit failed(name, tr("CurseForge couldn't list the pack's files: %1").arg(error), {});
        }
        Http::postJson(this, QUrl(QString(kCurse) + "/mods"), QJsonDocument(QJsonObject{{"modIds", modIds}}),
                       [=](bool ok, const QJsonDocument &mods, const QString &error, int) {
            if (!ok) {
                setState(false);
                return emit failed(name, tr("CurseForge couldn't list the pack's projects: %1").arg(error), {});
            }
            QHash<qint64, QJsonObject> modById;
            for (const QJsonValue &m : mods.object().value("data").toArray())
                modById.insert(m.toObject().value("id").toInteger(), m.toObject());

            const QString id = m_instances->create(name, mc, loader.first, loader.second, icon,
                                                   QJsonObject::fromVariantMap(source));
            Instance instance;
            if (id.isEmpty() || !m_instances->find(id, &instance)) {
                setState(false);
                return emit failed(name, tr("Couldn't create the instance folder"), {});
            }
            auto *batch = new DownloadBatch(this);
            QVariantList blocked;
            QJsonObject content;
            for (const QJsonValue &value : files.object().value("data").toArray()) {
                const QVariantMap file = Catalog::curseFile(value.toObject());
                const QJsonObject mod = modById.value(value.toObject().value("modId").toInteger());
                const int classId = mod.value("classId").toInt(6);
                const QString folder = classId == 12 ? "resourcepacks" : classId == 6552 ? "shaderpacks" : "mods";
                const QString fileName = file.value("file").toString();
                if (!safeRelative(fileName)) continue;
                const QString key = folder + "/" + fileName;
                if (!file.value("downloadable").toBool()) {
                    blocked.append(QVariantMap{{"title", mod.value("name").toString()}, {"file", fileName},
                                               {"url", mod.value("links").toObject().value("websiteUrl").toString()
                                                           + "/files/" + file.value("id").toString()},
                                               {"folder", instance.gameDir() + "/" + folder}});
                    continue;
                }
                batch->add(QUrl(file.value("url").toString()), instance.gameDir() + "/" + key, file.value("sha1").toString(),
                           file.value("size").toLongLong() > 0 ? file.value("size").toLongLong() : -1);
                content.insert(key, QJsonObject{{"provider", "curseforge"}, {"project", file.value("project").toString()},
                                                {"version", file.value("id").toString()}, {"number", file.value("name").toString()},
                                                {"title", mod.value("name").toString()},
                                                {"icon", mod.value("logo").toObject().value("thumbnailUrl").toString()},
                                                {"sha1", file.value("sha1").toString()}, {"size", file.value("size").toLongLong()}});
            }
            Content::writeIndex(instance.folder(), content);
            setState(true, tr("Downloading %1").arg(name), 0);
            connect(batch, &DownloadBatch::progress, this, [=](qint64, qint64, int done, int total) {
                if (total > 0) setState(true, tr("Downloading %1 (%2 of %3)").arg(name).arg(done).arg(total), double(done) / total);
            });
            connect(batch, &DownloadBatch::finished, this, [=](bool ok, const QString &error) {
                batch->deleteLater();
                if (!ok) {
                    setState(false);
                    return emit failed(name, error, {});
                }
                finishImport(id, archive, {overrides}, name, blocked);
            });
            batch->start();
        }, Catalog::curseHeaders(m_settings));
    }, Catalog::curseHeaders(m_settings));
}

// Moves everything under from into to, replacing files that are already there.
static void mergeInto(const QString &from, const QString &to) {
    QDirIterator it(from, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString source = it.next();
        const QString target = to + "/" + QDir(from).relativeFilePath(source);
        QDir().mkpath(QFileInfo(target).absolutePath());
        QFile::remove(target);
        if (!QFile::rename(source, target))
            QFile::copy(source, target);
    }
}

void Modpacks::finishImport(const QString &instanceId, const QString &archive, const QStringList &overrideDirs,
                            const QString &name, const QVariantList &blocked) {
    Instance instance;
    if (!m_instances->find(instanceId, &instance)) {
        setState(false);
        return;
    }
    setState(true, tr("Unpacking %1").arg(name));
    const QString staging = instance.folder() + "/.unpack";
    const QString gameDir = instance.gameDir();
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [=] {
        watcher->deleteLater();
        setState(false);
        const QString error = watcher->result();
        if (!error.isEmpty())
            return emit failed(name, error, blocked);
        if (!blocked.isEmpty())
            emit failed(name, tr("%n file(s) can only be downloaded from CurseForge's website. Put them in the folders shown, then play.",
                                 "", int(blocked.size())), blocked);
        emit imported(instanceId, name);
    });
    watcher->setFuture(QtConcurrent::run([=]() -> QString {
        const QStringList entries = archiveEntries(archive);
        QDir(staging).removeRecursively();
        QDir().mkpath(staging);
        // Later folders win: client-overrides replace overrides.
        for (const QString &dir : overrideDirs) {
            const bool present = std::any_of(entries.begin(), entries.end(),
                                             [&](const QString &e) { return e.startsWith(dir + "/"); });
            if (!present || !safeRelative(dir)) continue;
            // bsdtar refuses absolute paths and "..", so the pack can't write outside staging.
            if (QProcess::execute("bsdtar", {"-xf", archive, "-C", staging, dir}) != 0)
                return QCoreApplication::translate("Modpacks", "Couldn't unpack the pack's %1 folder").arg(dir);
            mergeInto(staging + "/" + dir, gameDir);
        }
        QDir(staging).removeRecursively();
        return {};
    }));
}
