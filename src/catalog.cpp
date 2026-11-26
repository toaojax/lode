#include "catalog.h"

#include "http.h"
#include "settings.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QUrlQuery>

static const char *kModrinth = "https://api.modrinth.com/v2";
static const char *kCurse = "https://api.curseforge.com/v1";
static constexpr int kPage = 30;

Catalog::Catalog(Settings *settings, QObject *parent) : QAbstractListModel(parent), m_settings(settings) {
    // Typing in the search box waits for a pause rather than firing per key.
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(250);
    connect(&m_debounce, &QTimer::timeout, this, [this] { request(false); });
}

int Catalog::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_items.size(); }

QVariant Catalog::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_items.size())
        return {};
    const Item &i = m_items[index.row()];
    switch (role) {
    case IdRole: return i.id;
    case ProviderRole: return i.provider;
    case SlugRole: return i.slug;
    case TitleRole: return i.title;
    case SummaryRole: return i.summary;
    case AuthorRole: return i.author;
    case IconRole: return i.icon;
    case DownloadsRole: return i.downloads;
    case FollowsRole: return i.follows;
    case CategoriesRole: return i.categories;
    case UpdatedRole: return i.updated;
    case UrlRole: return i.url;
    case TypeRole: return i.type;
    }
    return {};
}

QHash<int, QByteArray> Catalog::roleNames() const {
    return {{IdRole, "projectId"}, {ProviderRole, "provider"}, {SlugRole, "slug"}, {TitleRole, "title"},
            {SummaryRole, "summary"}, {AuthorRole, "author"}, {IconRole, "icon"}, {DownloadsRole, "downloads"},
            {FollowsRole, "follows"}, {CategoriesRole, "categories"}, {UpdatedRole, "updated"}, {UrlRole, "url"},
            {TypeRole, "projectType"}};
}

QVariantMap Catalog::at(int row) const {
    QVariantMap map;
    const auto roles = roleNames();
    for (auto it = roles.begin(); it != roles.end(); ++it)
        map.insert(QString::fromUtf8(it.value()), data(index(row), it.key()));
    return map;
}

#define FILTER_SETTER(name, member)                    \
    void Catalog::name(const QString &v) {             \
        if (member == v) return;                       \
        member = v;                                    \
        emit filtersChanged();                         \
        m_debounce.start();                            \
    }
FILTER_SETTER(setProvider, m_provider)
FILTER_SETTER(setProjectType, m_type)
FILTER_SETTER(setQuery, m_query)
FILTER_SETTER(setGameVersion, m_gameVersion)
FILTER_SETTER(setLoader, m_loader)
FILTER_SETTER(setSort, m_sort)

void Catalog::refresh() { request(false); }

void Catalog::loadMore() {
    if (!m_loading && hasMore())
        request(true);
}

void Catalog::setError(const QString &e) {
    m_error = e;
    m_loading = false;
    emit stateChanged();
}

int Catalog::curseClass(const QString &type) {
    if (type == "resourcepack") return 12;
    if (type == "modpack") return 4471;
    if (type == "shader") return 6552;
    return 6;
}

int Catalog::curseLoader(const QString &loader) {
    if (loader == "forge") return 1;
    if (loader == "fabric") return 4;
    if (loader == "quilt") return 5;
    if (loader == "neoforge") return 6;
    return 0;
}

QList<QPair<QByteArray, QByteArray>> Catalog::curseHeaders(Settings *settings) {
    return {{"x-api-key", settings->curseForgeKey().toUtf8()}, {"Accept", "application/json"}};
}

