#include "mojang.h"

#include "downloads.h"
#include "http.h"
#include "paths.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QSysInfo>

using namespace Mojang;

static const char *kManifestUrl = "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json";

static QString tr(const char *text) { return QCoreApplication::translate("Mojang", text); }

QString Mojang::osName() { return "linux"; }

QString Mojang::archName() {
    const QString arch = QSysInfo::currentCpuArchitecture();
    if (arch == "x86_64") return "x86_64";
    if (arch == "arm64") return "arm64";
    return arch;
}

bool Mojang::rulesAllow(const QJsonArray &rules, const QHash<QString, bool> &features) {
    if (rules.isEmpty())
        return true;
    bool allowed = false;
    for (const QJsonValue &value : rules) {
        const QJsonObject rule = value.toObject();
        bool applies = true;
        const QJsonObject os = rule.value("os").toObject();
        if (os.contains("name") && os.value("name").toString() != osName())
            applies = false;
        if (os.contains("arch")) {
            // "x86" in a rule means a 32-bit machine.
            const QString arch = os.value("arch").toString();
            if (arch == "x86" ? archName() != "x86" : arch != archName())
                applies = false;
        }
        const QJsonObject wanted = rule.value("features").toObject();
        for (auto it = wanted.begin(); it != wanted.end(); ++it)
            if (features.value(it.key(), false) != it.value().toBool())
                applies = false;
        if (applies)
            allowed = rule.value("action").toString() == "allow";
    }
    return allowed;
}

QString Mojang::versionJsonPath(const QString &id) { return Paths::versions() + "/" + id + "/" + id + ".json"; }
QString Mojang::clientJarPath(const QString &id) { return Paths::versions() + "/" + id + "/" + id + ".jar"; }

QList<Library> Mojang::librariesFor(const QJsonObject &lib) {
    QList<Library> out;
    if (!rulesAllow(lib.value("rules").toArray()))
        return out;
    const QString name = lib.value("name").toString();
    const QJsonObject downloads = lib.value("downloads").toObject();

    // Before 1.19, platform natives were a classifier of the same library,
    // picked by the "natives" map and unpacked before launch.
    const QJsonObject natives = lib.value("natives").toObject();
    if (natives.contains(osName())) {
        QString classifier = natives.value(osName()).toString();
        classifier.replace("${arch}", QSysInfo::WordSize == 64 ? "64" : "32");
        const QJsonObject artifact = downloads.value("classifiers").toObject().value(classifier).toObject();
        Library native;
        native.name = name + ":" + classifier;
        native.path = artifact.contains("path") ? artifact.value("path").toString() : Paths::mavenPath(native.name);
        native.url = QUrl(artifact.value("url").toString());
        native.sha1 = artifact.value("sha1").toString();
        native.size = artifact.value("size").toInteger(-1);
        native.native = true;
        for (const QJsonValue &e : lib.value("extract").toObject().value("exclude").toArray())
            native.exclude.append(e.toString());
        if (!native.path.isEmpty())
            out.append(native);
    }

    // Old natives-only entries have no main artifact at all.
    if (natives.contains(osName()) && !downloads.contains("artifact"))
        return out;

    Library library;
    library.name = name;
    if (downloads.contains("artifact")) {
        const QJsonObject artifact = downloads.value("artifact").toObject();
        library.path = artifact.value("path").toString();
        library.url = QUrl(artifact.value("url").toString());
        library.sha1 = artifact.value("sha1").toString();
        library.size = artifact.value("size").toInteger(-1);
    }
    if (library.path.isEmpty())
        library.path = Paths::mavenPath(name);
    // Fabric, Quilt and old Forge name a Maven repository instead of a file.
    if (library.url.isEmpty() && !downloads.contains("artifact")) {
        QString base = lib.value("url").toString();
        if (base.isEmpty() && !lib.contains("downloads"))
            base = "https://libraries.minecraft.net/";
        if (!base.isEmpty()) {
            if (!base.endsWith('/'))
                base += '/';
            library.url = QUrl(base + library.path);
        }
        library.sha1 = lib.value("sha1").toString();
        library.size = lib.value("size").toInteger(-1);
    }
    if (!library.path.isEmpty())
        out.append(library);
    return out;
}

// group:artifact[:classifier], the identity two copies of a library share.
static QString libraryKey(const QString &name) {
    const QStringList parts = name.split(':');
    if (parts.size() < 3)
        return name;
    return parts.size() > 3 ? parts[0] + ":" + parts[1] + ":" + parts.mid(3).join(':') : parts[0] + ":" + parts[1];
}

