#include "launcher.h"

#include "accounts.h"
#include "downloads.h"
#include "instances.h"
#include "java.h"
#include "loaders.h"
#include "logmodel.h"
#include "paths.h"
#include "settings.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QProcess>
#include <QtConcurrent>

struct Launcher::Job {
    Instance instance;
    Mojang::Version version;
    QString javaPath;
    bool launchAfter = true;
    Done done;
    QPointer<DownloadBatch> batch;
    QPointer<QProcess> installer;
    bool cancelled = false;
};

Launcher::Launcher(Settings *settings, Accounts *accounts, Instances *instances, Mojang::Manifest *manifest,
                   JavaRuntimes *java, Loaders *loaders, QObject *parent)
    : QObject(parent), m_settings(settings), m_accounts(accounts), m_instances(instances), m_manifest(manifest),
      m_java(java), m_loaders(loaders) {}

Launcher::~Launcher() {
    // Games keep running after the launcher closes, like any other launcher.
    for (QProcess *p : std::as_const(m_processes)) {
        p->disconnect(this);
        p->setParent(nullptr);
    }
    delete m_job;
}

LogModel *Launcher::log(const QString &id) {
    if (!m_logs.contains(id))
        m_logs.insert(id, new LogModel(this));
    return m_logs.value(id);
}

QString Launcher::stateOf(const QString &id) const {
    if (m_processes.contains(id)) return "running";
    if (m_preparing == id) return "preparing";
    return "idle";
}

void Launcher::step(const QString &status, double progress) {
    m_status = status;
    m_progress = progress;
    if (!m_preparing.isEmpty() && !status.isEmpty())
        log(m_preparing)->append("[lode] " + status);
    emit changed();
}

void Launcher::launch(const QString &id) {
    if (m_processes.contains(id) || !m_preparing.isEmpty())
        return;
    prepare(id, {});
    if (m_job)
        m_job->launchAfter = true;
}

void Launcher::cancel() {
    if (!m_job)
        return;
    m_job->cancelled = true;
    if (m_job->batch)
        m_job->batch->cancel();
    if (m_job->installer)
        m_job->installer->kill();
    finishPrepare(false, tr("Cancelled"));
}

void Launcher::kill(const QString &id) {
    if (QProcess *p = m_processes.value(id))
        p->kill();
}

void Launcher::fail(const QString &message) { finishPrepare(false, message); }

void Launcher::finishPrepare(bool ok, const QString &error) {
    if (!m_job)
        return;
    Job *job = m_job;
    m_job = nullptr;
    const QString id = m_preparing;
    m_preparing.clear();
    m_status.clear();
    m_progress = -1;
    if (!ok && !job->cancelled) {
        log(id)->append("[lode] " + error);
        emit failed(id, error);
    }
    emit changed();
    if (job->done)
        job->done(ok, error);
    delete job;
}

void Launcher::prepare(const QString &id, Done done) {
    Instance instance;
    if (!m_instances->find(id, &instance)) {
        if (done) done(false, tr("That instance is gone"));
        return;
    }
    if (m_job) {
        if (done) done(false, tr("Another instance is still getting ready"));
        return;
    }
    m_job = new Job;
    m_job->instance = instance;
    m_job->launchAfter = false;
    m_job->done = done;
    m_preparing = id;
    log(id)->clear();
    Job *job = m_job;
    auto alive = [this, job] { return m_job == job && !job->cancelled; };

    step(tr("Checking Minecraft %1").arg(instance.minecraft), -1);
    m_manifest->ensureVersion(instance.minecraft, [=](bool ok, const QString &error) {
        if (!alive()) return;
        if (!ok) return fail(error);
        Mojang::Version vanilla;
        QString resolveError;
        if (!Mojang::resolve(instance.minecraft, &vanilla, &resolveError))
            return fail(resolveError);

        step(tr("Checking Java"), -1);
        m_java->ensure(vanilla.javaComponent, [=](bool ok, const QString &javaPath, const QString &error) {
            if (!alive()) return;
            if (!ok) return fail(error);
            job->javaPath = javaPath;

            if (instance.loader != "vanilla")
                step(tr("Installing %1 %2").arg(Loaders::displayName(instance.loader), instance.loaderVersion), -1);
            m_loaders->install(instance.loader, instance.minecraft, instance.loaderVersion, javaPath,
                               [=](bool ok, const QString &versionId, const QString &error) {
                if (!alive()) return;
                if (!ok) return fail(error);
                if (job->instance.versionId != versionId) {
                    job->instance.versionId = versionId;
                    Instance fresh;
                    if (m_instances->find(instance.id, &fresh)) {
                        fresh.versionId = versionId;
                        m_instances->update(fresh);
                    }
                }
                QString resolveError;
                if (!Mojang::resolve(versionId, &job->version, &resolveError))
                    return fail(resolveError);

                ensureFiles(job, [=](bool ok, const QString &error) {
                    if (!alive()) return;
                    if (!ok) return fail(error);
                    if (!job->launchAfter)
                        return finishPrepare(true, {});
                    step(tr("Signing in"), -1);
                    m_accounts->activeAccount([=](bool ok, const Account &account, const QString &error) {
                        if (!alive()) return;
                        if (!ok) return fail(error);
                        start(job, account);
                    });
                });
            }, [this, id](const QString &line) { log(id)->append(line); });
        }, [this, alive](double fraction, const QString &status) {
            if (alive()) step(status, fraction);
        });
    });
}

