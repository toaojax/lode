#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <QTimer>

// A game's output. Modded games print thousands of lines while loading, so
// lines are collected and handed to the view in batches a few times a second,
// and only the most recent ones are kept.
class LogModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role { TextRole = Qt::UserRole + 1, LevelRole };

    explicit LogModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void append(const QString &line);
    Q_INVOKABLE void clear();
    Q_INVOKABLE QString text() const;
    Q_INVOKABLE void copy() const;

    static QString levelOf(const QString &line);

signals:
    void countChanged();

private:
    void flush();

    struct Line {
        QString text;
        QString level;
    };
    QList<Line> m_lines;
    QList<Line> m_pending;
    QTimer m_flush;
};