bool Mojang::resolve(const QString &id, Version *out, QString *error) {
    QList<QJsonObject> chain;
    QString current = id;
    QSet<QString> seen;
    while (!current.isEmpty()) {
        if (seen.contains(current)) {
            *error = tr("Version %1 inherits from itself").arg(current);
            return false;
        }
        seen.insert(current);
        QJsonParseError parse{};
        const QJsonDocument doc = QJsonDocument::fromJson(Paths::readAll(versionJsonPath(current)), &parse);
        if (!doc.isObject()) {
            *error = tr("Version %1 isn't installed").arg(current);
            return false;
        }
        chain.append(doc.object());
        out->chain.append(current);
        current = doc.object().value("inheritsFrom").toString();
    }

    Version &v = *out;
    v.id = id;
    QSet<QString> haveLibraries;
    // Walk from the child up, so the child's values win.
    for (const QJsonObject &json : chain) {
        if (v.type.isEmpty()) v.type = json.value("type").toString();
        if (v.mainClass.isEmpty()) v.mainClass = json.value("mainClass").toString();
        if (v.legacyArguments.isEmpty()) v.legacyArguments = json.value("minecraftArguments").toString();
        if (v.assets.isEmpty() && json.contains("assetIndex")) {
            const QJsonObject index = json.value("assetIndex").toObject();
            v.assets = index.value("id").toString();
            v.assetIndexUrl = QUrl(index.value("url").toString());
            v.assetIndexSha1 = index.value("sha1").toString();
            v.assetIndexSize = index.value("size").toInteger(-1);
        }
        if (v.clientUrl.isEmpty() && json.value("downloads").toObject().contains("client")) {
            const QJsonObject client = json.value("downloads").toObject().value("client").toObject();
            v.clientUrl = QUrl(client.value("url").toString());
            v.clientSha1 = client.value("sha1").toString();
            v.clientSize = client.value("size").toInteger(-1);
            v.jarVersion = json.value("id").toString();
        }
        if (v.javaComponent.isEmpty() && json.contains("javaVersion")) {
            v.javaComponent = json.value("javaVersion").toObject().value("component").toString();
            v.javaMajor = json.value("javaVersion").toObject().value("majorVersion").toInt();
        }
        for (const QJsonValue &lib : json.value("libraries").toArray()) {
            for (const Library &library : librariesFor(lib.toObject())) {
                const QString key = libraryKey(library.name) + (library.native ? "#native" : "");
                if (haveLibraries.contains(key))
                    continue;
                haveLibraries.insert(key);
                v.libraries.append(library);
            }
        }
    }
    // Arguments add up from the parent down: vanilla's first, then the loader's.
    for (auto it = chain.crbegin(); it != chain.crend(); ++it) {
        const QJsonObject args = it->value("arguments").toObject();
        for (const QJsonValue &a : args.value("game").toArray()) v.gameArguments.append(a);
        for (const QJsonValue &a : args.value("jvm").toArray()) v.jvmArguments.append(a);
    }
    if (v.assets.isEmpty())
        v.assets = chain.last().value("assets").toString("legacy");
    if (v.javaComponent.isEmpty()) {
        // Versions from before Mojang shipped Java ran on Java 8.
        v.javaComponent = "jre-legacy";
        v.javaMajor = 8;
    }
    if (v.mainClass.isEmpty()) {
        *error = tr("Version %1 doesn't say how to start the game").arg(id);
        return false;
    }
    return true;
}

Manifest::Manifest(QObject *parent) : QObject(parent) {
    parse(Paths::readAll(Paths::cache() + "/version_manifest_v2.json"));
}

bool Manifest::parse(const QByteArray &data) {
    const QJsonObject o = QJsonDocument::fromJson(data).object();
    const QJsonArray versions = o.value("versions").toArray();
    if (versions.isEmpty())
        return false;
    QList<ManifestEntry> entries;
    for (const QJsonValue &value : versions) {
        const QJsonObject v = value.toObject();
        entries.append({v.value("id").toString(), v.value("type").toString(), QUrl(v.value("url").toString()),
                        v.value("sha1").toString(),
                        QDateTime::fromString(v.value("releaseTime").toString(), Qt::ISODate)});
    }
    m_entries = entries;
    m_latestRelease = o.value("latest").toObject().value("release").toString();
    emit changed();
    return true;
}

ManifestEntry Manifest::find(const QString &id) const {
    for (const ManifestEntry &e : m_entries)
        if (e.id == id)
            return e;
    return {};
}

void Manifest::load(Callback done, bool force) {
    const QString cached = Paths::cache() + "/version_manifest_v2.json";
    const QFileInfo info(cached);
    if (!force && loaded() && info.exists() && info.lastModified().secsTo(QDateTime::currentDateTime()) < 3600)
        return done(true, {});
    m_waiting.append(done);
    if (m_loading)
        return;
    m_loading = true;
    Http::getData(this, QUrl(kManifestUrl), [this, cached](bool ok, const QByteArray &data, const QString &error, int) {
        m_loading = false;
        bool good = ok && parse(data);
        if (good)
            Paths::writeAtomically(cached, data);
        // Offline with an older copy still lets every installed version start.
        const bool usable = good || loaded();
        const QString message = usable ? QString() : tr("Couldn't load Minecraft's version list: %1").arg(error);
        const auto waiting = std::exchange(m_waiting, {});
        for (const auto &callback : waiting)
            callback(usable, message);
    });
}

void Manifest::ensureVersion(const QString &id, Callback done) {
    const QString path = versionJsonPath(id);
    load([this, id, path, done](bool ok, const QString &error) {
        const ManifestEntry entry = find(id);
        if (entry.id.isEmpty()) {
            // Not a Mojang version (or offline): fine if it's already here.
            if (QFileInfo::exists(path))
                return done(true, {});
            return done(false, ok ? tr("Minecraft %1 isn't in Mojang's version list").arg(id) : error);
        }
        DownloadBatch::Item item;
        item.url = entry.url;
        item.path = path;
        item.sha1 = entry.sha1;
        if (DownloadBatch::matches(item))
            return done(true, {});
        Http::getData(this, entry.url, [path, done, id](bool ok, const QByteArray &data, const QString &error, int) {
            if (!ok) {
                if (QFileInfo::exists(path))
                    return done(true, {});
                return done(false, tr("Couldn't fetch Minecraft %1: %2").arg(id, error));
            }
            Paths::writeAtomically(path, data);
            done(true, {});
        });
    });
}
