#include "loaders.h"

#include "downloads.h"
#include "http.h"
#include "mojang.h"
#include "paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QSet>
#include <QVersionNumber>

Loaders::Loaders(QObject *parent) : QObject(parent) {}

QString Loaders::displayName(const QString &loader) {
    if (loader == "fabric") return "Fabric";
    if (loader == "quilt") return "Quilt";
    if (loader == "forge") return "Forge";
    if (loader == "neoforge") return "NeoForge";
    return "Vanilla";
}

bool Loaders::neoForgeMatches(const QString &neo, const QString &minecraft) {
    QStringList mc = minecraft.split('.');
    if (mc.value(0) == "1") {
        // 1.21.1 → 21.1, and 1.21 → 21.0
        mc.removeFirst();
        if (mc.size() == 1)
            mc.append("0");
        return neo.startsWith(mc.join('.') + ".");
    }
    // Since the year-numbered releases (26.1), NeoForge repeats the full game
    // version and adds a build: 26.1.0.x for 26.1.
    while (mc.size() < 3)
        mc.append("0");
    return neo.startsWith(mc.join('.') + ".");
}

QString Loaders::expectedVersionId(const QString &loader, const QString &mc, const QString &v) {
    if (loader == "fabric") return "fabric-loader-" + v + "-" + mc;
    if (loader == "quilt") return "quilt-loader-" + v + "-" + mc;
    if (loader == "neoforge") return "neoforge-" + v;
    if (loader == "forge") {
        // Forge's list carries "1.20.1-47.3.0"; its installer writes 1.20.1-forge-47.3.0.
        const QString build = v.startsWith(mc + "-") ? v.mid(mc.size() + 1) : v;
        return mc + "-forge-" + build;
    }
    return mc;
}

static bool newerFirst(const Loaders::LoaderVersion &a, const Loaders::LoaderVersion &b) {
    auto key = [](const QString &s) {
        qsizetype suffix = 0;
        return QVersionNumber::fromString(s, &suffix);
    };
    return QVersionNumber::compare(key(a.version), key(b.version)) > 0;
}

void Loaders::list(const QString &loader, const QString &mc, ListCallback done) {
    if (loader == "fabric" || loader == "quilt") {
        const QString url = loader == "fabric" ? "https://meta.fabricmc.net/v2/versions/loader/" + mc
                                               : "https://meta.quiltmc.org/v3/versions/loader/" + mc;
        return Http::getJson(this, QUrl(url), [=](bool ok, const QJsonDocument &json, const QString &error, int) {
            if (!ok)
                return done(false, {}, tr("Couldn't list %1 versions: %2").arg(displayName(loader), error));
            QList<LoaderVersion> out;
            for (const QJsonValue &v : json.array()) {
                const QJsonObject l = v.toObject().value("loader").toObject();
                const QString version = l.value("version").toString();
                // Quilt marks betas in the version string rather than a flag.
                const bool stable = l.contains("stable") ? l.value("stable").toBool() : !version.contains("beta");
                out.append({version, stable});
            }
            if (out.isEmpty())
                return done(false, {}, tr("%1 doesn't support Minecraft %2 yet").arg(displayName(loader), mc));
            done(true, out, {});
        });
    }
    if (loader == "neoforge") {
        return Http::getJson(this, QUrl("https://maven.neoforged.net/api/maven/versions/releases/net/neoforged/neoforge"),
                             [=](bool ok, const QJsonDocument &json, const QString &error, int) {
            if (!ok)
                return done(false, {}, tr("Couldn't list NeoForge versions: %1").arg(error));
            QList<LoaderVersion> out;
            for (const QJsonValue &v : json.object().value("versions").toArray()) {
                const QString version = v.toString();
                if (neoForgeMatches(version, mc))
                    out.append({version, !version.contains("beta") && !version.contains("alpha")});
            }
            std::stable_sort(out.begin(), out.end(), newerFirst);
            if (out.isEmpty())
                return done(false, {}, tr("NeoForge doesn't support Minecraft %1").arg(mc));
            done(true, out, {});
        });
    }
    if (loader == "forge") {
        return Http::getJson(this, QUrl("https://files.minecraftforge.net/net/minecraftforge/forge/maven-metadata.json"),
                             [=](bool ok, const QJsonDocument &json, const QString &error, int) {
            if (!ok)
                return done(false, {}, tr("Couldn't list Forge versions: %1").arg(error));
            QList<LoaderVersion> out;
            for (const QJsonValue &v : json.object().value(mc).toArray())
                out.append({v.toString(), true});
            std::reverse(out.begin(), out.end());
            if (out.isEmpty())
                return done(false, {}, tr("Forge doesn't support Minecraft %1").arg(mc));
            done(true, out, {});
        });
    }
    done(false, {}, tr("Unknown loader %1").arg(loader));
}

