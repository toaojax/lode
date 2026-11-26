#include "content.h"

#include "catalog.h"
#include "downloads.h"
#include "http.h"
#include "instances.h"
#include "paths.h"
#include "settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrlQuery>
#include <QtConcurrent>

static const char *kModrinth = "https://api.modrinth.com/v2";
static const char *kCurse = "https://api.curseforge.com/v1";

struct Content::Install {
    QString instanceId, minecraft, loader, title;
    QList<QVariantMap> queue;            // {provider, project, version?} still to look up
    QList<QPair<QVariantMap, QVariantMap>> chosen;  // version, project
    QSet<QString> seen;
    QVariantList blocked;
};

Content::Content(Settings *settings, Instances *instances, QObject *parent)
    : QAbstractListModel(parent), m_settings(settings), m_instances(instances) {}

int Content::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_entries.size(); }

QVariant Content::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};
    const Entry &e = m_entries[index.row()];
    switch (role) {
    case FileRole: return e.file;
    case TitleRole: return e.title;
    case VersionRole: return e.version;
    case IconRole: return e.icon;
    case EnabledRole: return e.enabled;
    case ProviderRole: return e.provider;
    case ProjectRole: return e.project;
    case UpdateRole: return m_updates.value(e.file).value("number").toString();
    case SizeRole: return e.size;
    }
    return {};
}

QHash<int, QByteArray> Content::roleNames() const {
    return {{FileRole, "file"}, {TitleRole, "title"}, {VersionRole, "version"}, {IconRole, "icon"},
            {EnabledRole, "active"}, {ProviderRole, "provider"}, {ProjectRole, "project"},
            {UpdateRole, "newer"}, {SizeRole, "size"}};
}

QString Content::folderFor(const QString &type) {
    if (type == "resourcepack") return "resourcepacks";
    if (type == "shader") return "shaderpacks";
    return "mods";
}

QJsonObject Content::readIndex(const QString &folder) {
    return QJsonDocument::fromJson(Paths::readAll(folder + "/content.json")).object();
}

void Content::writeIndex(const QString &folder, const QJsonObject &index) {
    Paths::writeAtomically(folder + "/content.json", QJsonDocument(index).toJson());
}

// Newest release, or the newest of anything when a project only has betas.
QVariantMap Content::pickVersion(const QVariantList &versions) {
    for (const QVariant &v : versions)
        if (v.toMap().value("channel").toString() == "release")
            return v.toMap();
    return versions.isEmpty() ? QVariantMap() : versions.first().toMap();
}

void Content::setBusy(bool busy, const QString &status) {
    m_busy = busy;
    m_status = status;
    emit stateChanged();
}

void Content::setInstanceId(const QString &id) {
    if (m_instance == id) return;
    m_instance = id;
    m_updates.clear();
    m_checked = false;
    emit instanceChanged();
    refresh();
}

void Content::setCategory(const QString &category) {
    if (m_category == category) return;
    m_category = category;
    m_updates.clear();
    m_checked = false;
    emit instanceChanged();
    refresh();
}

QString Content::folderPath() const {
    Instance i;
    return m_instances->find(m_instance, &i) ? i.gameDir() + "/" + m_category : QString();
}

QStringList Content::projectsIn(const QString &instanceId) const {
    Instance i;
    if (!m_instances->find(instanceId, &i))
        return {};
    QStringList out;
    const QJsonObject index = readIndex(i.folder());
    for (auto it = index.begin(); it != index.end(); ++it) {
        const QString project = it.value().toObject().value("project").toString();
        // Only count projects whose file is still there.
        const QString path = i.gameDir() + "/" + it.key();
        if (!project.isEmpty() && (QFileInfo::exists(path) || QFileInfo::exists(path + ".disabled")))
            out.append(project);
    }
    return out;
}

