#include "accounts.h"

#include "http.h"
#include "paths.h"
#include "settings.h"

#include <QClipboard>
#include <QCryptographicHash>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUuid>

static const char *kDeviceCodeUrl = "https://login.microsoftonline.com/consumers/oauth2/v2.0/devicecode";
static const char *kTokenUrl = "https://login.microsoftonline.com/consumers/oauth2/v2.0/token";
static const char *kScope = "XboxLive.signin offline_access";

Accounts::Accounts(Settings *settings, QObject *parent) : QAbstractListModel(parent), m_settings(settings) {
    m_poll.setSingleShot(true);
    connect(&m_poll, &QTimer::timeout, this, &Accounts::pollDeviceCode);
    load();
}

int Accounts::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_accounts.size(); }

QVariant Accounts::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_accounts.size())
        return {};
    const Account &a = m_accounts[index.row()];
    switch (role) {
    case NameRole: return a.name;
    case KindRole: return a.kind;
    case UuidRole: return a.uuid;
    case ActiveRole: return index.row() == m_active;
    case ExpiredRole: return a.kind == "msa" && a.refreshToken.isEmpty();
    }
    return {};
}

QHash<int, QByteArray> Accounts::roleNames() const {
    return {{NameRole, "name"}, {KindRole, "kind"}, {UuidRole, "uuid"}, {ActiveRole, "active"}, {ExpiredRole, "expired"}};
}

void Accounts::load() {
    const QJsonObject o = QJsonDocument::fromJson(Paths::readAll(Paths::accountsFile())).object();
    for (const QJsonValue &v : o.value("accounts").toArray()) {
        const QJsonObject a = v.toObject();
        m_accounts.append({a.value("kind").toString(), a.value("name").toString(), a.value("uuid").toString(),
                           a.value("accessToken").toString(),
                           QDateTime::fromString(a.value("expires").toString(), Qt::ISODate),
                           a.value("refreshToken").toString(), a.value("xuid").toString()});
    }
    m_active = qBound(-1, o.value("active").toInt(m_accounts.isEmpty() ? -1 : 0), int(m_accounts.size()) - 1);
}

void Accounts::save() {
    QJsonArray list;
    for (const Account &a : m_accounts)
        list.append(QJsonObject{{"kind", a.kind}, {"name", a.name}, {"uuid", a.uuid}, {"accessToken", a.accessToken},
                                {"expires", a.expires.toString(Qt::ISODate)}, {"refreshToken", a.refreshToken},
                                {"xuid", a.xuid}});
    Paths::writeAtomically(Paths::accountsFile(), QJsonDocument(QJsonObject{{"active", m_active}, {"accounts", list}}).toJson(), true);
}

void Accounts::setActiveIndex(int index) {
    if (index < 0 || index >= m_accounts.size() || index == m_active)
        return;
    const int old = m_active;
    m_active = index;
    save();
    if (old >= 0)
        emit dataChanged(this->index(old), this->index(old), {ActiveRole});
    emit dataChanged(this->index(index), this->index(index), {ActiveRole});
    emit activeChanged();
}

QString Accounts::activeName() const { return m_active >= 0 ? m_accounts[m_active].name : QString(); }
QString Accounts::activeUuid() const { return m_active >= 0 ? m_accounts[m_active].uuid : QString(); }

bool Accounts::validName(const QString &name) {
    static const QRegularExpression valid("^[A-Za-z0-9_]{3,16}$");
    return valid.match(name).hasMatch();
}

// The same UUID the game and offline-mode servers derive from a name
// (a version 3 UUID of "OfflinePlayer:<name>"), so worlds keep their owner.
QString Accounts::offlineUuid(const QString &name) {
    QByteArray hash = QCryptographicHash::hash("OfflinePlayer:" + name.toUtf8(), QCryptographicHash::Md5);
    hash[6] = char((hash[6] & 0x0f) | 0x30);
    hash[8] = char((hash[8] & 0x3f) | 0x80);
    return QString::fromLatin1(hash.toHex());
}

bool Accounts::addOffline(const QString &rawName) {
    const QString name = rawName.trimmed();
    if (!validName(name))
        return false;
    upsert({"offline", name, offlineUuid(name), "0", {}, {}, {}});
    return true;
}

void Accounts::upsert(const Account &account) {
    for (int i = 0; i < m_accounts.size(); ++i) {
        if (m_accounts[i].uuid == account.uuid && m_accounts[i].kind == account.kind) {
            m_accounts[i] = account;
            save();
            emit dataChanged(index(i), index(i));
            setActiveIndex(i);
            emit activeChanged();
            return;
        }
    }
    beginInsertRows({}, m_accounts.size(), m_accounts.size());
    m_accounts.append(account);
    endInsertRows();
    emit countChanged();
    m_active = -1;
    setActiveIndex(m_accounts.size() - 1);
}