void Launcher::ensureFiles(Job *job, Done done) {
    const Mojang::Version &v = job->version;
    auto *batch = new DownloadBatch(this);
    job->batch = batch;

    if (!v.jarVersion.isEmpty())
        batch->add(v.clientUrl, Mojang::clientJarPath(v.jarVersion), v.clientSha1, v.clientSize);
    QStringList missingSources;
    for (const Mojang::Library &lib : v.libraries) {
        const QString path = Paths::libraries() + "/" + lib.path;
        if (lib.url.isEmpty()) {
            // Forge's processed jars come from its installer, not a URL.
            if (!QFileInfo::exists(path))
                missingSources.append(lib.name);
            continue;
        }
        batch->add(lib.url, path, lib.sha1, lib.size);
    }
    if (!missingSources.isEmpty()) {
        batch->deleteLater();
        return done(false, tr("Missing files the loader installer should have made: %1").arg(missingSources.join(", ")));
    }
    if (v.assetIndexUrl.isValid())
        batch->add(v.assetIndexUrl, Paths::assets() + "/indexes/" + v.assets + ".json", v.assetIndexSha1, v.assetIndexSize);

    step(tr("Downloading libraries"), 0);
    connect(batch, &DownloadBatch::progress, this, [this, job](qint64 bytes, qint64 total, int files, int count) {
        if (m_job != job) return;
        m_status = tr("Downloading libraries (%1 of %2)").arg(files).arg(count);
        m_progress = total > 0 ? double(bytes) / total : count > 0 ? double(files) / count : -1;
        emit changed();
    });
    connect(batch, &DownloadBatch::finished, this, [=](bool ok, const QString &error) {
        batch->deleteLater();
        if (m_job != job) return;
        if (!ok) return done(false, error);
        ensureAssets(job, done);
    });
    batch->start();
}

void Launcher::ensureAssets(Job *job, Done done) {
    const Mojang::Version v = job->version;
    const QJsonObject index = QJsonDocument::fromJson(Paths::readAll(Paths::assets() + "/indexes/" + v.assets + ".json")).object();
    const QJsonObject objects = index.value("objects").toObject();
    auto *batch = new DownloadBatch(this);
    job->batch = batch;
    for (auto it = objects.begin(); it != objects.end(); ++it) {
        const QJsonObject o = it.value().toObject();
        const QString hash = o.value("hash").toString();
        if (hash.size() < 2) continue;
        DownloadBatch::Item item;
        item.url = QUrl("https://resources.download.minecraft.net/" + hash.left(2) + "/" + hash);
        item.path = Paths::assets() + "/objects/" + hash.left(2) + "/" + hash;
        item.sha1 = hash;
        item.size = o.value("size").toInteger(-1);
        item.trustSize = true;
        batch->add(item);
    }
    step(tr("Downloading game assets"), 0);
    connect(batch, &DownloadBatch::progress, this, [this, job](qint64 bytes, qint64 total, int files, int count) {
        if (m_job != job) return;
        m_status = tr("Downloading game assets (%1 of %2)").arg(files).arg(count);
        m_progress = total > 0 ? double(bytes) / total : -1;
        emit changed();
    });
    connect(batch, &DownloadBatch::finished, this, [=](bool ok, const QString &error) {
        batch->deleteLater();
        if (m_job != job) return;
        if (!ok) return done(false, error);

        step(tr("Unpacking natives"), -1);
        const bool isVirtual = index.value("virtual").toBool();
        const bool mapToResources = index.value("map_to_resources").toBool();
        const QString gameDir = job->instance.gameDir();
        const QString nativesDir = job->instance.folder() + "/natives";
        const QString jarVersion = v.jarVersion, versionId = v.id;
        QList<Mojang::Library> natives;
        for (const Mojang::Library &lib : v.libraries)
            if (lib.native)
                natives.append(lib);

        // Copying legacy assets and unpacking natives touches a lot of small
        // files, so it runs off the UI thread.
        auto *watcher = new QFutureWatcher<QString>(this);
        connect(watcher, &QFutureWatcher<QString>::finished, this, [=] {
            watcher->deleteLater();
            if (m_job != job) return;
            const QString error = watcher->result();
            done(error.isEmpty(), error);
        });
        watcher->setFuture(QtConcurrent::run([=]() -> QString {
            // Pre-1.7 games read assets by name, not hash.
            if (isVirtual || mapToResources) {
                const QString target = mapToResources ? gameDir + "/resources" : Paths::assets() + "/virtual/" + v.assets;
                for (auto it = objects.begin(); it != objects.end(); ++it) {
                    const QString hash = it.value().toObject().value("hash").toString();
                    const QString to = target + "/" + it.key();
                    if (it.key().contains("..") || QFileInfo::exists(to)) continue;
                    QDir().mkpath(QFileInfo(to).absolutePath());
                    QFile::copy(Paths::assets() + "/objects/" + hash.left(2) + "/" + hash, to);
                }
            }
            QDir(nativesDir).removeRecursively();
            QDir().mkpath(nativesDir);
            for (const Mojang::Library &lib : natives) {
                QStringList args{"-xf", Paths::libraries() + "/" + lib.path, "-C", nativesDir, "--exclude", "META-INF"};
                for (const QString &e : lib.exclude)
                    args << "--exclude" << QString(e).remove(QRegularExpression("/$"));
                if (QProcess::execute("bsdtar", args) != 0)
                    return QCoreApplication::translate("Launcher", "Couldn't unpack %1").arg(lib.name);
            }
            // The official launcher gives an inheriting version its own copy of
            // the game jar, and Forge's module setup expects to find it there.
            if (!jarVersion.isEmpty() && jarVersion != versionId) {
                const QString from = Mojang::clientJarPath(jarVersion), to = Mojang::clientJarPath(versionId);
                if (QFileInfo(to).size() != QFileInfo(from).size()) {
                    QFile::remove(to);
                    QDir().mkpath(QFileInfo(to).absolutePath());
                    if (!QFile::copy(from, to))
                        return QCoreApplication::translate("Launcher", "Couldn't copy the game jar for %1").arg(versionId);
                }
            }
            return {};
        }));
    });
    batch->start();
}

