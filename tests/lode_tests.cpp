#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include "accounts.h"
#include "content.h"
#include "launcher.h"
#include "loaders.h"
#include "modpacks.h"
#include "mojang.h"
#include "paths.h"
#include "theme.h"
#include "translator.h"

class LodeTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void mavenPaths();
    void uniqueFolders();
    void rules();
    void oldStyleNatives();
    void mavenRepositoryLibraries();
    void resolvesInheritance();
    void buildsArguments();
    void legacyArguments();
    void loaderVersions();
    void modpackSafety();
    void pickVersion();
    void offlineAccounts();
    void themeReadsTonal();
    void themeFallsBackPerKey();
    void translatorFallsBackToEnglish();
    void translatorPicksPluralForms();

public:
    static void write(const QString &path, const QByteArray &contents) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
    void writeVersion(const QString &id, const QByteArray &json) {
        QVERIFY(Paths::writeAtomically(Mojang::versionJsonPath(id), json));
    }

    QTemporaryDir m_home;
};

void LodeTests::initTestCase() {
    QVERIFY(m_home.isValid());
    Paths::setRoot(m_home.path());
}

void LodeTests::mavenPaths() {
    QCOMPARE(Paths::mavenPath("org.lwjgl:lwjgl:3.3.3"), QString("org/lwjgl/lwjgl/3.3.3/lwjgl-3.3.3.jar"));
    QCOMPARE(Paths::mavenPath("org.lwjgl:lwjgl:3.3.3:natives-linux"),
             QString("org/lwjgl/lwjgl/3.3.3/lwjgl-3.3.3-natives-linux.jar"));
    QCOMPARE(Paths::mavenPath("de.oceanlabs.mcp:mcp_config:1.20.1@zip"),
             QString("de/oceanlabs/mcp/mcp_config/1.20.1/mcp_config-1.20.1.zip"));
    QVERIFY(Paths::mavenPath("nonsense").isEmpty());
}

void LodeTests::uniqueFolders() {
    QDir(m_home.path()).mkpath("box/Survival");
    QCOMPARE(Paths::uniqueFolder(m_home.path() + "/box", "Survival"), QString("Survival (2)"));
    QCOMPARE(Paths::uniqueFolder(m_home.path() + "/box", "a/b:c"), QString("a-b-c"));
    QCOMPARE(Paths::uniqueFolder(m_home.path() + "/box", "../.."), QString("instance"));
}

void LodeTests::rules() {
    const auto parse = [](const char *json) { return QJsonDocument::fromJson(json).array(); };
    QVERIFY(Mojang::rulesAllow({}));
    QVERIFY(Mojang::rulesAllow(parse(R"([{"action":"allow"},{"action":"disallow","os":{"name":"osx"}}])")));
    QVERIFY(!Mojang::rulesAllow(parse(R"([{"action":"allow","os":{"name":"windows"}}])")));
    QVERIFY(Mojang::rulesAllow(parse(R"([{"action":"allow","os":{"name":"linux"}}])")));
    const auto demo = parse(R"([{"action":"allow","features":{"is_demo_user":true}}])");
    QVERIFY(!Mojang::rulesAllow(demo));
    QVERIFY(Mojang::rulesAllow(demo, {{"is_demo_user", true}}));
}

void LodeTests::oldStyleNatives() {
    const QJsonObject lib = QJsonDocument::fromJson(R"({
        "name": "org.lwjgl.lwjgl:lwjgl-platform:2.9.4",
        "natives": {"linux": "natives-linux", "windows": "natives-windows"},
        "extract": {"exclude": ["META-INF/"]},
        "downloads": {"classifiers": {"natives-linux": {"path": "x/natives-linux.jar", "url": "https://e/x.jar", "sha1": "ab", "size": 3}}}
    })").object();
    const auto libs = Mojang::librariesFor(lib);
    QCOMPARE(libs.size(), 1);
    QVERIFY(libs.first().native);
    QCOMPARE(libs.first().path, QString("x/natives-linux.jar"));
    QCOMPARE(libs.first().exclude, QStringList{"META-INF/"});
}

