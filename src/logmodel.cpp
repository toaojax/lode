#include "logmodel.h"

#include <QClipboard>
#include <QGuiApplication>

static constexpr int kMaxLines = 8000;

LogModel::LogModel(QObject *parent) : QAbstractListModel(parent) {
    m_flush.setSingleShot(true);
    m_flush.setInterval(120);
    connect(&m_flush, &QTimer::timeout, this, &LogModel::flush);
}

int LogModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_lines.size(); }

QVariant LogModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_lines.size())
        return {};
    const Line &line = m_lines[index.row()];
    return role == TextRole ? QVariant(line.text) : role == LevelRole ? QVariant(line.level) : QVariant();
}

QHash<int, QByteArray> LogModel::roleNames() const { return {{TextRole, "line"}, {LevelRole, "level"}}; }

QString LogModel::levelOf(const QString &line) {
    if (line.contains("/ERROR]") || line.contains("/FATAL]") || line.contains("[ERROR]") || line.startsWith("Exception")
        || line.startsWith("Caused by:") || line.startsWith("\tat "))
        return "error";
    if (line.contains("/WARN]") || line.contains("[WARN]") || line.contains("WARNING"))
        return "warn";
    if (line.startsWith("[lode]"))
        return "lode";
    return "info";
}

void LogModel::append(const QString &line) {
    m_pending.append({line, levelOf(line)});
    if (!m_flush.isActive())
        m_flush.start();
}

void LogModel::flush() {
    if (m_pending.isEmpty())
        return;
    if (m_pending.size() > kMaxLines)
        m_pending = m_pending.mid(m_pending.size() - kMaxLines);
    const int overflow = m_lines.size() + m_pending.size() - kMaxLines;
    if (overflow > 0) {
        beginRemoveRows({}, 0, overflow - 1);
        m_lines.remove(0, overflow);
        endRemoveRows();
    }
    beginInsertRows({}, m_lines.size(), m_lines.size() + m_pending.size() - 1);
    m_lines.append(m_pending);
    m_pending.clear();
    endInsertRows();
    emit countChanged();
}

void LogModel::clear() {
    beginResetModel();
    m_lines.clear();
    m_pending.clear();
    endResetModel();
    emit countChanged();
}

QString LogModel::text() const {
    QStringList lines;
    lines.reserve(m_lines.size());
    for (const Line &l : m_lines)
        lines.append(l.text);
    return lines.join('\n');
}

void LogModel::copy() const {
    if (auto *clipboard = QGuiApplication::clipboard())
        clipboard->setText(text());
}