void Catalog::request(bool append) {
    m_debounce.stop();
    const int generation = ++m_generation;
    if (m_reply)
        m_reply->abort();
    m_loading = true;
    m_error.clear();
    if (!append) {
        beginResetModel();
        m_items.clear();
        endResetModel();
        m_total = 0;
    }
    emit stateChanged();

    if (m_provider == "curseforge") {
        if (!m_settings->hasCurseForge())
            return setError(tr("Add a CurseForge API key in Settings to search CurseForge."));
        QUrlQuery q;
        q.addQueryItem("gameId", "432");
        q.addQueryItem("classId", QString::number(curseClass(m_type)));
        q.addQueryItem("pageSize", QString::number(kPage));
        q.addQueryItem("index", QString::number(append ? m_items.size() : 0));
        q.addQueryItem("sortOrder", "desc");
        const QHash<QString, int> sorts{{"relevance", 1}, {"downloads", 6}, {"follows", 2}, {"updated", 3}, {"newest", 11}};
        q.addQueryItem("sortField", QString::number(sorts.value(m_sort, 1)));
        if (!m_query.trimmed().isEmpty())
            q.addQueryItem("searchFilter", m_query.trimmed());
        if (!m_gameVersion.isEmpty())
            q.addQueryItem("gameVersion", m_gameVersion);
        if ((m_type == "mod" || m_type == "modpack") && curseLoader(m_loader))
            q.addQueryItem("modLoaderType", QString::number(curseLoader(m_loader)));
        QUrl url(QString(kCurse) + "/mods/search");
        url.setQuery(q);
        m_reply = Http::get(url, curseHeaders(m_settings));
    } else {
        QJsonArray facets{QJsonArray{"project_type:" + m_type}};
        if (!m_gameVersion.isEmpty())
            facets.append(QJsonArray{"versions:" + m_gameVersion});
        if ((m_type == "mod" || m_type == "modpack") && !m_loader.isEmpty() && m_loader != "vanilla")
            facets.append(QJsonArray{"categories:" + m_loader});
        QUrlQuery q;
        q.addQueryItem("query", m_query.trimmed());
        q.addQueryItem("facets", QString::fromUtf8(QJsonDocument(facets).toJson(QJsonDocument::Compact)));
        q.addQueryItem("index", m_sort);
        q.addQueryItem("limit", QString::number(kPage));
        q.addQueryItem("offset", QString::number(append ? m_items.size() : 0));
        QUrl url(QString(kModrinth) + "/search");
        url.setQuery(q);
        m_reply = Http::get(url);
    }

    QNetworkReply *reply = m_reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, append] {
        reply->deleteLater();
        if (generation != m_generation || reply->error() == QNetworkReply::OperationCanceledError)
            return;
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (m_provider == "curseforge" && status == 403)
                return setError(tr("CurseForge rejected the API key. Check it in Settings."));
            return setError(tr("Search failed: %1").arg(Http::describe(reply, body)));
        }
        const QJsonObject o = QJsonDocument::fromJson(body).object();
        if (m_provider == "curseforge")
            parseCurse(o, append);
        else
            parseModrinth(o, append);
        m_loading = false;
        emit stateChanged();
    });
}

void Catalog::parseModrinth(const QJsonObject &o, bool) {
    QList<Item> page;
    for (const QJsonValue &value : o.value("hits").toArray()) {
        const QJsonObject h = value.toObject();
        Item i;
        i.id = h.value("project_id").toString();
        i.provider = "modrinth";
        i.slug = h.value("slug").toString();
        i.title = h.value("title").toString();
        i.summary = h.value("description").toString();
        i.author = h.value("author").toString();
        i.icon = h.value("icon_url").toString();
        i.downloads = h.value("downloads").toInteger();
        i.follows = h.value("follows").toInteger();
        i.type = h.value("project_type").toString(m_type);
        for (const QJsonValue &c : h.value("display_categories").toArray())
            i.categories.append(c.toString());
        i.updated = h.value("date_modified").toString();
        i.url = "https://modrinth.com/" + i.type + "/" + i.slug;
        page.append(i);
    }
    m_total = o.value("total_hits").toInt();
    if (!page.isEmpty()) {
        beginInsertRows({}, m_items.size(), m_items.size() + page.size() - 1);
        m_items.append(page);
        endInsertRows();
    }
    if (page.isEmpty())
        m_total = m_items.size();
}

void Catalog::parseCurse(const QJsonObject &o, bool) {
    QList<Item> page;
    for (const QJsonValue &value : o.value("data").toArray()) {
        const QJsonObject m = value.toObject();
        Item i;
        i.id = QString::number(m.value("id").toInteger());
        i.provider = "curseforge";
        i.slug = m.value("slug").toString();
        i.title = m.value("name").toString();
        i.summary = m.value("summary").toString();
        i.author = m.value("authors").toArray().first().toObject().value("name").toString();
        i.icon = m.value("logo").toObject().value("thumbnailUrl").toString();
        i.downloads = qint64(m.value("downloadCount").toDouble());
        i.follows = m.value("thumbsUpCount").toInteger();
        i.type = m_type;
        for (const QJsonValue &c : m.value("categories").toArray())
            i.categories.append(c.toObject().value("name").toString());
        i.updated = m.value("dateModified").toString();
        i.url = m.value("links").toObject().value("websiteUrl").toString();
        page.append(i);
    }
    // CurseForge stops paging at 10,000 results.
    m_total = qMin(o.value("pagination").toObject().value("totalCount").toInt(), 10000);
    if (!page.isEmpty()) {
        beginInsertRows({}, m_items.size(), m_items.size() + page.size() - 1);
        m_items.append(page);
        endInsertRows();
    }
    if (page.isEmpty())
        m_total = m_items.size();
}

