#include "instances.h"

#include "paths.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <algorithm>

QString Instance::folder() const { return Paths::instances() + "/" + id; }
QString Instance::gameDir() const { return folder() + "/minecraft"; }

QJsonObject Instance::toJson() const {
    return {{"name", name}, {"minecraft", minecraft}, {"loader", loader}, {"loaderVersion", loaderVersion},
            {"versionId", versionId}, {"memoryMb", memoryMb}, {"javaArgs", javaArgs}, {"icon", iconUrl},
            {"created", created.toString(Qt::ISODate)}, {"lastPlayed", lastPlayed.toString(Qt::ISODate)},
            {"playSeconds", playSeconds}, {"pack", pack}};
}

Instance Instance::fromJson(const QString &id, const QJsonObject &o) {
    Instance i;
    i.id = id;
    i.name = o.value("name").toString(id);
    i.minecraft = o.value("minecraft").toString();
    i.loader = o.value("loader").toString("vanilla");
    i.loaderVersion = o.value("loaderVersion").toString();
    i.versionId = o.value("versionId").toString();
    i.memoryMb = o.value("memoryMb").toInt();
    i.javaArgs = o.value("javaArgs").toString();
    i.iconUrl = o.value("icon").toString();
    i.created = QDateTime::fromString(o.value("created").toString(), Qt::ISODate);
    i.lastPlayed = QDateTime::fromString(o.value("lastPlayed").toString(), Qt::ISODate);
    i.playSeconds = o.value("playSeconds").toInteger();
    i.pack = o.value("pack").toObject();
    return i;
}

Instances::Instances(QObject *parent) : QAbstractListModel(parent) {
    QDir().mkpath(Paths::instances());
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(250);
    connect(&m_debounce, &QTimer::timeout, this, &Instances::reload);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    m_watcher.addPath(Paths::instances());
    reload();
}

void Instances::reload() {
    QList<Instance> items;
    const QStringList folders = QDir(Paths::instances()).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &id : folders) {
        const QJsonDocument doc = QJsonDocument::fromJson(Paths::readAll(Paths::instances() + "/" + id + "/instance.json"));
        if (doc.isObject())
            items.append(Instance::fromJson(id, doc.object()));
    }
    beginResetModel();
    m_items = items;
    sortItems();
    endResetModel();
    emit countChanged();
}

void Instances::sortItems() {
    std::stable_sort(m_items.begin(), m_items.end(), [](const Instance &a, const Instance &b) {
        const QDateTime ta = a.lastPlayed.isValid() ? a.lastPlayed : a.created;
        const QDateTime tb = b.lastPlayed.isValid() ? b.lastPlayed : b.created;
        if (ta != tb)
            return ta > tb;
        return a.name.localeAwareCompare(b.name) < 0;
    });
}

int Instances::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_items.size(); }

QVariant Instances::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_items.size())
        return {};
    const Instance &i = m_items[index.row()];
    switch (role) {
    case IdRole: return i.id;
    case NameRole: return i.name;
    case MinecraftRole: return i.minecraft;
    case LoaderRole: return i.loader;
    case LoaderVersionRole: return i.loaderVersion;
    case IconRole: return i.iconUrl;
    case LastPlayedRole: return i.lastPlayed;
    case PlaySecondsRole: return i.playSeconds;
    case MemoryRole: return i.memoryMb;
    case JavaArgsRole: return i.javaArgs;
    case PackRole: return i.pack.toVariantMap();
    case FolderRole: return i.gameDir();
    }
    return {};
}

QHash<int, QByteArray> Instances::roleNames() const {
    return {{IdRole, "instanceId"}, {NameRole, "name"}, {MinecraftRole, "minecraft"}, {LoaderRole, "loader"},
            {LoaderVersionRole, "loaderVersion"}, {IconRole, "icon"}, {LastPlayedRole, "lastPlayed"},
            {PlaySecondsRole, "playSeconds"}, {MemoryRole, "memoryMb"}, {JavaArgsRole, "javaArgs"},
            {PackRole, "pack"}, {FolderRole, "folder"}};
}

