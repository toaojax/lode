#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <functional>

#include "mojang.h"

class Accounts;
class Instances;
class JavaRuntimes;
class Loaders;
class LogModel;
class QProcess;
class Settings;
struct Account;
struct Instance;

// Gets an instance from "never run" to a running game: the version JSON, the
// right Java, the mod loader, libraries, assets, natives, a fresh sign-in,
// and finally the java command line. Several instances can run at once; only
// one prepares at a time so their downloads don't fight.
class Launcher : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString preparingId READ preparingId NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(QStringList running READ running NOTIFY changed)

public:
    Launcher(Settings *settings, Accounts *accounts, Instances *instances, Mojang::Manifest *manifest,
             JavaRuntimes *java, Loaders *loaders, QObject *parent = nullptr);
    ~Launcher() override;

    QString preparingId() const { return m_preparing; }
    QString status() const { return m_status; }
    double progress() const { return m_progress; }
    QStringList running() const { return m_processes.keys(); }

    Q_INVOKABLE void launch(const QString &instanceId);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void kill(const QString &instanceId);
    Q_INVOKABLE LogModel *log(const QString &instanceId);
    Q_INVOKABLE QString stateOf(const QString &instanceId) const;

    // Downloads and installs everything an instance needs without starting
    // it, used after creating one so the first Play is quick.
    using Done = std::function<void(bool ok, const QString &error)>;
    void prepare(const QString &instanceId, Done done);

    struct Context {
        QString versionName, gameDir, assetsRoot, gameAssets, nativesDir, librariesDir, classpath;
        QString playerName, uuid, accessToken, userType, xuid, clientId;
        int width = 0, height = 0;
        int memoryMb = 4096;
        QStringList extraJvm;
    };
    // The full command after "java", from the version's own argument lists
    // (or the pre-1.13 string), with the launcher's memory and flags added.
    static QStringList arguments(const Mojang::Version &version, const Context &context);
    static QString substitute(const QString &text, const QHash<QString, QString> &vars);

signals:
    void changed();
    void failed(const QString &instanceId, const QString &message);
    void started(const QString &instanceId);
    void exited(const QString &instanceId, int code, bool crashed);

private:
    struct Job;
    void step(const QString &status, double progress = -1);
    void fail(const QString &message);
    void finishPrepare(bool ok, const QString &error);
    void ensureFiles(Job *job, Done done);
    void ensureAssets(Job *job, Done done);
    void start(Job *job, const Account &account);

    Settings *m_settings;
    Accounts *m_accounts;
    Instances *m_instances;
    Mojang::Manifest *m_manifest;
    JavaRuntimes *m_java;
    Loaders *m_loaders;
    QHash<QString, LogModel *> m_logs;
    QHash<QString, QProcess *> m_processes;
    Job *m_job = nullptr;
    QString m_preparing, m_status;
    double m_progress = -1;
};