void Catalog::fetchProject(const QString &provider, const QString &id) {
    if (provider == "curseforge") {
        if (!m_settings->hasCurseForge())
            return;
        Http::getJson(this, QUrl(QString(kCurse) + "/mods/" + id), [=](bool ok, const QJsonDocument &json, const QString &, int) {
            if (!ok) return;
            const QJsonObject m = json.object().value("data").toObject();
            QVariantList gallery;
            for (const QJsonValue &s : m.value("screenshots").toArray())
                gallery.append(QVariantMap{{"url", s.toObject().value("url").toString()}, {"title", s.toObject().value("title").toString()}});
            const QJsonObject links = m.value("links").toObject();
            Http::getJson(this, QUrl(QString(kCurse) + "/mods/" + id + "/description"),
                          [=](bool ok, const QJsonDocument &json, const QString &, int) {
                emit projectReady(id, {{"body", ok ? json.object().value("data").toString() : QString()},
                                       {"format", "html"}, {"gallery", gallery},
                                       {"source", links.value("sourceUrl").toString()},
                                       {"issues", links.value("issuesUrl").toString()},
                                       {"wiki", links.value("wikiUrl").toString()},
                                       {"website", links.value("websiteUrl").toString()}});
            }, curseHeaders(m_settings));
        }, curseHeaders(m_settings));
        return;
    }
    Http::getJson(this, QUrl(QString(kModrinth) + "/project/" + id), [=](bool ok, const QJsonDocument &json, const QString &, int) {
        if (!ok) return;
        const QJsonObject p = json.object();
        QVariantList gallery;
        for (const QJsonValue &g : p.value("gallery").toArray())
            gallery.append(QVariantMap{{"url", g.toObject().value("url").toString()}, {"title", g.toObject().value("title").toString()}});
        emit projectReady(id, {{"body", p.value("body").toString()}, {"format", "markdown"}, {"gallery", gallery},
                               {"source", p.value("source_url").toString()}, {"issues", p.value("issues_url").toString()},
                               {"wiki", p.value("wiki_url").toString()}, {"discord", p.value("discord_url").toString()},
                               {"license", p.value("license").toObject().value("id").toString()},
                               {"client", p.value("client_side").toString()}, {"server", p.value("server_side").toString()},
                               {"website", "https://modrinth.com/" + p.value("project_type").toString() + "/" + p.value("slug").toString()}});
    });
}

QVariantMap Catalog::modrinthVersion(const QJsonObject &v) {
    QJsonObject primary;
    const QJsonArray files = v.value("files").toArray();
    for (const QJsonValue &f : files)
        if (f.toObject().value("primary").toBool())
            primary = f.toObject();
    if (primary.isEmpty() && !files.isEmpty())
        primary = files.first().toObject();
    QVariantList deps;
    for (const QJsonValue &d : v.value("dependencies").toArray()) {
        const QJsonObject o = d.toObject();
        deps.append(QVariantMap{{"project", o.value("project_id").toString()}, {"version", o.value("version_id").toString()},
                                {"type", o.value("dependency_type").toString()}});
    }
    return {{"id", v.value("id").toString()}, {"project", v.value("project_id").toString()},
            {"name", v.value("name").toString()}, {"number", v.value("version_number").toString()},
            {"channel", v.value("version_type").toString()}, {"date", v.value("date_published").toString()},
            {"gameVersions", v.value("game_versions").toVariant()}, {"loaders", v.value("loaders").toVariant()},
            {"file", primary.value("filename").toString()}, {"url", primary.value("url").toString()},
            {"sha1", primary.value("hashes").toObject().value("sha1").toString()},
            {"size", primary.value("size").toInteger()}, {"dependencies", deps}, {"downloadable", true},
            {"provider", "modrinth"}};
}

