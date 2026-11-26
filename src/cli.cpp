#include "cli.h"

#include "accounts.h"
#include "backend.h"
#include "instances.h"
#include "launcher.h"
#include "loaders.h"
#include "logmodel.h"
#include "paths.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

static const QStringList kCommands{"list", "create", "launch", "prepare", "skill", "help", "--help", "-h", "--version"};

bool Cli::wantsWindow(const QStringList &arguments) {
    return arguments.size() < 2 || !kCommands.contains(arguments.at(1));
}

static QTextStream &out() { static QTextStream s(stdout); return s; }
static QTextStream &err() { static QTextStream s(stderr); return s; }

static QString option(const QStringList &args, const QString &name, const QString &fallback = {}) {
    const int i = args.indexOf(name);
    return i >= 0 && i + 1 < args.size() ? args.at(i + 1) : fallback;
}

static const char *kSkill = R"(---
name: lode
description: Create, list and launch Minecraft instances with the lode launcher. Use when asked to start Minecraft, make a modded instance, or see which instances exist.
---

# lode

- `lode list --json` prints every instance: id, name, minecraft, loader, loaderVersion, lastPlayed.
- `lode create "<name>" --minecraft 1.21.1 --loader fabric [--loader-version 0.16.9] [--json]` makes an instance. Without --loader-version the newest stable build is used. Loaders: vanilla, fabric, quilt, forge, neoforge.
- `lode prepare <id or name>` downloads everything without starting the game.
- `lode launch <id or name>` downloads anything missing and starts the game with the active account, then exits once it's running.
- Mods, packs and accounts are managed in the window: `lode` opens it.

Every command exits 0 on success and 1 with the reason on stderr.
)";

static int help() {
    out() << "lode " << QCoreApplication::applicationVersion() << " — Minecraft launcher\n\n"
          << "  lode                         open the window\n"
          << "  lode <pack.mrpack|pack.zip>  open the window and import a modpack\n"
          << "  lode list [--json]           list instances\n"
          << "  lode create <name> --minecraft <version> [--loader <loader>] [--loader-version <v>] [--json]\n"
          << "  lode prepare <id or name>    download everything an instance needs, without starting it\n"
          << "  lode launch <id or name>     prepare and start an instance\n"
          << "  lode skill install           install the agent skill in ~/.claude/skills/lode\n";
    out().flush();
    return 0;
}

static QJsonObject describe(const QVariantMap &i) {
    return {{"id", i.value("instanceId").toString()}, {"name", i.value("name").toString()},
            {"minecraft", i.value("minecraft").toString()}, {"loader", i.value("loader").toString()},
            {"loaderVersion", i.value("loaderVersion").toString()},
            {"lastPlayed", i.value("lastPlayed").toDateTime().toString(Qt::ISODate)},
            {"folder", i.value("folder").toString()}};
}

int Cli::run(const QStringList &args) {
    const QString command = args.value(1);
    const bool json = args.contains("--json");
    if (command == "--version") {
        out() << QCoreApplication::applicationVersion() << "\n";
        return 0;
    }
    if (command == "help" || command == "--help" || command == "-h")
        return help();

    if (command == "skill") {
        if (args.value(2) != "install") {
            err() << "usage: lode skill install\n";
            return 1;
        }
        const QString path = QDir::homePath() + "/.claude/skills/lode/SKILL.md";
        if (!Paths::writeAtomically(path, kSkill)) {
            err() << "couldn't write " << path << "\n";
            return 1;
        }
        out() << "Installed " << path << "\n";
        return 0;
    }

    Backend backend;
    Instances *instances = backend.instances();

    if (command == "list") {
        QJsonArray list;
        for (int row = 0; row < instances->rowCount(); ++row)
            list.append(describe(instances->get(instances->idAt(row))));
        if (json) {
            out() << QJsonDocument(list).toJson();
        } else {
            for (const QJsonValue &v : list) {
                const QJsonObject o = v.toObject();
                out() << o.value("id").toString() << "\t" << o.value("minecraft").toString() << "\t"
                      << o.value("loader").toString() << "\t" << o.value("name").toString() << "\n";
            }
        }
        return 0;
    }

    if (command == "create") {
        const QString name = args.value(2);
        const QString minecraft = option(args, "--minecraft");
        const QString loader = option(args, "--loader", "vanilla");
        QString loaderVersion = option(args, "--loader-version");
        if (name.isEmpty() || name.startsWith("--") || minecraft.isEmpty()) {
            err() << "usage: lode create <name> --minecraft <version> [--loader <loader>] [--loader-version <v>]\n";
            return 1;
        }
        if (!QStringList{"vanilla", "fabric", "quilt", "forge", "neoforge"}.contains(loader)) {
            err() << "unknown loader " << loader << "\n";
            return 1;
        }
        if (loader != "vanilla" && loaderVersion.isEmpty()) {
            QEventLoop loop;
            QString error;
            Loaders loaders;
            loaders.list(loader, minecraft, [&](bool ok, const QList<Loaders::LoaderVersion> &versions, const QString &e) {
                error = e;
                for (const auto &v : versions)
                    if (v.stable) { loaderVersion = v.version; break; }
                if (ok && loaderVersion.isEmpty() && !versions.isEmpty())
                    loaderVersion = versions.first().version;
                loop.quit();
            });
            loop.exec();
            if (loaderVersion.isEmpty()) {
                err() << error << "\n";
                return 1;
            }
        }
        const QString id = backend.createInstance(name, minecraft, loader, loaderVersion);
        if (id.isEmpty()) {
            err() << "couldn't create the instance\n";
            return 1;
        }
        if (json)
            out() << QJsonDocument(describe(instances->get(id))).toJson();
        else
            out() << "Created " << id << "\n";
        return 0;
    }

    if (command == "launch" || command == "prepare") {
        const QString wanted = args.value(2);
        QString id;
        for (int row = 0; row < instances->rowCount(); ++row) {
            const QVariantMap i = instances->get(instances->idAt(row));
            if (i.value("instanceId").toString() == wanted || i.value("name").toString() == wanted)
                id = i.value("instanceId").toString();
        }
        if (id.isEmpty()) {
            err() << "no instance called " << wanted << "\n";
            return 1;
        }
        Launcher *launcher = backend.launcher();
        QEventLoop loop;
        int code = 0;
        QString lastStatus;
        QObject::connect(launcher, &Launcher::changed, &loop, [&] {
            if (launcher->status() != lastStatus && !launcher->status().isEmpty()) {
                lastStatus = launcher->status();
                if (!lastStatus.contains("(")) err() << lastStatus << "\n";
            }
        });
        QObject::connect(launcher, &Launcher::failed, &loop, [&](const QString &, const QString &message) {
            err() << message << "\n";
            code = 1;
            loop.quit();
        });
        QObject::connect(launcher, &Launcher::started, &loop, [&] { loop.quit(); });
        if (command == "prepare") {
            launcher->prepare(id, [&](bool ok, const QString &) {
                // Launcher::failed has already printed the reason.
                if (!ok) code = 1;
                loop.quit();
            });
        } else {
            launcher->launch(id);
        }
        loop.exec();
        if (code == 0)
            out() << (command == "prepare" ? "Ready " : "Started ") << id << "\n";
        return code;
    }

    return help();
}