void Content::refresh() {
    const int generation = ++m_generation;
    Instance instance;
    beginResetModel();
    m_entries.clear();
    m_projects.clear();
    if (!m_instances->find(m_instance, &instance)) {
        endResetModel();
        emit stateChanged();
        emit installedChanged();
        return;
    }
    const QString dir = instance.gameDir() + "/" + m_category;
    const QJsonObject index = readIndex(instance.folder());
    const QStringList filters = m_category == "mods" ? QStringList{"*.jar", "*.jar.disabled"}
                                                     : QStringList{"*.zip", "*.zip.disabled", "*.jar"};
    QList<Entry> unknown;
    const QFileInfoList files = QDir(dir).entryInfoList(filters, QDir::Files, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &info : files) {
        Entry e;
        e.file = info.fileName();
        e.enabled = !e.file.endsWith(".disabled");
        const QString base = e.enabled ? e.file : e.file.chopped(9);
        e.key = m_category + "/" + base;
        e.size = info.size();
        const QJsonObject meta = index.value(e.key).toObject();
        e.title = meta.value("title").toString();
        e.version = meta.value("number").toString();
        e.icon = meta.value("icon").toString();
        e.provider = meta.value("provider").toString();
        e.project = meta.value("project").toString();
        e.versionId = meta.value("version").toString();
        e.sha1 = meta.value("sha1").toString();
        if (e.title.isEmpty()) {
            e.title = QFileInfo(base).completeBaseName();
            if (!meta.contains("size") || meta.value("size").toInteger() != e.size)
                unknown.append(e);
        }
        if (!e.project.isEmpty())
            m_projects.insert(e.project);
        m_entries.append(e);
    }
    // Shader packs can also be plain folders.
    if (m_category == "shaderpacks") {
        for (const QFileInfo &info : QDir(dir).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            Entry e;
            e.file = info.fileName();
            e.key = m_category + "/" + e.file;
            e.title = e.file;
            m_entries.append(e);
        }
    }
    endResetModel();
    emit stateChanged();
    emit installedChanged();
    if (!unknown.isEmpty())
        identify(unknown, generation);
}

void Content::identify(const QList<Entry> &unknown, int generation) {
    Instance instance;
    if (!m_instances->find(m_instance, &instance))
        return;
    const QString dir = instance.gameDir() + "/" + m_category;
    const QString folder = instance.folder();
    auto *watcher = new QFutureWatcher<QList<Entry>>(this);
    connect(watcher, &QFutureWatcher<QList<Entry>>::finished, this, [=] {
        watcher->deleteLater();
        if (generation != m_generation) return;
        const QList<Entry> hashed = watcher->result();
        QJsonArray hashes;
        for (const Entry &e : hashed) hashes.append(e.sha1);
        Http::postJson(this, QUrl(QString(kModrinth) + "/version_files"),
                       QJsonDocument(QJsonObject{{"hashes", hashes}, {"algorithm", "sha1"}}),
                       [=](bool ok, const QJsonDocument &json, const QString &, int) {
            if (generation != m_generation) return;
            const QJsonObject byHash = ok ? json.object() : QJsonObject();
            QJsonArray ids;
            for (auto it = byHash.begin(); it != byHash.end(); ++it)
                ids.append(it.value().toObject().value("project_id").toString());
            auto record = [=](const QHash<QString, QJsonObject> &projects) {
                if (generation != m_generation) return;
                QJsonObject index = readIndex(folder);
                for (const Entry &e : hashed) {
                    QJsonObject meta{{"sha1", e.sha1}, {"size", e.size}};
                    if (byHash.contains(e.sha1)) {
                        const QJsonObject v = byHash.value(e.sha1).toObject();
                        const QJsonObject p = projects.value(v.value("project_id").toString());
                        meta.insert("provider", "modrinth");
                        meta.insert("project", v.value("project_id").toString());
                        meta.insert("version", v.value("id").toString());
                        meta.insert("number", v.value("version_number").toString());
                        meta.insert("title", p.value("title").toString());
                        meta.insert("icon", p.value("icon_url").toString());
                    }
                    index.insert(e.key, meta);
                }
                writeIndex(folder, index);
                refresh();
            };
            if (ids.isEmpty())
                return record({});
            QUrl url(QString(kModrinth) + "/projects");
            url.setQuery(QUrlQuery{{"ids", QString::fromUtf8(QJsonDocument(ids).toJson(QJsonDocument::Compact))}});
            Http::getJson(this, url, [=](bool ok, const QJsonDocument &json, const QString &, int) {
                QHash<QString, QJsonObject> projects;
                if (ok)
                    for (const QJsonValue &p : json.array())
                        projects.insert(p.toObject().value("id").toString(), p.toObject());
                record(projects);
            });
        });
    });
    watcher->setFuture(QtConcurrent::run([unknown, dir] {
        QList<Entry> out;
        for (Entry e : unknown) {
            e.sha1 = DownloadBatch::sha1Of(dir + "/" + e.file);
            if (!e.sha1.isEmpty())
                out.append(e);
        }
        return out;
    }));
}

