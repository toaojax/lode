#pragma once

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>
#include <functional>

// Minecraft's own metadata: the version manifest, per-version JSON, and the
// rules that decide which libraries and arguments apply on this machine.
namespace Mojang {

struct Library {
    QString name;      // group:artifact:version[:classifier]
    QString path;      // relative to the libraries directory
    QUrl url;          // empty when the file only comes from an installer
    QString sha1;
    qint64 size = -1;
    bool native = false;  // an old-style natives jar to unpack, not put on the classpath
    QStringList exclude;
};

struct Version {
    QString id;
    QString type;
    QString mainClass;
    QString assets;            // the asset index id
    QUrl assetIndexUrl;
    QString assetIndexSha1;
    qint64 assetIndexSize = -1;
    QString jarVersion;        // which version's client jar to run
    QUrl clientUrl;
    QString clientSha1;
    qint64 clientSize = -1;
    QList<Library> libraries;
    QJsonArray gameArguments;
    QJsonArray jvmArguments;
    QString legacyArguments;   // minecraftArguments, before 1.13
    QString javaComponent;
    int javaMajor = 0;
    QStringList chain;         // this id first, then what it inherits from
};

struct ManifestEntry {
    QString id;
    QString type;
    QUrl url;
    QString sha1;
    QDateTime released;
};

QString osName();
QString archName();

// Mojang's rules: a list of allow/disallow entries matched against the OS and
// the launcher's feature flags. No rules means allowed.
bool rulesAllow(const QJsonArray &rules, const QHash<QString, bool> &features = {});

QString versionJsonPath(const QString &id);
QString clientJarPath(const QString &id);

// Reads the version and everything it inherits from, all of which must
// already be on disk. Child libraries come first and win over the parent's
// copy of the same artifact, the way the official launcher merges them.
bool resolve(const QString &id, Version *out, QString *error);

// A version's library entry turned into the file it needs on this machine,
// or nothing if its rules leave it out here.
QList<Library> librariesFor(const QJsonObject &library);

class Manifest : public QObject {
    Q_OBJECT
public:
    using Callback = std::function<void(bool ok, const QString &error)>;

    explicit Manifest(QObject *parent = nullptr);

    // Uses a copy cached for an hour, and the stale copy when offline.
    void load(Callback done, bool force = false);
    bool loaded() const { return !m_entries.isEmpty(); }
    QList<ManifestEntry> entries() const { return m_entries; }
    ManifestEntry find(const QString &id) const;
    QString latestRelease() const { return m_latestRelease; }

    // Fetches versions/<id>/<id>.json for a vanilla version if it's missing
    // or doesn't match the manifest's checksum.
    void ensureVersion(const QString &id, Callback done);

signals:
    void changed();

private:
    bool parse(const QByteArray &data);

    QList<ManifestEntry> m_entries;
    QString m_latestRelease;
    QList<Callback> m_waiting;
    bool m_loading = false;
};

}