QString Launcher::substitute(const QString &text, const QHash<QString, QString> &vars) {
    QString out;
    out.reserve(text.size());
    int i = 0;
    while (i < text.size()) {
        const int open = text.indexOf("${", i);
        if (open < 0) { out += text.mid(i); break; }
        const int close = text.indexOf('}', open);
        if (close < 0) { out += text.mid(i); break; }
        out += text.mid(i, open - i);
        const QString key = text.mid(open + 2, close - open - 2);
        out += vars.contains(key) ? vars.value(key) : text.mid(open, close - open + 1);
        i = close + 1;
    }
    return out;
}

static void appendArguments(const QJsonArray &list, const QHash<QString, bool> &features,
                            const QHash<QString, QString> &vars, QStringList *out) {
    for (const QJsonValue &entry : list) {
        if (entry.isString()) {
            out->append(Launcher::substitute(entry.toString(), vars));
            continue;
        }
        const QJsonObject o = entry.toObject();
        if (!Mojang::rulesAllow(o.value("rules").toArray(), features))
            continue;
        const QJsonValue value = o.value("value");
        if (value.isString())
            out->append(Launcher::substitute(value.toString(), vars));
        else
            for (const QJsonValue &v : value.toArray())
                out->append(Launcher::substitute(v.toString(), vars));
    }
}

QStringList Launcher::arguments(const Mojang::Version &v, const Context &c) {
    const QHash<QString, QString> vars{
        {"auth_player_name", c.playerName}, {"version_name", c.versionName}, {"game_directory", c.gameDir},
        {"assets_root", c.assetsRoot}, {"game_assets", c.gameAssets}, {"assets_index_name", v.assets},
        {"auth_uuid", c.uuid}, {"auth_access_token", c.accessToken}, {"auth_session", c.accessToken},
        {"clientid", c.clientId}, {"auth_xuid", c.xuid}, {"user_type", c.userType}, {"version_type", v.type},
        {"user_properties", "{}"}, {"natives_directory", c.nativesDir}, {"launcher_name", "lode"},
        {"launcher_version", QCoreApplication::applicationVersion()}, {"classpath", c.classpath},
        {"classpath_separator", ":"}, {"library_directory", c.librariesDir},
        {"resolution_width", QString::number(c.width)}, {"resolution_height", QString::number(c.height)},
    };
    const QHash<QString, bool> features{{"has_custom_resolution", c.width > 0 && c.height > 0}};

    QStringList args;
    args << QString("-Xmx%1M").arg(c.memoryMb);
    // Old log4j versions in pre-1.18.1 games are hardened this way too.
    args << "-Dlog4j2.formatMsgNoLookups=true";
    if (v.jvmArguments.isEmpty()) {
        args << "-Djava.library.path=" + c.nativesDir << "-Dminecraft.launcher.brand=lode"
             << "-Dminecraft.launcher.version=" + QCoreApplication::applicationVersion() << "-cp" << c.classpath;
    } else {
        appendArguments(v.jvmArguments, features, vars, &args);
    }
    args << c.extraJvm;
    args << v.mainClass;
    if (!v.gameArguments.isEmpty()) {
        appendArguments(v.gameArguments, features, vars, &args);
    } else {
        for (const QString &part : v.legacyArguments.split(' ', Qt::SkipEmptyParts))
            args << substitute(part, vars);
        if (features.value("has_custom_resolution"))
            args << "--width" << QString::number(c.width) << "--height" << QString::number(c.height);
    }
    return args;
}