void Content::toggle(const QString &file) {
    const QString dir = folderPath();
    if (dir.isEmpty()) return;
    const QString target = file.endsWith(".disabled") ? file.chopped(9) : file + ".disabled";
    if (QFile::rename(dir + "/" + file, dir + "/" + target)) {
        if (m_updates.contains(file))
            m_updates.insert(target, m_updates.take(file));
        refresh();
    }
}

bool Content::remove(const QString &file) {
    const QString dir = folderPath();
    if (dir.isEmpty() || !QFile::moveToTrash(dir + "/" + file))
        return false;
    m_updates.remove(file);
    refresh();
    return true;
}

void Content::checkUpdates() {
    Instance instance;
    if (!m_instances->find(m_instance, &instance) || m_busy)
        return;
    m_updates.clear();
    setBusy(true, tr("Checking for updates"));
    const int generation = m_generation;
    QJsonArray hashes;
    QHash<QString, QString> fileByHash;
    QList<Entry> curse;
    for (const Entry &e : std::as_const(m_entries)) {
        if (e.provider == "modrinth" && !e.sha1.isEmpty()) {
            hashes.append(e.sha1);
            fileByHash.insert(e.sha1, e.file);
        } else if (e.provider == "curseforge" && !e.project.isEmpty()) {
            curse.append(e);
        }
    }
    auto pending = std::make_shared<int>(1 + int(curse.size()));
    auto finishOne = [this, pending, generation] {
        if (--*pending > 0 || generation != m_generation) return;
        m_checked = true;
        setBusy(false);
        emit dataChanged(index(0), index(int(m_entries.size()) - 1), {UpdateRole});
    };

    QJsonObject body{{"hashes", hashes}, {"algorithm", "sha1"}, {"game_versions", QJsonArray{instance.minecraft}}};
    if (m_category == "mods")
        body.insert("loaders", QJsonArray{instance.loader});
    else if (m_category == "resourcepacks")
        body.insert("loaders", QJsonArray{"minecraft"});
    if (hashes.isEmpty()) {
        finishOne();
    } else {
        Http::postJson(this, QUrl(QString(kModrinth) + "/version_files/update"), QJsonDocument(body),
                       [=](bool ok, const QJsonDocument &json, const QString &, int) {
            if (generation != m_generation) return;
            const QJsonObject o = json.object();
            for (auto it = o.begin(); ok && it != o.end(); ++it) {
                const QString file = fileByHash.value(it.key());
                QVariantMap v = Catalog::modrinthVersion(it.value().toObject());
                if (!file.isEmpty() && v.value("sha1").toString() != it.key())
                    m_updates.insert(file, v);
            }
            finishOne();
        });
    }
    for (const Entry &e : curse) {
        if (!m_settings->hasCurseForge()) { finishOne(); continue; }
        QUrlQuery q{{"gameVersion", instance.minecraft}, {"pageSize", "20"}};
        if (m_category == "mods" && Catalog::curseLoader(instance.loader))
            q.addQueryItem("modLoaderType", QString::number(Catalog::curseLoader(instance.loader)));
        QUrl url(QString(kCurse) + "/mods/" + e.project + "/files");
        url.setQuery(q);
        Http::getJson(this, url, [=](bool ok, const QJsonDocument &json, const QString &, int) {
            if (generation != m_generation) return;
            QVariantMap newest;
            for (const QJsonValue &f : json.object().value("data").toArray()) {
                const QVariantMap v = Catalog::curseFile(f.toObject());
                if (newest.isEmpty() || v.value("date").toString() > newest.value("date").toString())
                    newest = v;
            }
            if (ok && !newest.isEmpty() && newest.value("id").toString() != e.versionId
                && newest.value("downloadable").toBool())
                m_updates.insert(e.file, newest);
            finishOne();
        }, Catalog::curseHeaders(m_settings));
    }
}

