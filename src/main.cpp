// lode — a Minecraft launcher with Modrinth and CurseForge built in.

#include <QGuiApplication>
#include <QIcon>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlNetworkAccessManagerFactory>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QUrl>

#include "accounts.h"
#include "backend.h"
#include "catalog.h"
#include "cli.h"
#include "content.h"
#include "instances.h"
#include "launcher.h"
#include "logmodel.h"
#include "modpacks.h"
#include "paths.h"
#include "settings.h"
#include "theme.h"
#include "translator.h"

// Project icons and gallery images are kept on disk, so scrolling back
// through results or reopening the app never fetches them twice.
class CachedNetworkFactory : public QQmlNetworkAccessManagerFactory {
public:
    QNetworkAccessManager *create(QObject *parent) override {
        auto *manager = new QNetworkAccessManager(parent);
        auto *cache = new QNetworkDiskCache(manager);
        cache->setCacheDirectory(Paths::cache() + "/images");
        cache->setMaximumCacheSize(256 * 1024 * 1024);
        manager->setCache(cache);
        manager->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
        return manager;
    }
};

int main(int argc, char *argv[]) {
    QStringList arguments;
    for (int i = 0; i < argc; ++i)
        arguments.append(QString::fromLocal8Bit(argv[i]));

    // Commands for scripts and agents never need a display.
    if (!Cli::wantsWindow(arguments)) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        // A desktop theme plugin (GTK's, say) would try to reach a display.
        qunsetenv("QT_QPA_PLATFORMTHEME");
        QGuiApplication app(argc, argv);
        app.setApplicationName("lode");
        app.setApplicationVersion("0.1.0");
        return Cli::run(arguments);
    }

    qputenv("QT_QUICK_CONTROLS_MATERIAL_VARIANT", "Dense");
    QGuiApplication app(argc, argv);
    app.setApplicationName("lode");
    app.setApplicationVersion("0.1.0");

    // Matches the window to lode.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("lode");
    app.setWindowIcon(QIcon::fromTheme("lode"));

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Backend backend;

    QQmlApplicationEngine engine;
    CachedNetworkFactory networkFactory;
    engine.setNetworkAccessManagerFactory(&networkFactory);
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    auto *context = engine.rootContext();
    context->setContextProperty("translator", &translator);
    context->setContextProperty("theme", &theme);
    context->setContextProperty("backend", &backend);
    context->setContextProperty("settings", backend.settings());
    context->setContextProperty("accounts", backend.accounts());
    context->setContextProperty("instances", backend.instances());
    context->setContextProperty("launcher", backend.launcher());
    context->setContextProperty("catalog", backend.catalog());
    context->setContextProperty("content", backend.content());
    context->setContextProperty("modpacks", backend.modpacks());
    qmlRegisterUncreatableType<LogModel>("Lode", 1, 0, "LogModel", "Logs come from launcher.log()");

    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    // "lode open <file.mrpack>" or "lode <file.mrpack>" imports a pack.
    const QStringList rest = arguments.mid(1);
    const QString file = rest.value(0) == "open" ? rest.value(1) : rest.value(0);
    if (!file.isEmpty() && (file.endsWith(".mrpack") || file.endsWith(".zip")))
        backend.modpacks()->importFile(file);

    return app.exec();
}
