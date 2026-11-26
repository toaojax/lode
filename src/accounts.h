#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QTimer>
#include <functional>

class Settings;

struct Account {
    QString kind;          // "msa" or "offline"
    QString name;
    QString uuid;          // without dashes, as the game wants it
    QString accessToken;   // the Minecraft token
    QDateTime expires;
    QString refreshToken;  // Microsoft's, for signing in again quietly
    QString xuid;
};

// The people who can play. Microsoft accounts sign in with the device-code
// flow (a code shown in the window, entered at microsoft.com/link), which
// needs no embedded browser and no redirect server. Tokens are stored in a
// file only the user can read.
class Accounts : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int activeIndex READ activeIndex WRITE setActiveIndex NOTIFY activeChanged)
    Q_PROPERTY(QString activeName READ activeName NOTIFY activeChanged)
    Q_PROPERTY(QString activeUuid READ activeUuid NOTIFY activeChanged)
    Q_PROPERTY(QString loginState READ loginState NOTIFY loginChanged)
    Q_PROPERTY(QString userCode READ userCode NOTIFY loginChanged)
    Q_PROPERTY(QString verificationUrl READ verificationUrl NOTIFY loginChanged)
    Q_PROPERTY(QString loginError READ loginError NOTIFY loginChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role { NameRole = Qt::UserRole + 1, KindRole, UuidRole, ActiveRole, ExpiredRole };
    using Ready = std::function<void(bool ok, const Account &account, const QString &error)>;

    explicit Accounts(Settings *settings, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int activeIndex() const { return m_active; }
    void setActiveIndex(int index);
    QString activeName() const;
    QString activeUuid() const;
    QString loginState() const { return m_loginState; }
    QString userCode() const { return m_userCode; }
    QString verificationUrl() const { return m_verificationUrl; }
    QString loginError() const { return m_loginError; }

    Q_INVOKABLE void startMicrosoftLogin();
    Q_INVOKABLE void cancelLogin();
    Q_INVOKABLE bool addOffline(const QString &name);
    Q_INVOKABLE void remove(int index);
    Q_INVOKABLE void copyCode() const;

    // The active account with a token good for at least ten more minutes,
    // signing in again quietly when it's close to expiring.
    void activeAccount(Ready done);

    static QString offlineUuid(const QString &name);
    static bool validName(const QString &name);

signals:
    void activeChanged();
    void loginChanged();
    void countChanged();
    void signedIn(const QString &name);

private:
    void load();
    void save();
    void setLogin(const QString &state, const QString &error = {});
    void pollDeviceCode();
    // Microsoft token → Xbox Live → XSTS → Minecraft token → profile.
    void finishMicrosoft(const QString &msToken, const QString &refreshToken, Ready done);
    void upsert(const Account &account);

    Settings *m_settings;
    QList<Account> m_accounts;
    int m_active = -1;
    QString m_loginState = "idle";  // idle, code, working, error
    QString m_userCode, m_verificationUrl, m_deviceCode, m_loginError;
    QTimer m_poll;
    int m_pollInterval = 5;
    QDateTime m_codeExpires;
    int m_loginGeneration = 0;
};
