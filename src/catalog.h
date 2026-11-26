#pragma once

#include <QAbstractListModel>
#include <QJsonObject>
#include <QPointer>
#include <QTimer>
#include <QVariantList>

class QNetworkReply;
class Settings;

// Both stores behind one search: Modrinth's open API and CurseForge's keyed
// one. Results come in pages and the view asks for the next page as it nears
// the end, so scrolling never waits on a giant first request.
class Catalog : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString provider READ provider WRITE setProvider NOTIFY filtersChanged)
    Q_PROPERTY(QString projectType READ projectType WRITE setProjectType NOTIFY filtersChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY filtersChanged)
    Q_PROPERTY(QString gameVersion READ gameVersion WRITE setGameVersion NOTIFY filtersChanged)
    Q_PROPERTY(QString loader READ loader WRITE setLoader NOTIFY filtersChanged)
    Q_PROPERTY(QString sort READ sortKey WRITE setSort NOTIFY filtersChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(int total READ total NOTIFY stateChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY stateChanged)

public:
    enum Role { IdRole = Qt::UserRole + 1, ProviderRole, SlugRole, TitleRole, SummaryRole, AuthorRole, IconRole,
                DownloadsRole, FollowsRole, CategoriesRole, UpdatedRole, UrlRole, TypeRole };

    explicit Catalog(Settings *settings, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString provider() const { return m_provider; }
    QString projectType() const { return m_type; }
    QString query() const { return m_query; }
    QString gameVersion() const { return m_gameVersion; }
    QString loader() const { return m_loader; }
    QString sortKey() const { return m_sort; }
    bool loading() const { return m_loading; }
    QString error() const { return m_error; }
    int total() const { return m_total; }
    bool hasMore() const { return m_items.size() < m_total; }

    void setProvider(const QString &v);
    void setProjectType(const QString &v);
    void setQuery(const QString &v);
    void setGameVersion(const QString &v);
    void setLoader(const QString &v);
    void setSort(const QString &v);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE QVariantMap at(int row) const;

    // The project's long description, gallery and links.
    Q_INVOKABLE void fetchProject(const QString &provider, const QString &id);
    // Every file of a project that fits the filters, newest first. Each entry
    // is {id, name, number, channel, date, gameVersions, loaders, file, url,
    // sha1, size, dependencies, downloadable}.
    Q_INVOKABLE void fetchVersions(const QString &provider, const QString &id, const QString &gameVersion,
                                   const QString &loader, const QString &projectType);

    // CurseForge's numbers for its classes and loaders.
    static int curseClass(const QString &projectType);
    static int curseLoader(const QString &loader);
    static QVariantMap modrinthVersion(const QJsonObject &v);
    static QVariantMap curseFile(const QJsonObject &f);
    static QList<QPair<QByteArray, QByteArray>> curseHeaders(Settings *settings);

signals:
    void filtersChanged();
    void stateChanged();
    void projectReady(const QString &id, const QVariantMap &project);
    void versionsReady(const QString &id, const QVariantList &versions, const QString &error);

private:
    struct Item {
        QString id, provider, slug, title, summary, author, icon, url, type;
        qint64 downloads = 0, follows = 0;
        QStringList categories;
        QString updated;
    };
    void request(bool append);
    void parseModrinth(const QJsonObject &o, bool append);
    void parseCurse(const QJsonObject &o, bool append);
    void setError(const QString &e);

    Settings *m_settings;
    QList<Item> m_items;
    QString m_provider = "modrinth", m_type = "mod", m_query, m_gameVersion, m_loader, m_sort = "relevance";
    bool m_loading = false;
    QString m_error;
    int m_total = 0;
    int m_generation = 0;
    QTimer m_debounce;
    QPointer<QNetworkReply> m_reply;
};