void Launcher::start(Job *job, const Account &account) {
    const Mojang::Version &v = job->version;
    const Instance &instance = job->instance;
    QDir().mkpath(instance.gameDir());

    QStringList classpath;
    for (const Mojang::Library &lib : v.libraries)
        if (!lib.native)
            classpath.append(Paths::libraries() + "/" + lib.path);
    // An inheriting version runs its own copy of the jar (see ensureAssets).
    classpath.append(Mojang::clientJarPath(v.id));

    Context c;
    c.versionName = v.id;
    c.gameDir = instance.gameDir();
    c.assetsRoot = Paths::assets();
    c.gameAssets = v.assets == "legacy" || v.assets == "pre-1.6" ? Paths::assets() + "/virtual/" + v.assets : Paths::assets();
    c.nativesDir = instance.folder() + "/natives";
    c.librariesDir = Paths::libraries();
    c.classpath = classpath.join(':');
    c.playerName = account.name;
    c.uuid = account.uuid;
    c.accessToken = account.accessToken;
    c.userType = account.kind == "msa" ? "msa" : "legacy";
    c.xuid = account.xuid.isEmpty() ? "0" : account.xuid;
    c.clientId = m_settings->microsoftClientId();
    c.width = m_settings->windowWidth();
    c.height = m_settings->windowHeight();
    c.memoryMb = instance.memoryMb > 0 ? instance.memoryMb : m_settings->memoryMb();
    c.extraJvm = QProcess::splitCommand(m_settings->javaArgs()) + QProcess::splitCommand(instance.javaArgs);

    QStringList args = arguments(v, c);
    QString program = job->javaPath;
    // A wrapper like gamemoderun or mangohud goes in front of java.
    const QStringList wrapper = QProcess::splitCommand(m_settings->wrapperCommand());
    if (!wrapper.isEmpty()) {
        args.prepend(program);
        args = wrapper.mid(1) + args;
        program = wrapper.first();
    }

    const QString id = instance.id;
    LogModel *logModel = log(id);
    QStringList shown = args;
    for (QString &a : shown)
        if (a == account.accessToken && a.size() > 4)
            a = "<token>";
    logModel->append("[lode] " + program + " " + shown.join(' '));

    auto *process = new QProcess(this);
    process->setProgram(program);
    process->setArguments(args);
    process->setWorkingDirectory(instance.gameDir());
    process->setProcessChannelMode(QProcess::MergedChannels);
    const QDateTime startedAt = QDateTime::currentDateTimeUtc();

    connect(process, &QProcess::readyRead, this, [process, logModel] {
        while (process->canReadLine())
            logModel->append(QString::fromUtf8(process->readLine()).trimmed());
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, id](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart) return;
        m_processes.remove(id);
        process->deleteLater();
        emit failed(id, tr("Couldn't start %1").arg(process->program()));
        emit changed();
    });
    connect(process, &QProcess::finished, this, [this, process, id, startedAt, logModel](int code, QProcess::ExitStatus status) {
        const QByteArray rest = process->readAll();
        if (!rest.isEmpty())
            logModel->append(QString::fromUtf8(rest).trimmed());
        m_processes.remove(id);
        process->deleteLater();
        Instance fresh;
        if (m_instances->find(id, &fresh)) {
            fresh.playSeconds += startedAt.secsTo(QDateTime::currentDateTimeUtc());
            m_instances->update(fresh);
        }
        const bool crashed = status != QProcess::NormalExit || code != 0;
        logModel->append(crashed ? QString("[lode] The game stopped with code %1").arg(code) : QString("[lode] The game closed"));
        emit exited(id, code, crashed);
        emit changed();
    });

    Instance fresh;
    if (m_instances->find(id, &fresh)) {
        fresh.lastPlayed = startedAt;
        m_instances->update(fresh);
    }
    m_processes.insert(id, process);
    finishPrepare(true, {});
    process->start();
    emit started(id);
    emit changed();
}