void Content::replaceFile(const QString &file, const QVariantMap &version) {
    Instance instance;
    if (!m_instances->find(m_instance, &instance)) return;
    const QString dir = instance.gameDir() + "/" + m_category;
    const bool disabled = file.endsWith(".disabled");
    const QString newName = version.value("file").toString();
    if (newName.isEmpty() || newName.contains('/') || newName.startsWith('.')) return;
    const QString target = dir + "/" + newName + (disabled ? ".disabled" : "");
    const QString folder = instance.folder();
    const QString category = m_category;

    QString oldKey = category + "/" + (disabled ? file.chopped(9) : file);
    const QJsonObject oldMeta = readIndex(folder).value(oldKey).toObject();

    auto *batch = new DownloadBatch(this);
    batch->add(QUrl(version.value("url").toString()), target + ".new", version.value("sha1").toString());
    setBusy(true, tr("Updating %1").arg(oldMeta.value("title").toString(file)));
    connect(batch, &DownloadBatch::finished, this, [=](bool ok, const QString &error) {
        batch->deleteLater();
        if (!ok) {
            setBusy(false);
            emit installFailed(oldMeta.value("title").toString(file), error, {});
            return;
        }
        // The old file goes to the trash only once the new one is complete.
        if (QFileInfo(dir + "/" + file) != QFileInfo(target))
            QFile::moveToTrash(dir + "/" + file);
        QFile::remove(target);
        QFile::rename(target + ".new", target);
        QJsonObject index = readIndex(folder);
        index.remove(oldKey);
        QJsonObject meta = oldMeta;
        meta.insert("version", version.value("id").toString());
        meta.insert("number", version.value("number").toString());
        meta.insert("sha1", version.value("sha1").toString());
        meta.insert("size", QFileInfo(target).size());
        index.insert(category + "/" + newName, meta);
        writeIndex(folder, index);
        m_updates.remove(file);
        setBusy(false);
        refresh();
    });
    batch->start();
}

void Content::update(const QString &file) {
    if (m_updates.contains(file) && !m_busy)
        replaceFile(file, m_updates.value(file));
}

void Content::updateAll() {
    // One at a time keeps the index writes in order.
    if (m_updates.isEmpty() || m_busy) return;
    const QString file = m_updates.constBegin().key();
    replaceFile(file, m_updates.value(file));
    auto *connection = new QMetaObject::Connection;
    *connection = connect(this, &Content::stateChanged, this, [this, connection] {
        if (m_busy) return;
        disconnect(*connection);
        delete connection;
        if (!m_updates.isEmpty())
            updateAll();
    });
}

void Content::install(const QString &instanceId, const QVariantMap &version, const QVariantMap &project) {
    Instance instance;
    if (!m_instances->find(instanceId, &instance))
        return emit installFailed(project.value("title").toString(), tr("Pick an instance first"), {});
    auto *job = new Install;
    job->instanceId = instanceId;
    job->minecraft = instance.minecraft;
    job->loader = instance.loader;
    job->title = project.value("title").toString();
    // Projects already in the instance are never pulled in again as dependencies.
    for (const QString &p : projectsIn(instanceId))
        job->seen.insert(p);
    job->seen.insert(version.value("project").toString());
    collectVersion(job, version, project);
    setBusy(true, tr("Resolving %1").arg(job->title));
    resolveNext(job);
}