void Accounts::remove(int i) {
    if (i < 0 || i >= m_accounts.size())
        return;
    beginRemoveRows({}, i, i);
    m_accounts.removeAt(i);
    endRemoveRows();
    if (m_active >= m_accounts.size() || m_active == i)
        m_active = m_accounts.isEmpty() ? -1 : 0;
    else if (m_active > i)
        --m_active;
    save();
    emit countChanged();
    emit activeChanged();
    if (m_active >= 0)
        emit dataChanged(index(m_active), index(m_active), {ActiveRole});
}

void Accounts::copyCode() const {
    if (auto *clipboard = QGuiApplication::clipboard())
        clipboard->setText(m_userCode);
}

void Accounts::setLogin(const QString &state, const QString &error) {
    m_loginState = state;
    m_loginError = error;
    if (state != "code") {
        m_userCode.clear();
        m_verificationUrl.clear();
    }
    emit loginChanged();
}

void Accounts::cancelLogin() {
    ++m_loginGeneration;
    m_poll.stop();
    m_deviceCode.clear();
    setLogin("idle");
}

void Accounts::startMicrosoftLogin() {
    if (!m_settings->hasMicrosoft())
        return setLogin("error", tr("Add your Azure client ID in Settings to sign in with Microsoft."));
    const int generation = ++m_loginGeneration;
    setLogin("working");
    Http::postForm(this, QUrl(kDeviceCodeUrl), {{"client_id", m_settings->microsoftClientId()}, {"scope", kScope}},
                   [this, generation](bool ok, const QJsonDocument &json, const QString &error, int) {
        if (generation != m_loginGeneration)
            return;
        if (!ok)
            return setLogin("error", tr("Microsoft refused to start signing in: %1").arg(error));
        const QJsonObject o = json.object();
        m_deviceCode = o.value("device_code").toString();
        m_userCode = o.value("user_code").toString();
        m_verificationUrl = o.value("verification_uri").toString();
        m_pollInterval = qMax(1, o.value("interval").toInt(5));
        m_codeExpires = QDateTime::currentDateTimeUtc().addSecs(o.value("expires_in").toInt(900));
        m_loginState = "code";
        m_loginError.clear();
        emit loginChanged();
        m_poll.start(m_pollInterval * 1000);
    });
}

void Accounts::pollDeviceCode() {
    if (m_deviceCode.isEmpty())
        return;
    if (QDateTime::currentDateTimeUtc() > m_codeExpires)
        return setLogin("error", tr("The code expired. Start again to get a new one."));
    const int generation = m_loginGeneration;
    Http::postForm(this, QUrl(kTokenUrl),
                   {{"grant_type", "urn:ietf:params:oauth:grant-type:device_code"},
                    {"client_id", m_settings->microsoftClientId()}, {"device_code", m_deviceCode}},
                   [this, generation](bool ok, const QJsonDocument &json, const QString &error, int) {
        if (generation != m_loginGeneration)
            return;
        const QJsonObject o = json.object();
        const QString code = o.value("error").toString();
        if (code == "authorization_pending")
            return m_poll.start(m_pollInterval * 1000);
        if (code == "slow_down") {
            m_pollInterval += 5;
            return m_poll.start(m_pollInterval * 1000);
        }
        if (!ok || !o.contains("access_token")) {
            m_deviceCode.clear();
            if (code == "authorization_declined")
                return setLogin("error", tr("Signing in was declined."));
            return setLogin("error", tr("Microsoft sign-in failed: %1").arg(error));
        }
        m_deviceCode.clear();
        setLogin("working");
        finishMicrosoft(o.value("access_token").toString(), o.value("refresh_token").toString(),
                        [this, generation](bool ok, const Account &account, const QString &error) {
            if (generation != m_loginGeneration)
                return;
            if (!ok)
                return setLogin("error", error);
            upsert(account);
            setLogin("idle");
            emit signedIn(account.name);
        });
    });
}

static QString xstsMessage(const QJsonObject &o) {
    switch (qint64(o.value("XErr").toDouble())) {
    case 2148916233: return QObject::tr("This Microsoft account has no Xbox profile yet. Sign in at xbox.com once, then try again.");
    case 2148916235: return QObject::tr("Xbox Live isn't available in this account's country.");
    case 2148916236: case 2148916237: return QObject::tr("This account needs adult verification on xbox.com first.");
    case 2148916238: return QObject::tr("This is a child account. An adult has to add it to a Microsoft family first.");
    }
    return {};
}

