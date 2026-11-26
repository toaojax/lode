#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QUrl>

class QNetworkReply;
class QFile;
class QCryptographicHash;

// A batch of files fetched in parallel. Each file is streamed to a temporary
// name, checked against its SHA-1 when one is known, and only then renamed
// into place, so a cancelled or broken download never looks finished. Files
// that already match are skipped without touching the network.
class DownloadBatch : public QObject {
    Q_OBJECT

public:
    struct Item {
        QUrl url;
        QString path;
        QString sha1;
        qint64 size = -1;
        bool executable = false;
        // Assets are checked by size only; hashing thousands of them on every
        // launch costs more than it saves.
        bool trustSize = false;
    };

    explicit DownloadBatch(QObject *parent = nullptr);
    ~DownloadBatch() override;

    void add(const Item &item);
    void add(const QUrl &url, const QString &path, const QString &sha1 = {}, qint64 size = -1);
    int count() const { return m_items.size(); }
    void start();
    void cancel();

    static bool matches(const Item &item);
    static QString sha1Of(const QString &path);

signals:
    void progress(qint64 doneBytes, qint64 totalBytes, int doneFiles, int totalFiles);
    // On failure, error names the first file that couldn't be fetched.
    void finished(bool ok, const QString &error);

private:
    struct Active {
        Item item;
        QNetworkReply *reply = nullptr;
        QFile *file = nullptr;
        QCryptographicHash *hash = nullptr;
        int attempt = 0;
        qint64 received = 0;
    };

    void pump();
    void begin(const Item &item, int attempt);
    void complete(Active *active);
    void fail(const QString &error);
    void report();

    QList<Item> m_items;
    QList<Item> m_queue;
    QList<Active *> m_active;
    QSet<QString> m_seen;
    qint64 m_totalBytes = 0, m_doneBytes = 0;
    int m_doneFiles = 0, m_totalFiles = 0;
    bool m_failed = false, m_finished = false, m_started = false;
};