void LodeTests::mavenRepositoryLibraries() {
    const QJsonObject lib = QJsonDocument::fromJson(
        R"({"name": "net.fabricmc:fabric-loader:0.16.9", "url": "https://maven.fabricmc.net/"})").object();
    const auto libs = Mojang::librariesFor(lib);
    QCOMPARE(libs.size(), 1);
    QCOMPARE(libs.first().url.toString(),
             QString("https://maven.fabricmc.net/net/fabricmc/fabric-loader/0.16.9/fabric-loader-0.16.9.jar"));
}

void LodeTests::resolvesInheritance() {
    writeVersion("1.0", R"({"id":"1.0","type":"release","mainClass":"net.minecraft.client.main.Main",
        "assetIndex":{"id":"5","url":"https://e/5.json","sha1":"a","size":1},
        "downloads":{"client":{"url":"https://e/c.jar","sha1":"b","size":2}},
        "javaVersion":{"component":"java-runtime-delta","majorVersion":21},
        "arguments":{"game":["--version","${version_name}"],"jvm":["-cp","${classpath}"]},
        "libraries":[{"name":"org.ow2.asm:asm:9.1","downloads":{"artifact":{"path":"asm-9.1.jar","url":"https://e/asm91","sha1":"c","size":1}}},
                     {"name":"com.mojang:brigadier:1.0","downloads":{"artifact":{"path":"brig.jar","url":"https://e/b","sha1":"d","size":1}}}]})");
    writeVersion("loader-1.0", R"({"id":"loader-1.0","inheritsFrom":"1.0","mainClass":"loader.Main",
        "arguments":{"game":["--loader"]},
        "libraries":[{"name":"org.ow2.asm:asm:9.6","url":"https://maven.example/"}]})");

    Mojang::Version v;
    QString error;
    QVERIFY2(Mojang::resolve("loader-1.0", &v, &error), qPrintable(error));
    QCOMPARE(v.mainClass, QString("loader.Main"));
    QCOMPARE(v.jarVersion, QString("1.0"));
    QCOMPARE(v.javaComponent, QString("java-runtime-delta"));
    QCOMPARE(v.assets, QString("5"));
    // The loader's newer ASM wins over vanilla's, and comes first.
    QCOMPARE(v.libraries.size(), 2);
    QCOMPARE(v.libraries[0].name, QString("org.ow2.asm:asm:9.6"));
    QCOMPARE(v.libraries[1].name, QString("com.mojang:brigadier:1.0"));
    // Arguments add up, vanilla's first.
    QCOMPARE(v.gameArguments.size(), 3);
    QCOMPARE(v.gameArguments.last().toString(), QString("--loader"));

    QVERIFY(!Mojang::resolve("missing", &v, &error));
}

void LodeTests::buildsArguments() {
    Mojang::Version v;
    v.mainClass = "net.minecraft.client.main.Main";
    v.assets = "17";
    v.type = "release";
    v.jvmArguments = QJsonDocument::fromJson(R"(["-Djava.library.path=${natives_directory}","-cp","${classpath}",
        {"rules":[{"action":"allow","os":{"name":"osx"}}],"value":"-XstartOnFirstThread"}])").array();
    v.gameArguments = QJsonDocument::fromJson(R"(["--username","${auth_player_name}","--uuid","${auth_uuid}",
        {"rules":[{"action":"allow","features":{"has_custom_resolution":true}}],"value":["--width","${resolution_width}"]},
        {"rules":[{"action":"allow","features":{"is_demo_user":true}}],"value":"--demo"}])").array();
    Launcher::Context c;
    c.playerName = "Steve";
    c.uuid = "abc";
    c.classpath = "a.jar:b.jar";
    c.nativesDir = "/n";
    c.memoryMb = 2048;
    QStringList args = Launcher::arguments(v, c);
    QCOMPARE(args.first(), QString("-Xmx2048M"));
    QVERIFY(args.contains("-Djava.library.path=/n"));
    QVERIFY(!args.contains("-XstartOnFirstThread"));
    QVERIFY(!args.contains("--demo"));
    QVERIFY(!args.contains("--width"));
    QCOMPARE(args.mid(args.indexOf("--username"), 4), (QStringList{"--username", "Steve", "--uuid", "abc"}));
    QVERIFY(args.indexOf(v.mainClass) < args.indexOf("--username"));

    c.width = 1280;
    c.height = 720;
    args = Launcher::arguments(v, c);
    QCOMPARE(args.mid(args.indexOf("--width"), 2), (QStringList{"--width", "1280"}));
}

void LodeTests::legacyArguments() {
    Mojang::Version v;
    v.mainClass = "net.minecraft.client.Minecraft";
    v.legacyArguments = "--username ${auth_player_name} --session ${auth_session}";
    Launcher::Context c;
    c.playerName = "Alex";
    c.accessToken = "tok";
    c.classpath = "x.jar";
    const QStringList args = Launcher::arguments(v, c);
    QVERIFY(args.contains("-cp"));
    QCOMPARE(args.mid(args.indexOf(v.mainClass) + 1), (QStringList{"--username", "Alex", "--session", "tok"}));
    QCOMPARE(Launcher::substitute("${unknown} ${a}", {{"a", "1"}}), QString("${unknown} 1"));
}

void LodeTests::loaderVersions() {
    QVERIFY(Loaders::neoForgeMatches("21.1.77", "1.21.1"));
    QVERIFY(!Loaders::neoForgeMatches("21.1.77", "1.21"));
    QVERIFY(Loaders::neoForgeMatches("21.0.167", "1.21"));
    QVERIFY(Loaders::neoForgeMatches("26.1.0.12-beta", "26.1"));
    QVERIFY(!Loaders::neoForgeMatches("26.1.2.3", "26.1"));
    QCOMPARE(Loaders::expectedVersionId("forge", "1.20.1", "1.20.1-47.3.0"), QString("1.20.1-forge-47.3.0"));
    QCOMPARE(Loaders::expectedVersionId("fabric", "1.21.1", "0.16.9"), QString("fabric-loader-0.16.9-1.21.1"));
    QCOMPARE(Loaders::expectedVersionId("neoforge", "1.21.1", "21.1.77"), QString("neoforge-21.1.77"));
}

