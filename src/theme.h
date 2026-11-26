#pragma once

#include <QColor>
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QTimer>

// The whole Omarchy palette, not just the accent. Tonal writes a full set of
// surfaces and text colors generated from the wallpaper, so the app builds its
// layered surfaces from those and follows every change live. Themes that only
// set a few keys fall back to Tonal's defaults key by key.
class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QColor accent READ accent NOTIFY changed)
    Q_PROPERTY(QColor accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(QColor accentContainer READ accentContainer NOTIFY changed)
    Q_PROPERTY(QColor onAccentContainer READ onAccentContainer NOTIFY changed)
    Q_PROPERTY(QColor background READ background NOTIFY changed)
    Q_PROPERTY(QColor surfaceLowest READ surfaceLowest NOTIFY changed)
    Q_PROPERTY(QColor surface READ surface NOTIFY changed)
    Q_PROPERTY(QColor surfaceHigh READ surfaceHigh NOTIFY changed)
    Q_PROPERTY(QColor surfaceHighest READ surfaceHighest NOTIFY changed)
    Q_PROPERTY(QColor outline READ outline NOTIFY changed)
    Q_PROPERTY(QColor outlineVariant READ outlineVariant NOTIFY changed)
    Q_PROPERTY(QColor text READ text NOTIFY changed)
    Q_PROPERTY(QColor textBright READ textBright NOTIFY changed)
    Q_PROPERTY(QColor textSoft READ textSoft NOTIFY changed)
    Q_PROPERTY(QColor muted READ muted NOTIFY changed)
    Q_PROPERTY(QColor red READ red NOTIFY changed)
    Q_PROPERTY(QColor green READ green NOTIFY changed)
    Q_PROPERTY(QColor yellow READ yellow NOTIFY changed)
    Q_PROPERTY(QColor orange READ orange NOTIFY changed)
    Q_PROPERTY(QColor magenta READ magenta NOTIFY changed)
    Q_PROPERTY(QColor cyan READ cyan NOTIFY changed)
    Q_PROPERTY(bool dark READ dark NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    QColor accent() const { return color("accent"); }
    QColor accentForeground() const;
    QColor accentContainer() const;
    QColor onAccentContainer() const;
    QColor background() const { return color("background"); }
    QColor surfaceLowest() const { return color("dark_background"); }
    QColor surface() const { return tone(0.05); }
    QColor surfaceHigh() const { return tone(0.09); }
    QColor surfaceHighest() const { return tone(0.14); }
    QColor outline() const { return mix(color("muted"), background(), 0.35); }
    QColor outlineVariant() const { return mix(tone(0.14), color("muted"), 0.18); }
    QColor text() const { return color("foreground"); }
    QColor textBright() const { return color("bright_foreground"); }
    QColor textSoft() const { return color("light_foreground"); }
    QColor muted() const { return color("muted"); }
    QColor red() const { return color("red"); }
    QColor green() const { return color("green"); }
    QColor yellow() const { return color("yellow"); }
    QColor orange() const { return color("orange"); }
    QColor magenta() const { return color("magenta"); }
    QColor cyan() const { return color("cyan"); }
    bool dark() const { return m_dark; }
    int radius() const { return m_radius; }

    // Reads every `key = "#rrggbb"` line of a colors.toml.
    static QHash<QString, QColor> readColors(const QString &path, bool *dark = nullptr);
    static QHash<QString, QColor> defaults();
    static QColor mix(const QColor &a, const QColor &b, double amount);

signals:
    void changed();

private:
    void reload();
    void refreshRounding();
    QColor color(const QString &key) const { return m_colors.value(key); }
    // A surface tinted toward the accent, the way tonal palettes lift layers
    // off the background instead of greying them.
    QColor tone(double amount) const { return mix(background(), accent(), amount); }

    QString m_directory;
    QHash<QString, QColor> m_colors;
    bool m_dark = true;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 0;
    QProcess m_roundingProcess;
    QTimer m_roundingPoll, m_roundingTimeout;
};
