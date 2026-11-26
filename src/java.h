#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>

class DownloadBatch;

// The Java runtimes Mojang publishes for each game version (java-runtime-delta
// for 1.21, jre-legacy for 1.16 and older, ...), installed under java/ so a
// game always gets the Java it was built for.
class JavaRuntimes : public QObject {
    Q_OBJECT
public:
    using Callback = std::function<void(bool ok, const QString &javaPath, const QString &error)>;
    using Progress = std::function<void(double fraction, const QString &status)>;

    explicit JavaRuntimes(QObject *parent = nullptr);

    static QString javaPath(const QString &component);
    // Installed and verified once; later launches only check the stamp.
    static bool installed(const QString &component);

    void ensure(const QString &component, Callback done, Progress progress = {});

private:
    void install(const QString &component, const QUrl &manifestUrl, const QString &manifestSha1, Callback done,
                 Progress progress);
};