void Accounts::finishMicrosoft(const QString &msToken, const QString &refreshToken, Ready done) {
    const QJsonObject xblBody{{"Properties", QJsonObject{{"AuthMethod", "RPS"}, {"SiteName", "user.auth.xboxlive.com"},
                                                           {"RpsTicket", "d=" + msToken}}},
                              {"RelyingParty", "http://auth.xboxlive.com"}, {"TokenType", "JWT"}};
    Http::postJson(this, QUrl("https://user.auth.xboxlive.com/user/authenticate"), QJsonDocument(xblBody),
                   [=](bool ok, const QJsonDocument &json, const QString &error, int) {
        if (!ok)
            return done(false, {}, tr("Xbox Live sign-in failed: %1").arg(error));
        const QString xbl = json.object().value("Token").toString();
        const QJsonObject xstsBody{{"Properties", QJsonObject{{"SandboxId", "RETAIL"}, {"UserTokens", QJsonArray{xbl}}}},
                                   {"RelyingParty", "rp://api.minecraftservices.com/"}, {"TokenType", "JWT"}};
        Http::postJson(this, QUrl("https://xsts.auth.xboxlive.com/xsts/authorize"), QJsonDocument(xstsBody),
                       [=](bool ok, const QJsonDocument &json, const QString &error, int) {
            const QJsonObject o = json.object();
            if (!ok) {
                const QString specific = xstsMessage(o);
                return done(false, {}, specific.isEmpty() ? tr("Xbox Live refused the sign-in: %1").arg(error) : specific);
            }
            const QString xsts = o.value("Token").toString();
            const QJsonObject claim = o.value("DisplayClaims").toObject().value("xui").toArray().first().toObject();
            const QString uhs = claim.value("uhs").toString();
            const QString xuid = claim.value("xid").toString();
            Http::postJson(this, QUrl("https://api.minecraftservices.com/authentication/login_with_xbox"),
                           QJsonDocument(QJsonObject{{"identityToken", "XBL3.0 x=" + uhs + ";" + xsts}}),
                           [=](bool ok, const QJsonDocument &json, const QString &error, int status) {
                if (!ok) {
                    // Mojang answers 403 to apps it hasn't approved yet.
                    if (status == 403)
                        return done(false, {}, tr("Minecraft refused this client ID. New launchers have to be approved "
                                                  "by Mojang before they can sign in (aka.ms/mce-reviewappid)."));
                    return done(false, {}, tr("Minecraft sign-in failed: %1").arg(error));
                }
                const QString mcToken = json.object().value("access_token").toString();
                const int expiresIn = json.object().value("expires_in").toInt(86400);
                Http::getJson(this, QUrl("https://api.minecraftservices.com/minecraft/profile"),
                              [=](bool ok, const QJsonDocument &json, const QString &error, int status) {
                    if (status == 404)
                        return done(false, {}, tr("This Microsoft account doesn't own Minecraft: Java Edition."));
                    if (!ok)
                        return done(false, {}, tr("Couldn't load the Minecraft profile: %1").arg(error));
                    Account a;
                    a.kind = "msa";
                    a.name = json.object().value("name").toString();
                    a.uuid = json.object().value("id").toString();
                    a.accessToken = mcToken;
                    a.expires = QDateTime::currentDateTimeUtc().addSecs(expiresIn);
                    a.refreshToken = refreshToken;
                    a.xuid = xuid;
                    done(true, a, {});
                }, {{"Authorization", "Bearer " + mcToken.toUtf8()}});
            });
        });
    });
}

void Accounts::activeAccount(Ready done) {
    if (m_active < 0)
        return done(false, {}, tr("Add an account first."));
    const Account account = m_accounts[m_active];
    if (account.kind == "offline")
        return done(true, account, {});
    if (account.expires.isValid() && QDateTime::currentDateTimeUtc().secsTo(account.expires) > 600)
        return done(true, account, {});
    if (!m_settings->hasMicrosoft())
        return done(false, {}, tr("Add your Azure client ID in Settings to refresh %1's sign-in.").arg(account.name));
    if (account.refreshToken.isEmpty())
        return done(false, {}, tr("Sign in to %1 again.").arg(account.name));
    Http::postForm(this, QUrl(kTokenUrl),
                   {{"grant_type", "refresh_token"}, {"client_id", m_settings->microsoftClientId()},
                    {"refresh_token", account.refreshToken}, {"scope", kScope}},
                   [this, account, done](bool ok, const QJsonDocument &json, const QString &error, int) {
        if (!ok)
            return done(false, {}, tr("Couldn't refresh %1's sign-in (%2). Sign in again in Accounts.").arg(account.name, error));
        const QJsonObject o = json.object();
        finishMicrosoft(o.value("access_token").toString(), o.value("refresh_token").toString(account.refreshToken),
                        [this, done](bool ok, const Account &fresh, const QString &error) {
            if (ok) {
                for (Account &a : m_accounts)
                    if (a.kind == "msa" && a.uuid == fresh.uuid)
                        a = fresh;
                save();
            }
            done(ok, fresh, error);
        });
    });
}
