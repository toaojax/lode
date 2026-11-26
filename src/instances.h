#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QString>
#include <QTimer>

struct Instance {
    QString id;             // the folder name under instances/
    QString name;
    QString minecraft;      // game version, e.g. 1.21.1
    QString loader;         // vanilla, fabric, quilt, forge, neoforge
    QString loaderVersion;
    QString versionId;      // the installed version JSON this runs, once known
    int memoryMb = 0;       // 0 means the launcher default
    QString javaArgs;
    QString iconUrl;
    QDateTime created;
    QDateTime lastPlayed;
    qint64 playSeconds = 0;
    QJsonObject pack;       // where a modpack instance came from: provider, project, version

    QString folder() const;
    QString gameDir() const;
    QJsonObject toJson() const;
    static Instance fromJson(const QString &id, const QJsonObject &o);
};

// Every instance, most recently played first. Edits from outside (a folder
// deleted in the file manager, a modpack import) show up without a restart.
class Instances : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role { IdRole = Qt::UserRole + 1, NameRole, MinecraftRole, LoaderRole, LoaderVersionRole, IconRole,
                LastPlayedRole, PlaySecondsRole, MemoryRole, JavaArgsRole, PackRole, FolderRole };

    explicit Instances(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap get(const QString &id) const;
    Q_INVOKABLE int indexOf(const QString &id) const;
    Q_INVOKABLE QString idAt(int row) const;
    Q_INVOKABLE void rename(const QString &id, const QString &name);
    Q_INVOKABLE void setMemory(const QString &id, int megabytes);
    Q_INVOKABLE void setJavaArgs(const QString &id, const QString &args);
    // Moves the folder to the trash, so worlds can be recovered.
    Q_INVOKABLE bool remove(const QString &id);
    Q_INVOKABLE QString duplicate(const QString &id);

    QString create(const QString &name, const QString &minecraft, const QString &loader,
                   const QString &loaderVersion, const QString &iconUrl = {}, const QJsonObject &pack = {});
    bool find(const QString &id, Instance *out) const;
    void update(const Instance &instance);
    void reload();

signals:
    void countChanged();

private:
    void sortItems();
    static bool write(const Instance &instance);

    QList<Instance> m_items;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