void Loaders::install(const QString &loader, const QString &mc, const QString &v, const QString &javaPath,
                      InstallCallback done, Log log) {
    const QString expected = expectedVersionId(loader, mc, v);
    if (loader == "vanilla")
        return done(true, mc, {});
    // Already installed by an earlier launch: nothing to run.
    if (QFileInfo::exists(Mojang::versionJsonPath(expected)))
        return done(true, expected, {});
    if (loader == "fabric")
        return installProfile("https://meta.fabricmc.net/v2/versions/loader/" + mc + "/" + v + "/profile/json", expected, done);
    if (loader == "quilt")
        return installProfile("https://meta.quiltmc.org/v3/versions/loader/" + mc + "/" + v + "/profile/json", expected, done);
    if (loader == "neoforge")
        return runInstaller("https://maven.neoforged.net/releases/net/neoforged/neoforge/" + v + "/neoforge-" + v + "-installer.jar",
                            "neoforge-" + v + "-installer.jar", javaPath, expected, done, log);
    if (loader == "forge")
        return runInstaller("https://maven.minecraftforge.net/net/minecraftforge/forge/" + v + "/forge-" + v + "-installer.jar",
                            "forge-" + v + "-installer.jar", javaPath, expected, done, log);
    done(false, {}, tr("Unknown loader %1").arg(loader));
}

void Loaders::installProfile(const QString &url, const QString &expected, InstallCallback done) {
    Http::getData(this, QUrl(url), [=](bool ok, const QByteArray &data, const QString &error, int) {
        if (!ok)
            return done(false, {}, tr("Couldn't fetch the loader profile: %1").arg(error));
        const QJsonObject profile = QJsonDocument::fromJson(data).object();
        const QString id = profile.value("id").toString(expected);
        if (id.isEmpty() || id.contains('/') || id.contains(".."))
            return done(false, {}, tr("The loader profile has no usable id"));
        Paths::writeAtomically(Mojang::versionJsonPath(id), data);
        done(true, id, {});
    });
}

void Loaders::runInstaller(const QString &url, const QString &cacheName, const QString &javaPath,
                           const QString &expected, InstallCallback done, Log log) {
    const QString installer = Paths::cache() + "/installers/" + cacheName;
    auto *batch = new DownloadBatch(this);
    batch->add(QUrl(url), installer);
    connect(batch, &DownloadBatch::finished, this, [=](bool ok, const QString &error) {
        batch->deleteLater();
        if (!ok)
            return done(false, {}, tr("Couldn't download the installer: %1").arg(error));
        // The installers refuse to run without the official launcher's
        // profile file, though they only append to it.
        const QString profiles = Paths::root() + "/launcher_profiles.json";
        if (!QFileInfo::exists(profiles))
            Paths::writeAtomically(profiles, R"({"profiles":{},"selectedProfile":null})");
        const QStringList before = QDir(Paths::versions()).entryList(QDir::Dirs | QDir::NoDotAndDotDot);

        auto *process = new QProcess(this);
        process->setProcessChannelMode(QProcess::MergedChannels);
        process->setWorkingDirectory(Paths::cache());
        connect(process, &QProcess::readyRead, this, [process, log] {
            while (process->canReadLine()) {
                const QString line = QString::fromUtf8(process->readLine()).trimmed();
                if (log && !line.isEmpty())
                    log(line);
            }
        });
        connect(process, &QProcess::errorOccurred, this, [=](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart) {
                process->deleteLater();
                done(false, {}, tr("Couldn't start Java to run the installer"));
            }
        });
        connect(process, &QProcess::finished, this, [=](int code, QProcess::ExitStatus status) {
            process->deleteLater();
            if (QFileInfo::exists(Mojang::versionJsonPath(expected)))
                return done(true, expected, {});
            // Older installers name their version differently; take whatever new one appeared.
            const QStringList after = QDir(Paths::versions()).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString &id : after)
                if (!before.contains(id) && QFileInfo::exists(Mojang::versionJsonPath(id)))
                    return done(true, id, {});
            if (status != QProcess::NormalExit || code != 0)
                return done(false, {}, tr("The installer failed (exit %1). Its output is in the log.").arg(code));
            done(false, {}, tr("The installer finished without installing a version. Very old Forge builds can't be "
                               "installed automatically."));
        });
        process->start(javaPath, {"-jar", installer, "--installClient", Paths::root()});
    });
    batch->start();
}