QVariantMap Catalog::curseFile(const QJsonObject &f) {
    QString sha1;
    for (const QJsonValue &h : f.value("hashes").toArray())
        if (h.toObject().value("algo").toInt() == 1)
            sha1 = h.toObject().value("value").toString();
    QVariantList deps;
    for (const QJsonValue &d : f.value("dependencies").toArray()) {
        const QJsonObject o = d.toObject();
        // 3 is a required dependency; optional and embedded ones are left alone.
        deps.append(QVariantMap{{"project", QString::number(o.value("modId").toInteger())}, {"version", QString()},
                                {"type", o.value("relationType").toInt() == 3 ? "required" : "optional"}});
    }
    const QHash<int, QString> channels{{1, "release"}, {2, "beta"}, {3, "alpha"}};
    QStringList gameVersions, loaders;
    for (const QJsonValue &g : f.value("gameVersions").toArray()) {
        const QString s = g.toString();
        if (s.isEmpty()) continue;
        if (s.at(0).isDigit()) gameVersions.append(s);
        else if (QStringList{"Forge", "Fabric", "Quilt", "NeoForge"}.contains(s)) loaders.append(s.toLower());
    }
    const QString url = f.value("downloadUrl").toString();
    return {{"id", QString::number(f.value("id").toInteger())}, {"project", QString::number(f.value("modId").toInteger())},
            {"name", f.value("displayName").toString()}, {"number", f.value("displayName").toString()},
            {"channel", channels.value(f.value("releaseType").toInt(), "release")}, {"date", f.value("fileDate").toString()},
            {"gameVersions", gameVersions}, {"loaders", loaders}, {"file", f.value("fileName").toString()}, {"url", url},
            {"sha1", sha1}, {"size", f.value("fileLength").toInteger()}, {"dependencies", deps},
            // Authors can keep their files off third-party launchers.
            {"downloadable", !url.isEmpty()}, {"provider", "curseforge"}};
}

void Catalog::fetchVersions(const QString &provider, const QString &id, const QString &gameVersion,
                            const QString &loader, const QString &type) {
    if (provider == "curseforge") {
        if (!m_settings->hasCurseForge())
            return emit versionsReady(id, {}, tr("Add a CurseForge API key in Settings."));
        QUrlQuery q;
        q.addQueryItem("pageSize", "50");
        if (!gameVersion.isEmpty()) q.addQueryItem("gameVersion", gameVersion);
        if ((type == "mod") && curseLoader(loader)) q.addQueryItem("modLoaderType", QString::number(curseLoader(loader)));
        QUrl url(QString(kCurse) + "/mods/" + id + "/files");
        url.setQuery(q);
        return Http::getJson(this, url, [=](bool ok, const QJsonDocument &json, const QString &error, int) {
            if (!ok)
                return emit versionsReady(id, {}, error);
            QVariantList out;
            for (const QJsonValue &f : json.object().value("data").toArray())
                out.append(curseFile(f.toObject()));
            std::stable_sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
                return a.toMap().value("date").toString() > b.toMap().value("date").toString();
            });
            emit versionsReady(id, out, {});
        }, curseHeaders(m_settings));
    }
    QUrlQuery q;
    if (!gameVersion.isEmpty())
        q.addQueryItem("game_versions", QString::fromUtf8(QJsonDocument(QJsonArray{gameVersion}).toJson(QJsonDocument::Compact)));
    QString loaderFilter;
    if (type == "mod" && !loader.isEmpty() && loader != "vanilla") loaderFilter = loader;
    if (type == "modpack" && !loader.isEmpty() && loader != "vanilla") loaderFilter = loader;
    if (type == "resourcepack") loaderFilter = "minecraft";
    if (!loaderFilter.isEmpty())
        q.addQueryItem("loaders", QString::fromUtf8(QJsonDocument(QJsonArray{loaderFilter}).toJson(QJsonDocument::Compact)));
    QUrl url(QString(kModrinth) + "/project/" + id + "/version");
    url.setQuery(q);
    Http::getJson(this, url, [=](bool ok, const QJsonDocument &json, const QString &error, int) {
        if (!ok)
            return emit versionsReady(id, {}, error);
        QVariantList out;
        for (const QJsonValue &v : json.array())
            out.append(modrinthVersion(v.toObject()));
        emit versionsReady(id, out, {});
    });
}
