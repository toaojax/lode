#pragma once

#include <QObject>
#include <QString>

// The launcher's own preferences, kept in ~/.config/lode/config.json. The
// Microsoft client ID and CurseForge key are the user's own registrations; the
// app works without them and says what's missing where it matters.
class Settings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString microsoftClientId READ microsoftClientId WRITE setMicrosoftClientId NOTIFY changed)
    Q_PROPERTY(QString curseForgeKey READ curseForgeKey WRITE setCurseForgeKey NOTIFY changed)
    Q_PROPERTY(int memoryMb READ memoryMb WRITE setMemoryMb NOTIFY changed)
    Q_PROPERTY(QString javaArgs READ javaArgs WRITE setJavaArgs NOTIFY changed)
    Q_PROPERTY(bool closeOnLaunch READ closeOnLaunch WRITE setCloseOnLaunch NOTIFY changed)
    Q_PROPERTY(int windowWidth READ windowWidth WRITE setWindowWidth NOTIFY changed)
    Q_PROPERTY(int windowHeight READ windowHeight WRITE setWindowHeight NOTIFY changed)
    Q_PROPERTY(QString wrapperCommand READ wrapperCommand WRITE setWrapperCommand NOTIFY changed)
    Q_PROPERTY(bool hasCurseForge READ hasCurseForge NOTIFY changed)
    Q_PROPERTY(bool hasMicrosoft READ hasMicrosoft NOTIFY changed)
    Q_PROPERTY(int totalMemoryMb READ totalMemoryMb CONSTANT)
    Q_PROPERTY(QString path READ path CONSTANT)

public:
    explicit Settings(QObject *parent = nullptr);

    QString microsoftClientId() const { return m_clientId; }
    QString curseForgeKey() const { return m_cfKey; }
    int memoryMb() const { return m_memory; }
    QString javaArgs() const { return m_javaArgs; }
    bool closeOnLaunch() const { return m_closeOnLaunch; }
    int windowWidth() const { return m_width; }
    int windowHeight() const { return m_height; }
    QString wrapperCommand() const { return m_wrapper; }
    bool hasCurseForge() const { return !m_cfKey.isEmpty(); }
    bool hasMicrosoft() const { return !m_clientId.isEmpty(); }
    int totalMemoryMb() const;
    QString path() const;

    void setMicrosoftClientId(const QString &v);
    void setCurseForgeKey(const QString &v);
    void setMemoryMb(int v);
    void setJavaArgs(const QString &v);
    void setCloseOnLaunch(bool v);
    void setWindowWidth(int v);
    void setWindowHeight(int v);
    void setWrapperCommand(const QString &v);

signals:
    void changed();

private:
    void save();

    QString m_clientId, m_cfKey, m_javaArgs, m_wrapper;
    int m_memory = 4096, m_width = 0, m_height = 0;
    bool m_closeOnLaunch = false;
};
