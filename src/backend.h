#pragma once

#include <QObject>
#include <QVariantList>

class Accounts;
class Catalog;
class Content;
class Instances;
class JavaRuntimes;
class Launcher;
class Loaders;
class Modpacks;
class Settings;
namespace Mojang { class Manifest; }

// Owns every service and hands QML the few things that don't belong to one
// of them: game version lists, loader versions, folders and notices.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList gameVersions READ gameVersions NOTIFY gameVersionsChanged)
    Q_PROPERTY(QString latestRelease READ latestRelease NOTIFY gameVersionsChanged)
    Q_PROPERTY(QString dataPath READ dataPath CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend() override;

    Settings *settings() const { return m_settings; }
    Accounts *accounts() const { return m_accounts; }
    Instances *instances() const { return m_instances; }
    Launcher *launcher() const { return m_launcher; }
    Catalog *catalog() const { return m_catalog; }
    Content *content() const { return m_content; }
    Modpacks *modpacks() const { return m_modpacks; }

    QVariantList gameVersions() const;
    QString latestRelease() const;
    QString dataPath() const;
    QString version() const;

    Q_INVOKABLE void loadGameVersions(bool force = false);
    Q_INVOKABLE void listLoaderVersions(const QString &loader, const QString &minecraft);
    Q_INVOKABLE QString createInstance(const QString &name, const QString &minecraft, const QString &loader,
                                       const QString &loaderVersion);
    Q_INVOKABLE void openFolder(const QString &path) const;
    Q_INVOKABLE void openUrl(const QString &url) const;
    Q_INVOKABLE void copyText(const QString &text) const;
    Q_INVOKABLE QString loaderName(const QString &loader) const;

signals:
    void gameVersionsChanged();
    void loaderVersionsReady(const QString &loader, const QString &minecraft, const QVariantList &versions,
                             const QString &error);
    // Short messages for the toast at the bottom of the window.
    void notice(const QString &message, const QString &kind);

private:
    Settings *m_settings;
    Accounts *m_accounts;
    Instances *m_instances;
    Mojang::Manifest *m_manifest;
    JavaRuntimes *m_java;
    Loaders *m_loaders;
    Launcher *m_launcher;
    Catalog *m_catalog;
    Content *m_content;
    Modpacks *m_modpacks;
};
