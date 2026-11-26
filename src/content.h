#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QVariantMap>

class Instances;
class Settings;

// What's inside one instance: its mods, resource packs or shader packs. Each
// file is matched to the project it came from, either because lode installed
// it (and wrote that down in content.json) or by asking Modrinth about its
// hash, so dropped-in files get names, icons and updates too.
class Content : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString instanceId READ instanceId WRITE setInstanceId NOTIFY instanceChanged)
    Q_PROPERTY(QString category READ category WRITE setCategory NOTIFY instanceChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(int updateCount READ updateCount NOTIFY stateChanged)
    Q_PROPERTY(bool checkedUpdates READ checkedUpdates NOTIFY stateChanged)
    Q_PROPERTY(QStringList installedProjects READ installedProjects NOTIFY installedChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY stateChanged)

public:
    enum Role { FileRole = Qt::UserRole + 1, TitleRole, VersionRole, IconRole, EnabledRole, ProviderRole,
                ProjectRole, UpdateRole, SizeRole };

    Content(Settings *settings, Instances *instances, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString instanceId() const { return m_instance; }
    QString category() const { return m_category; }
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    int updateCount() const { return m_updates.size(); }
    bool checkedUpdates() const { return m_checked; }
    QStringList installedProjects() const { return m_projects.values(); }

    void setInstanceId(const QString &id);
    void setCategory(const QString &category);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void toggle(const QString &file);
    Q_INVOKABLE bool remove(const QString &file);
    Q_INVOKABLE void checkUpdates();
    Q_INVOKABLE void update(const QString &file);
    Q_INVOKABLE void updateAll();
    Q_INVOKABLE QString folderPath() const;

    // Installs one version from Catalog::fetchVersions into an instance,
    // with every required dependency that isn't there yet.
    Q_INVOKABLE void install(const QString &instanceId, const QVariantMap &version, const QVariantMap &project);
    // Every project already in an instance, across categories.
    Q_INVOKABLE QStringList projectsIn(const QString &instanceId) const;

    static QString folderFor(const QString &projectType);
    static QJsonObject readIndex(const QString &instanceFolder);
    static void writeIndex(const QString &instanceFolder, const QJsonObject &index);
    static QVariantMap pickVersion(const QVariantList &versions);

signals:
    void instanceChanged();
    void stateChanged();
    void installedChanged();
    void installed(const QString &instanceId, const QString &title, int extra);
    void installFailed(const QString &title, const QString &error, const QVariantList &blocked);

private:
    struct Entry {
        QString file;   // name on disk, maybe with .disabled
        QString key;    // category/name without .disabled
        QString title, version, icon, provider, project, versionId, sha1;
        qint64 size = 0;
        bool enabled = true;
    };
    struct Install;

    void setBusy(bool busy, const QString &status = {});
    void identify(const QList<Entry> &unknown, int generation);
    void resolveNext(Install *job);
    void downloadAll(Install *job);
    void collectVersion(Install *job, const QVariantMap &version, const QVariantMap &project);
    void replaceFile(const QString &file, const QVariantMap &version);

    Settings *m_settings;
    Instances *m_instances;
    QString m_instance, m_category = "mods";
    QList<Entry> m_entries;
    QHash<QString, QVariantMap> m_updates;  // file → newer version
    QSet<QString> m_projects;
    bool m_busy = false, m_checked = false;
    QString m_status;
    int m_generation = 0;
};