void Content::collectVersion(Install *job, const QVariantMap &version, const QVariantMap &project) {
    job->chosen.append({version, project});
    if (project.value("projectType", "mod").toString() != "mod")
        return;
    for (const QVariant &d : version.value("dependencies").toList()) {
        const QVariantMap dep = d.toMap();
        const QString projectId = dep.value("project").toString();
        if (dep.value("type").toString() != "required" || projectId.isEmpty() || job->seen.contains(projectId))
            continue;
        job->seen.insert(projectId);
        job->queue.append({{"provider", version.value("provider")}, {"project", projectId}, {"version", dep.value("version")}});
    }
}

void Content::resolveNext(Install *job) {
    if (job->queue.isEmpty())
        return downloadAll(job);
    // Deep chains of dependencies are a sign something's off; stop well before.
    if (job->chosen.size() > 64) {
        setBusy(false);
        emit installFailed(job->title, tr("Too many dependencies"), {});
        delete job;
        return;
    }
    const QVariantMap next = job->queue.takeFirst();
    const QString provider = next.value("provider").toString(), projectId = next.value("project").toString();
    auto fail = [this, job](const QString &error) {
        setBusy(false);
        emit installFailed(job->title, error, {});
        delete job;
    };

    if (provider == "curseforge") {
        Http::getJson(this, QUrl(QString(kCurse) + "/mods/" + projectId), [=](bool ok, const QJsonDocument &json, const QString &error, int) {
            if (!ok) return fail(tr("Couldn't look up a dependency: %1").arg(error));
            const QJsonObject m = json.object().value("data").toObject();
            const QVariantMap project{{"title", m.value("name").toString()}, {"icon", m.value("logo").toObject().value("thumbnailUrl").toString()},
                                      {"projectId", projectId}, {"provider", "curseforge"}, {"projectType", "mod"},
                                      {"url", m.value("links").toObject().value("websiteUrl").toString()}};
            QUrlQuery q{{"gameVersion", job->minecraft}, {"pageSize", "20"}};
            if (Catalog::curseLoader(job->loader))
                q.addQueryItem("modLoaderType", QString::number(Catalog::curseLoader(job->loader)));
            QUrl url(QString(kCurse) + "/mods/" + projectId + "/files");
            url.setQuery(q);
            Http::getJson(this, url, [=](bool ok, const QJsonDocument &json, const QString &error, int) {
                if (!ok) return fail(error);
                QVariantList versions;
                for (const QJsonValue &f : json.object().value("data").toArray())
                    versions.append(Catalog::curseFile(f.toObject()));
                std::stable_sort(versions.begin(), versions.end(), [](const QVariant &a, const QVariant &b) {
                    return a.toMap().value("date").toString() > b.toMap().value("date").toString();
                });
                const QVariantMap version = pickVersion(versions);
                if (version.isEmpty())
                    return fail(tr("%1 needs %2, which has no version for this instance").arg(job->title, project.value("title").toString()));
                collectVersion(job, version, project);
                resolveNext(job);
            }, Catalog::curseHeaders(m_settings));
        }, Catalog::curseHeaders(m_settings));
        return;
    }

    Http::getJson(this, QUrl(QString(kModrinth) + "/project/" + projectId), [=](bool ok, const QJsonDocument &json, const QString &error, int) {
        if (!ok) return fail(tr("Couldn't look up a dependency: %1").arg(error));
        const QJsonObject p = json.object();
        const QVariantMap project{{"title", p.value("title").toString()}, {"icon", p.value("icon_url").toString()},
                                  {"projectId", projectId}, {"provider", "modrinth"}, {"projectType", p.value("project_type").toString()}};
        auto choose = [=](const QVariantMap &version) {
            if (version.isEmpty())
                return fail(tr("%1 needs %2, which has no version for this instance").arg(job->title, project.value("title").toString()));
            collectVersion(job, version, project);
            resolveNext(job);
        };
        const QString versionId = next.value("version").toString();
        if (!versionId.isEmpty()) {
            return Http::getJson(this, QUrl(QString(kModrinth) + "/version/" + versionId),
                                 [=](bool ok, const QJsonDocument &json, const QString &error, int) {
                if (!ok) return fail(error);
                choose(Catalog::modrinthVersion(json.object()));
            });
        }
        QUrl url(QString(kModrinth) + "/project/" + projectId + "/version");
        url.setQuery(QUrlQuery{{"game_versions", QString::fromUtf8(QJsonDocument(QJsonArray{job->minecraft}).toJson(QJsonDocument::Compact))},
                               {"loaders", QString::fromUtf8(QJsonDocument(QJsonArray{job->loader}).toJson(QJsonDocument::Compact))}});
        Http::getJson(this, url, [=](bool ok, const QJsonDocument &json, const QString &error, int) {
            if (!ok) return fail(error);
            QVariantList versions;
            for (const QJsonValue &v : json.array())
                versions.append(Catalog::modrinthVersion(v.toObject()));
            choose(pickVersion(versions));
        });
    });
}

