import QtQuick

// A tonal container: a layer of the background lifted toward the accent.
// level 0 is the lowest, 3 the highest, like Material 3's surface containers.
Rectangle {
    property int level: 1
    radius: theme.radius
    color: level <= 0 ? theme.surfaceLowest : level === 1 ? theme.surface : level === 2 ? theme.surfaceHigh : theme.surfaceHighest
}
