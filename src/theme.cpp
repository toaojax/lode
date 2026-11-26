// Theme watching follows Omacut (MIT, David Heinemeier Hansson).
#include "theme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <cmath>

Theme::Theme(const QString &directory, QObject *parent, const QString &hyprctl)
    : QObject(parent),
      m_directory(directory.isEmpty() ? QDir::homePath() + "/.local/state/omarchy/current" : directory),
      m_colors(defaults()) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Theme::reload);

    // Corners follow Hyprland's effective rounding, which themes and personal
    // overrides can both set, so ask hyprctl rather than reading its config.
    m_hyprctl = !hyprctl.isEmpty() ? hyprctl
              : directory.isEmpty() && !qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE")
                  ? QStandardPaths::findExecutable("hyprctl") : QString();
    m_roundingTimeout.setSingleShot(true);
    m_roundingTimeout.setInterval(500);
    connect(&m_roundingTimeout, &QTimer::timeout, &m_roundingProcess, &QProcess::kill);
    connect(&m_roundingProcess, &QProcess::errorOccurred, this, [this] { m_roundingTimeout.stop(); });
    connect(&m_roundingProcess, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_roundingTimeout.stop();
        const auto value = QJsonDocument::fromJson(m_roundingProcess.readAllStandardOutput()).object().value("int");
        if (code != 0 || status != QProcess::NormalExit || !value.isDouble() || value.toInt(-1) < 0)
            return;
        if (value.toInt() != m_radius) {
            m_radius = value.toInt();
            emit changed();
        }
    });
    m_roundingPoll.setInterval(1000);
    connect(&m_roundingPoll, &QTimer::timeout, this, &Theme::refreshRounding);
    if (!m_hyprctl.isEmpty())
        m_roundingPoll.start();
    reload();
}

Theme::~Theme() {
    if (m_roundingProcess.state() != QProcess::NotRunning) {
        m_roundingProcess.kill();
        m_roundingProcess.waitForFinished(1000);
    }
}

void Theme::refreshRounding() {
    if (m_hyprctl.isEmpty() || m_roundingProcess.state() != QProcess::NotRunning)
        return;
    m_roundingProcess.start(m_hyprctl, {"-j", "getoption", "decoration:rounding"});
    m_roundingTimeout.start();
}

// Tonal's own output for a blue wallpaper, so the app looks deliberate even
// where Omarchy or Tonal aren't installed.
QHash<QString, QColor> Theme::defaults() {
    return {
        {"accent", QColor("#a5c8ff")},          {"selection", QColor("#24467c")},
        {"muted", QColor("#8891a0")},           {"background", QColor("#101419")},
        {"dark_background", QColor("#0b0e13")}, {"darker_background", QColor("#000000")},
        {"lighter_background", QColor("#262a30")}, {"foreground", QColor("#dee3eb")},
        {"light_foreground", QColor("#bdc7d6")}, {"bright_foreground", QColor("#fbfcff")},
        {"red", QColor("#fc91a3")},             {"green", QColor("#54c58f")},
        {"yellow", QColor("#d4d688")},          {"orange", QColor("#fa9774")},
        {"magenta", QColor("#d49bee")},         {"cyan", QColor("#11c1e0")},
    };
}

QHash<QString, QColor> Theme::readColors(const QString &path, bool *dark) {
    QHash<QString, QColor> colors;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return colors;
    const QString contents = QString::fromUtf8(file.readAll());
    static const QRegularExpression entry(R"re(^\s*([A-Za-z0-9_]+)\s*=\s*["'](#[0-9a-fA-F]{6})["'])re",
                                          QRegularExpression::MultilineOption);
    for (auto it = entry.globalMatch(contents); it.hasNext();) {
        const auto match = it.next();
        colors.insert(match.captured(1), QColor(match.captured(2)));
    }
    // Themes written before Tonal name their ANSI colors color0..color15.
    const QList<QPair<QString, QString>> ansi{{"color1", "red"}, {"color2", "green"}, {"color3", "yellow"},
                                              {"color5", "magenta"}, {"color6", "cyan"}, {"color8", "muted"}};
    for (const auto &[from, to] : ansi)
        if (!colors.contains(to) && colors.contains(from))
            colors.insert(to, colors.value(from));
    if (dark) {
        static const QRegularExpression mode(R"re(^\s*mode\s*=\s*["'](\w+)["'])re", QRegularExpression::MultilineOption);
        const auto match = mode.match(contents);
        *dark = match.hasMatch() ? match.captured(1) != "light"
              : colors.contains("background") ? colors.value("background").lightnessF() < 0.5 : true;
    }
    return colors;
}

QColor Theme::mix(const QColor &a, const QColor &b, double t) {
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t);
}

static double luminance(const QColor &c) {
    auto linear = [](double v) { return v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4); };
    return .2126 * linear(c.redF()) + .7152 * linear(c.greenF()) + .0722 * linear(c.blueF());
}

// The theme's darkest or brightest color, whichever reads on the accent (WCAG
// relative luminance), so text on a pale accent stays in the palette's family.
QColor Theme::accentForeground() const {
    return luminance(accent()) > .179 ? color("dark_background") : color("bright_foreground");
}

// Tonal's selection color is its secondary container: a deep shade of the
// accent that a chip or a selected row sits on.
QColor Theme::accentContainer() const {
    return m_colors.contains("selection") ? color("selection") : mix(background(), accent(), 0.3);
}

QColor Theme::onAccentContainer() const {
    return luminance(accentContainer()) > .179 ? color("dark_background") : mix(accent(), color("bright_foreground"), 0.55);
}

void Theme::reload() {
    refreshRounding();
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);

    // A theme switch replaces symlinks and files, and Tonal rewrites
    // colors.toml on every wallpaper change, so watch the parents too and
    // re-arm after every change.
    QString ancestor = m_directory;
    while (!QFileInfo::exists(ancestor) && ancestor != "/")
        ancestor = QFileInfo(ancestor).absolutePath();
    QStringList candidates{ancestor, QFileInfo(ancestor).absolutePath(), m_directory,
                           m_directory + "/theme", m_directory + "/theme/colors.toml"};
    candidates.removeDuplicates();
    for (const auto &path : candidates)
        if (QFileInfo::exists(path))
            m_watcher.addPath(path);

    bool dark = true;
    auto colors = defaults();
    const auto read = readColors(m_directory + "/theme/colors.toml", &dark);
    for (auto it = read.begin(); it != read.end(); ++it)
        colors.insert(it.key(), it.value());
    if (colors != m_colors || dark != m_dark) {
        m_colors = colors;
        m_dark = dark;
        emit changed();
    }
}