void Content::downloadAll(Install *job) {
    Instance instance;
    if (!m_instances->find(job->instanceId, &instance)) {
        setBusy(false);
        delete job;
        return;
    }
    auto *batch = new DownloadBatch(this);
    QList<QPair<QString, QPair<QVariantMap, QVariantMap>>> placed;
    for (const auto &[version, project] : job->chosen) {
        const QString file = version.value("file").toString();
        if (!version.value("downloadable").toBool() || file.isEmpty()) {
            job->blocked.append(QVariantMap{{"title", project.value("title")}, {"file", file},
                                            {"url", project.value("url").toString() + "/files/" + version.value("id").toString()}});
            continue;
        }
        if (file.contains('/') || file.startsWith('.'))
            continue;
        const QString key = folderFor(project.value("projectType", "mod").toString()) + "/" + file;
        batch->add(QUrl(version.value("url").toString()), instance.gameDir() + "/" + key, version.value("sha1").toString(),
                   version.value("size").toLongLong() > 0 ? version.value("size").toLongLong() : -1);
        placed.append({key, {version, project}});
    }
    setBusy(true, tr("Downloading %1").arg(job->title));
    connect(batch, &DownloadBatch::finished, this, [=](bool ok, const QString &error) {
        batch->deleteLater();
        if (!ok) {
            setBusy(false);
            emit installFailed(job->title, error, job->blocked);
            delete job;
            return;
        }
        QJsonObject index = readIndex(instance.folder());
        for (const auto &[key, pair] : placed) {
            const auto &[version, project] = pair;
            // An older file of the same project is replaced by the new one.
            for (auto it = index.begin(); it != index.end();) {
                const QJsonObject meta = it.value().toObject();
                if (it.key() != key && meta.value("project").toString() == version.value("project").toString()
                    && it.key().section('/', 0, 0) == key.section('/', 0, 0)) {
                    QFile::moveToTrash(instance.gameDir() + "/" + it.key());
                    QFile::moveToTrash(instance.gameDir() + "/" + it.key() + ".disabled");
                    it = index.erase(it);
                } else {
                    ++it;
                }
            }
            index.insert(key, QJsonObject{{"provider", version.value("provider").toString()},
                                          {"project", version.value("project").toString()},
                                          {"version", version.value("id").toString()},
                                          {"number", version.value("number").toString()},
                                          {"title", project.value("title").toString()},
                                          {"icon", project.value("icon").toString()},
                                          {"sha1", version.value("sha1").toString()},
                                          {"size", QFileInfo(instance.gameDir() + "/" + key).size()}});
        }
        writeIndex(instance.folder(), index);
        setBusy(false);
        if (!job->blocked.isEmpty())
            emit installFailed(job->title, tr("Some files can only be downloaded from CurseForge's website"), job->blocked);
        else
            emit installed(job->instanceId, job->title, int(placed.size()) - 1);
        if (job->instanceId == m_instance)
            refresh();
        else
            emit installedChanged();
        delete job;
    });
    batch->start();
}
