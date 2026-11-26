#include "downloads.h"

#include "http.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QTimer>
#include <QtConcurrent>

// Enough to keep a fast line busy with small asset files without opening so
// many connections that CDNs start refusing.
static constexpr int kParallel = 16;
static constexpr int kAttempts = 3;

DownloadBatch::DownloadBatch(QObject *parent) : QObject(parent) {}

DownloadBatch::~DownloadBatch() {
    m_finished = true;
    for (Active *a : m_active) {
        if (a->reply) {
            a->reply->disconnect(this);
            a->reply->abort();
            a->reply->deleteLater();
        }
        if (a->file) {
            a->file->remove();
            delete a->file;
        }
        delete a->hash;
        delete a;
    }
}

void DownloadBatch::add(const Item &item) {
    // The same library is often named by both a game version and its loader.
    if (item.path.isEmpty() || m_seen.contains(item.path))
        return;
    m_seen.insert(item.path);
    m_items.append(item);
}

void DownloadBatch::add(const QUrl &url, const QString &path, const QString &sha1, qint64 size) {
    Item item;
    item.url = url;
    item.path = path;
    item.sha1 = sha1;
    item.size = size;
    add(item);
}

QString DownloadBatch::sha1Of(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(&file);
    return QString::fromLatin1(hash.result().toHex());
}

bool DownloadBatch::matches(const Item &item) {
    const QFileInfo info(item.path);
    if (!info.exists())
        return false;
    if (item.size >= 0 && info.size() != item.size)
        return false;
    if (item.trustSize || item.sha1.isEmpty())
        return item.size >= 0 || info.size() > 0;
    return sha1Of(item.path).compare(item.sha1, Qt::CaseInsensitive) == 0;
}

void DownloadBatch::start() {
    if (m_started)
        return;
    m_started = true;
    // Checking what's already on disk hashes every library, which takes a
    // moment on a big modpack, so it happens off the UI thread.
    const QList<Item> items = m_items;
    auto *watcher = new QFutureWatcher<QList<Item>>(this);
    connect(watcher, &QFutureWatcher<QList<Item>>::finished, this, [this, watcher] {
        watcher->deleteLater();
        m_queue = watcher->result();
        m_totalFiles = m_queue.size();
        for (const Item &item : m_queue)
            m_totalBytes += qMax<qint64>(item.size, 0);
        report();
        if (m_queue.isEmpty()) {
            m_finished = true;
            emit finished(true, {});
            return;
        }
        pump();
    });
    watcher->setFuture(QtConcurrent::run([items] {
        QList<Item> missing;
        for (const Item &item : items)
            if (!matches(item))
                missing.append(item);
        return missing;
    }));
}

void DownloadBatch::cancel() { fail(tr("Cancelled")); }

void DownloadBatch::pump() {
    while (!m_failed && m_active.size() < kParallel && !m_queue.isEmpty())
        begin(m_queue.takeFirst(), 1);
    if (!m_failed && m_active.isEmpty() && m_queue.isEmpty() && !m_finished) {
        m_finished = true;
        emit finished(true, {});
    }
}

void DownloadBatch::begin(const Item &item, int attempt) {
    auto *active = new Active;
    active->item = item;
    active->attempt = attempt;
    QDir().mkpath(QFileInfo(item.path).absolutePath());
    active->file = new QFile(item.path + ".part");
    if (!active->file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString message = tr("Couldn't write %1: %2").arg(item.path, active->file->errorString());
        delete active->file;
        delete active;
        return fail(message);
    }
    active->hash = new QCryptographicHash(QCryptographicHash::Sha1);
    active->reply = Http::get(item.url);
    m_active.append(active);

    connect(active->reply, &QNetworkReply::readyRead, this, [this, active] {
        const QByteArray chunk = active->reply->readAll();
        active->file->write(chunk);
        active->hash->addData(chunk);
        active->received += chunk.size();
        if (active->item.size >= 0)
            m_doneBytes += chunk.size();
        report();
    });
    connect(active->reply, &QNetworkReply::finished, this, [this, active] { complete(active); });
}

void DownloadBatch::complete(Active *active) {
    if (m_finished)
        return;
    m_active.removeOne(active);
    QNetworkReply *reply = active->reply;
    reply->deleteLater();
    const QByteArray rest = reply->readAll();
    active->file->write(rest);
    active->hash->addData(rest);
    active->file->close();

    QString error;
    if (reply->error() != QNetworkReply::NoError)
        error = Http::describe(reply, {});
    else if (!active->item.sha1.isEmpty()
             && QString::fromLatin1(active->hash->result().toHex()).compare(active->item.sha1, Qt::CaseInsensitive) != 0)
        error = tr("the file didn't match its checksum");

    const Item item = active->item;
    const int attempt = active->attempt;
    if (item.size >= 0)
        m_doneBytes -= active->received;
    if (!error.isEmpty()) {
        active->file->remove();
    } else {
        QFile::remove(item.path);
        if (!active->file->rename(item.path))
            error = tr("couldn't move it into place");
        else if (item.executable)
            QFile::setPermissions(item.path, QFile::permissions(item.path) | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);
    }
    delete active->file;
    delete active->hash;
    delete active;

    if (!error.isEmpty()) {
        if (attempt < kAttempts) {
            // A short pause rides out CDN hiccups and rate limits.
            QTimer::singleShot(400 * attempt, this, [this, item, attempt] {
                if (!m_failed)
                    begin(item, attempt + 1);
            });
            return;
        }
        return fail(tr("Couldn't download %1 (%2)").arg(QFileInfo(item.path).fileName(), error));
    }
    m_doneBytes += qMax<qint64>(item.size, 0);
    ++m_doneFiles;
    report();
    pump();
}

void DownloadBatch::fail(const QString &error) {
    if (m_finished)
        return;
    m_failed = true;
    m_finished = true;
    for (Active *a : std::as_const(m_active)) {
        a->reply->disconnect(this);
        a->reply->abort();
        a->reply->deleteLater();
        a->file->close();
        a->file->remove();
        delete a->file;
        delete a->hash;
        delete a;
    }
    m_active.clear();
    m_queue.clear();
    emit finished(false, error);
}

void DownloadBatch::report() {
    emit progress(m_doneBytes, m_totalBytes, m_doneFiles, m_totalFiles);
}
