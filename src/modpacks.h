#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class Instances;
class Settings;

// Turns a modpack into a new instance: Modrinth's .mrpack (a file list with
// hashes plus override folders) or a CurseForge zip (a manifest of project
// and file ids plus overrides). Files the pack's authors keep off third-party
// launchers are listed for the user to fetch by hand, never skipped quietly.
class Modpacks : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)

public:
    Modpacks(Settings *settings, Instances *instances, QObject *parent = nullptr);

    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    double progress() const { return m_progress; }

    // A version from Catalog::fetchVersions of a modpack project.
    Q_INVOKABLE void installVersion(const QVariantMap &version, const QVariantMap &project);
    // A .mrpack or CurseForge .zip on disk.
    Q_INVOKABLE void importFile(const QString &pathOrUrl);

    // CurseForge's loader ids, "forge-47.2.0", in lode's terms.
    static QPair<QString, QString> curseLoader(const QString &id, const QString &minecraft);
    // A pack's file path, rejected if it would land outside the game folder.
    static bool safeRelative(const QString &path);

signals:
    void changed();
    void imported(const QString &instanceId, const QString &name);
    void failed(const QString &name, const QString &error, const QVariantList &blocked);

private:
    void setState(bool busy, const QString &status = {}, double progress = -1);
    void importArchive(const QString &archive, const QString &name, const QString &icon, const QVariantMap &source);
    void importModrinth(const QString &archive, const QString &name, const QString &icon, const QVariantMap &source,
                        const QByteArray &indexJson);
    void importCurse(const QString &archive, const QString &name, const QString &icon, const QVariantMap &source,
                     const QByteArray &manifestJson);
    void finishImport(const QString &instanceId, const QString &archive, const QStringList &overrideDirs,
                      const QString &name, const QVariantList &blocked);

    Settings *m_settings;
    Instances *m_instances;
    bool m_busy = false;
    QString m_status;
    double m_progress = -1;
};
