#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

class QProcess;

// Fabric and Quilt publish a ready-made version JSON per game version. Forge
// and NeoForge ship an installer that has to run, which processes the game
// jar and writes its own version into the shared tree; they get the same
// files the official launcher would.
class Loaders : public QObject {
    Q_OBJECT
public:
    struct LoaderVersion {
        QString version;
        bool stable = true;
    };
    using ListCallback = std::function<void(bool ok, const QList<LoaderVersion> &versions, const QString &error)>;
    using InstallCallback = std::function<void(bool ok, const QString &versionId, const QString &error)>;
    using Log = std::function<void(const QString &line)>;

    explicit Loaders(QObject *parent = nullptr);

    // Newest first.
    void list(const QString &loader, const QString &minecraft, ListCallback done);
    void install(const QString &loader, const QString &minecraft, const QString &loaderVersion,
                 const QString &javaPath, InstallCallback done, Log log = {});

    // The version id a loader install will produce, when it's predictable.
    static QString expectedVersionId(const QString &loader, const QString &minecraft, const QString &loaderVersion);
    // NeoForge numbers its builds after the game: 21.1.x is for 1.21.1.
    static bool neoForgeMatches(const QString &neoVersion, const QString &minecraft);
    static QString displayName(const QString &loader);

private:
    void installProfile(const QString &url, const QString &expectedId, InstallCallback done);
    void runInstaller(const QString &url, const QString &cacheName, const QString &javaPath,
                      const QString &expectedId, InstallCallback done, Log log);
};
