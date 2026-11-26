#include "backend.h"

#include "accounts.h"
#include "catalog.h"
#include "content.h"
#include "instances.h"
#include "java.h"
#include "launcher.h"
#include "loaders.h"
#include "modpacks.h"
#include "mojang.h"
#include "paths.h"
#include "settings.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QGuiApplication>
#include <QUrl>

Backend::Backend(QObject *parent) : QObject(parent) {
    QDir().mkpath(Paths::root());
    m_settings = new Settings(this);
    m_accounts = new Accounts(m_settings, this);
    m_instances = new Instances(this);
    m_manifest = new Mojang::Manifest(this);
    m_java = new JavaRuntimes(this);
    m_loaders = new Loaders(this);
    m_launcher = new Launcher(m_settings, m_accounts, m_instances, m_manifest, m_java, m_loaders, this);
    m_catalog = new Catalog(m_settings, this);
    m_content = new Content(m_settings, m_instances, this);
    m_modpacks = new Modpacks(m_settings, m_instances, this);

    connect(m_manifest, &Mojang::Manifest::changed, this, &Backend::gameVersionsChanged);
    connect(m_launcher, &Launcher::failed, this, [this](const QString &, const QString &message) {
        emit notice(message, "error");
    });
    connect(m_launcher, &Launcher::exited, this, [this](const QString &id, int code, bool crashed) {
        if (!crashed) return;
        Instance i;
        const QString name = m_instances->find(id, &i) ? i.name : id;
        emit notice(tr("%1 crashed (exit %2). The log has the details.").arg(name).arg(code), "error");
    });
    connect(m_content, &Content::installed, this, [this](const QString &, const QString &title, int extra) {
        emit notice(extra > 0 ? tr("Installed %1 and %n dependencies", "", extra).arg(title) : tr("Installed %1").arg(title), "ok");
    });
    connect(m_content, &Content::installFailed, this, [this](const QString &title, const QString &error, const QVariantList &blocked) {
        if (blocked.isEmpty())
            emit notice(tr("Couldn't install %1: %2").arg(title, error), "error");
    });
    connect(m_modpacks, &Modpacks::imported, this, [this](const QString &, const QString &name) {
        emit notice(tr("%1 is ready to play").arg(name), "ok");
    });
    connect(m_modpacks, &Modpacks::failed, this, [this](const QString &name, const QString &error, const QVariantList &blocked) {
        if (blocked.isEmpty())
            emit notice(tr("Couldn't install %1: %2").arg(name, error), "error");
    });
    connect(m_accounts, &Accounts::signedIn, this, [this](const QString &name) {
        emit notice(tr("Signed in as %1").arg(name), "ok");
    });
}

Backend::~Backend() = default;

QVariantList Backend::gameVersions() const {
    QVariantList out;
    for (const Mojang::ManifestEntry &e : m_manifest->entries())
        out.append(QVariantMap{{"id", e.id}, {"type", e.type}, {"date", e.released}});
    return out;
}

QString Backend::latestRelease() const { return m_manifest->latestRelease(); }
QString Backend::dataPath() const { return Paths::root(); }
QString Backend::version() const { return QCoreApplication::applicationVersion(); }

void Backend::loadGameVersions(bool force) {
    m_manifest->load([this](bool ok, const QString &error) {
        if (!ok) emit notice(error, "error");
        emit gameVersionsChanged();
    }, force);
}

void Backend::listLoaderVersions(const QString &loader, const QString &minecraft) {
    m_loaders->list(loader, minecraft, [=](bool ok, const QList<Loaders::LoaderVersion> &versions, const QString &error) {
        QVariantList out;
        for (const auto &v : versions)
            out.append(QVariantMap{{"version", v.version}, {"stable", v.stable}});
        emit loaderVersionsReady(loader, minecraft, out, ok ? QString() : error);
    });
}

QString Backend::createInstance(const QString &name, const QString &minecraft, const QString &loader,
                                const QString &loaderVersion) {
    const QString id = m_instances->create(name.trimmed().isEmpty() ? minecraft : name, minecraft, loader, loaderVersion);
    if (id.isEmpty())
        emit notice(tr("Couldn't create the instance folder in %1").arg(Paths::instances()), "error");
    return id;
}

void Backend::openFolder(const QString &path) const {
    QDir().mkpath(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void Backend::openUrl(const QString &url) const {
    const QUrl u(url);
    // Only web links leave the app; anything else in a description is ignored.
    if (u.scheme() == "https" || u.scheme() == "http")
        QDesktopServices::openUrl(u);
}

void Backend::copyText(const QString &text) const {
    if (auto *clipboard = QGuiApplication::clipboard())
        clipboard->setText(text);
}

QString Backend::loaderName(const QString &loader) const { return Loaders::displayName(loader); }