void LodeTests::modpackSafety() {
    QVERIFY(Modpacks::safeRelative("mods/sodium.jar"));
    QVERIFY(Modpacks::safeRelative("config/a/b.toml"));
    QVERIFY(!Modpacks::safeRelative("../escape.jar"));
    QVERIFY(!Modpacks::safeRelative("mods/../../x"));
    QVERIFY(!Modpacks::safeRelative("/etc/passwd"));
    QVERIFY(!Modpacks::safeRelative("C:\\windows"));
    QVERIFY(!Modpacks::safeRelative(""));
    QCOMPARE(Modpacks::curseLoader("forge-47.2.0", "1.20.1"), (QPair<QString, QString>{"forge", "1.20.1-47.2.0"}));
    QCOMPARE(Modpacks::curseLoader("neoforge-21.1.77", "1.21.1"), (QPair<QString, QString>{"neoforge", "21.1.77"}));
    QCOMPARE(Modpacks::curseLoader("fabric-0.15.11", "1.20.4"), (QPair<QString, QString>{"fabric", "0.15.11"}));
}

void LodeTests::pickVersion() {
    const QVariantList versions{QVariantMap{{"id", "b"}, {"channel", "beta"}}, QVariantMap{{"id", "r"}, {"channel", "release"}}};
    QCOMPARE(Content::pickVersion(versions).value("id").toString(), QString("r"));
    QCOMPARE(Content::pickVersion({versions.first()}).value("id").toString(), QString("b"));
    QVERIFY(Content::pickVersion({}).isEmpty());
}

void LodeTests::offlineAccounts() {
    // The UUID offline-mode servers give the player "Notch".
    QCOMPARE(Accounts::offlineUuid("Notch"), QString("b50ad385829d3141a2167e7d7539ba7f"));
    QVERIFY(Accounts::validName("Steve_01"));
    QVERIFY(!Accounts::validName("no"));
    QVERIFY(!Accounts::validName("has space"));
    QVERIFY(!Accounts::validName("waytoolongforaname1"));
}

void LodeTests::themeReadsTonal() {
    QTemporaryDir dir;
    QDir(dir.path()).mkpath("theme");
    write(dir.path() + "/theme/colors.toml",
          "mode = \"dark\"\naccent = \"#a5c8ff\"\nbackground = \"#101419\"\nselection = \"#24467c\"\nforeground = \"#dee3eb\"\n");
    Theme theme(dir.path());
    QCOMPARE(theme.accent(), QColor("#a5c8ff"));
    QCOMPARE(theme.accentContainer(), QColor("#24467c"));
    QVERIFY(theme.dark());
    // Surfaces sit between the background and the accent, rising in order.
    QVERIFY(theme.surface().lightnessF() > theme.background().lightnessF());
    QVERIFY(theme.surfaceHigh().lightnessF() > theme.surface().lightnessF());
    QVERIFY(theme.surfaceHighest().lightnessF() > theme.surfaceHigh().lightnessF());
}

void LodeTests::themeFallsBackPerKey() {
    QTemporaryDir dir;
    QDir(dir.path()).mkpath("theme");
    write(dir.path() + "/theme/colors.toml", "accent = \"#ff0000\"\ncolor1 = \"#aa0000\"\nbackground = \"#fafafa\"\n");
    Theme theme(dir.path());
    QCOMPARE(theme.accent(), QColor("#ff0000"));
    QCOMPARE(theme.red(), QColor("#aa0000"));
    QCOMPARE(theme.text(), QColor("#dee3eb"));
    QVERIFY(!theme.dark());
}

void LodeTests::translatorFallsBackToEnglish() {
    QTemporaryDir dir;
    Translator translator(dir.path() + "/language", dir.path());
    QCOMPARE(translator.language(), QString("en"));
    QVERIFY(translator.translate("Main", "Play").isNull());
}

void LodeTests::translatorPicksPluralForms() {
    QTemporaryDir dir;
    write(dir.path() + "/language", "ru\n");
    write(dir.path() + "/ru.json", R"j({"%n line(s)": {"one": "%n строка", "few": "%n строки", "many": "%n строк", "other": "%n строки"}})j");
    Translator translator(dir.path() + "/language", dir.path());
    QCOMPARE(translator.translate("Main", "%n line(s)", nullptr, 21), QString("%n строка"));
    QCOMPARE(translator.translate("Main", "%n line(s)", nullptr, 25), QString("%n строк"));
}

QTEST_MAIN(LodeTests)
#include "lode_tests.moc"
