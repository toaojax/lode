#include "paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
QString &rootOverride() {
    static QString value;
    return value;
}
}

void Paths::setRoot(const QString &root) { rootOverride() = root; }

QString Paths::root() {
    if (!rootOverride().isEmpty())
        return rootOverride();
    if (!qEnvironmentVariableIsEmpty("LODE_HOME"))
        return qEnvironmentVariable("LODE_HOME");
    const QString data = qEnvironmentVariableIsEmpty("XDG_DATA_HOME") ? QDir::homePath() + "/.local/share"
                                                                       : qEnvironmentVariable("XDG_DATA_HOME");
    return data + "/lode";
}

QString Paths::libraries() { return root() + "/libraries"; }
QString Paths::assets() { return root() + "/assets"; }
QString Paths::versions() { return root() + "/versions"; }
QString Paths::instances() { return root() + "/instances"; }
QString Paths::java() { return root() + "/java"; }
QString Paths::cache() { return root() + "/cache"; }

QString Paths::configFile() {
    if (!rootOverride().isEmpty() || !qEnvironmentVariableIsEmpty("LODE_HOME"))
        return root() + "/config.json";
    const QString config = qEnvironmentVariableIsEmpty("XDG_CONFIG_HOME") ? QDir::homePath() + "/.config"
                                                                           : qEnvironmentVariable("XDG_CONFIG_HOME");
    return config + "/lode/config.json";
}

QString Paths::accountsFile() { return root() + "/accounts.json"; }

QString Paths::mavenPath(const QString &coordinate) {
    QString coords = coordinate, extension = "jar";
    const int at = coords.indexOf('@');
    if (at >= 0) {
        extension = coords.mid(at + 1);
        coords = coords.left(at);
    }
    const QStringList parts = coords.split(':');
    if (parts.size() < 3)
        return {};
    const QString group = QString(parts[0]).replace('.', '/');
    const QString &artifact = parts[1], &version = parts[2];
    QString file = artifact + "-" + version;
    if (parts.size() > 3)
        file += "-" + parts[3];
    return group + "/" + artifact + "/" + version + "/" + file + "." + extension;
}

bool Paths::writeAtomically(const QString &path, const QByteArray &data, bool privateFile) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    if (privateFile)
        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    file.write(data);
    return file.commit();
}

QByteArray Paths::readAll(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QString Paths::uniqueFolder(const QString &parent, const QString &name) {
    QString base = name.trimmed();
    base.replace(QRegularExpression(R"([/\\:*?"<>|\x00-\x1f])"), "-");
    base = base.left(64).trimmed();
    if (base.isEmpty() || base.startsWith('.'))
        base = "instance";
    QString candidate = base;
    for (int n = 2; QFileInfo::exists(parent + "/" + candidate); ++n)
        candidate = QString("%1 (%2)").arg(base).arg(n);
    return candidate;
}
