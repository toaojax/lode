#include "settings.h"

#include "paths.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

Settings::Settings(QObject *parent) : QObject(parent) {
    const QJsonObject o = QJsonDocument::fromJson(Paths::readAll(path())).object();
    m_clientId = o.value("microsoftClientId").toString();
    m_cfKey = o.value("curseForgeKey").toString();
    m_memory = o.value("memoryMb").toInt(4096);
    m_javaArgs = o.value("javaArgs").toString();
    m_closeOnLaunch = o.value("closeOnLaunch").toBool(false);
    m_width = o.value("windowWidth").toInt(0);
    m_height = o.value("windowHeight").toInt(0);
    m_wrapper = o.value("wrapperCommand").toString();
    // Environment variables win, so a key can be tried without saving it.
    if (!qEnvironmentVariableIsEmpty("LODE_MS_CLIENT_ID"))
        m_clientId = qEnvironmentVariable("LODE_MS_CLIENT_ID");
    if (!qEnvironmentVariableIsEmpty("LODE_CURSEFORGE_KEY"))
        m_cfKey = qEnvironmentVariable("LODE_CURSEFORGE_KEY");
}

QString Settings::path() const { return Paths::configFile(); }

int Settings::totalMemoryMb() const {
    QFile meminfo("/proc/meminfo");
    if (meminfo.open(QIODevice::ReadOnly)) {
        const QByteArray line = meminfo.readLine();
        const QList<QByteArray> parts = line.simplified().split(' ');
        if (parts.size() >= 2)
            return int(parts[1].toLongLong() / 1024);
    }
    return 16384;
}

void Settings::save() {
    QJsonObject o{{"microsoftClientId", m_clientId}, {"curseForgeKey", m_cfKey}, {"memoryMb", m_memory},
                  {"javaArgs", m_javaArgs}, {"closeOnLaunch", m_closeOnLaunch}, {"windowWidth", m_width},
                  {"windowHeight", m_height}, {"wrapperCommand", m_wrapper}};
    // The CurseForge key is a secret of sorts, so keep the file private.
    Paths::writeAtomically(path(), QJsonDocument(o).toJson(), true);
    emit changed();
}

#define SETTER(name, member, type)              \
    void Settings::name(type v) {               \
        if (member == v) return;                \
        member = v;                             \
        save();                                 \
    }

SETTER(setMicrosoftClientId, m_clientId, const QString &)
SETTER(setCurseForgeKey, m_cfKey, const QString &)
SETTER(setMemoryMb, m_memory, int)
SETTER(setJavaArgs, m_javaArgs, const QString &)
SETTER(setCloseOnLaunch, m_closeOnLaunch, bool)
SETTER(setWindowWidth, m_width, int)
SETTER(setWindowHeight, m_height, int)
SETTER(setWrapperCommand, m_wrapper, const QString &)