int Instances::indexOf(const QString &id) const {
    for (int i = 0; i < m_items.size(); ++i)
        if (m_items[i].id == id)
            return i;
    return -1;
}

QString Instances::idAt(int row) const { return row >= 0 && row < m_items.size() ? m_items[row].id : QString(); }

bool Instances::find(const QString &id, Instance *out) const {
    const int i = indexOf(id);
    if (i < 0)
        return false;
    *out = m_items[i];
    return true;
}

QVariantMap Instances::get(const QString &id) const {
    const int row = indexOf(id);
    if (row < 0)
        return {};
    QVariantMap map;
    const auto roles = roleNames();
    for (auto it = roles.begin(); it != roles.end(); ++it)
        map.insert(QString::fromUtf8(it.value()), data(index(row), it.key()));
    return map;
}

bool Instances::write(const Instance &instance) {
    return Paths::writeAtomically(instance.folder() + "/instance.json", QJsonDocument(instance.toJson()).toJson());
}

QString Instances::create(const QString &name, const QString &minecraft, const QString &loader,
                          const QString &loaderVersion, const QString &iconUrl, const QJsonObject &pack) {
    Instance i;
    i.id = Paths::uniqueFolder(Paths::instances(), name);
    i.name = name.trimmed().isEmpty() ? i.id : name.trimmed();
    i.minecraft = minecraft;
    i.loader = loader.isEmpty() ? "vanilla" : loader;
    i.loaderVersion = loaderVersion;
    i.iconUrl = iconUrl;
    i.created = QDateTime::currentDateTimeUtc();
    i.pack = pack;
    QDir().mkpath(i.gameDir());
    if (!write(i))
        return {};
    beginInsertRows({}, 0, 0);
    m_items.prepend(i);
    endInsertRows();
    emit countChanged();
    return i.id;
}

void Instances::update(const Instance &instance) {
    const int row = indexOf(instance.id);
    if (row < 0)
        return;
    m_items[row] = instance;
    write(instance);
    // Playing moves an instance to the top.
    beginResetModel();
    sortItems();
    endResetModel();
}

void Instances::rename(const QString &id, const QString &name) {
    Instance i;
    if (!find(id, &i) || name.trimmed().isEmpty())
        return;
    i.name = name.trimmed();
    update(i);
}

void Instances::setMemory(const QString &id, int megabytes) {
    Instance i;
    if (!find(id, &i))
        return;
    i.memoryMb = qMax(0, megabytes);
    update(i);
}

void Instances::setJavaArgs(const QString &id, const QString &args) {
    Instance i;
    if (!find(id, &i))
        return;
    i.javaArgs = args.trimmed();
    update(i);
}

bool Instances::remove(const QString &id) {
    Instance i;
    if (!find(id, &i))
        return false;
    if (!QFile::moveToTrash(i.folder()))
        return false;
    const int row = indexOf(id);
    beginRemoveRows({}, row, row);
    m_items.removeAt(row);
    endRemoveRows();
    emit countChanged();
    return true;
}

QString Instances::duplicate(const QString &id) {
    Instance source;
    if (!find(id, &source))
        return {};
    Instance copy = source;
    copy.id = Paths::uniqueFolder(Paths::instances(), source.name + " copy");
    copy.name = tr("%1 (copy)").arg(source.name);
    copy.created = QDateTime::currentDateTimeUtc();
    copy.lastPlayed = {};
    copy.playSeconds = 0;
    // cp keeps symlinks and permissions, and reflinks where the filesystem can.
    if (QProcess::execute("cp", {"-a", "--reflink=auto", source.folder(), copy.folder()}) != 0)
        return {};
    write(copy);
    reload();
    return copy.id;
}
