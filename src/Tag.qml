import QtQuick

// A small read-only label, like an assist chip without the action.
Rectangle {
    property alias text: label.text
    property color tint: theme.accentContainer
    property color ink: theme.onAccentContainer
    implicitWidth: label.implicitWidth + 16
    implicitHeight: 24
    radius: Math.min(8, theme.radius)
    color: tint
    Text {
        id: label
        anchors.centerIn: parent
        color: parent.ink
        font.pixelSize: 12
        font.weight: Font.Medium
    }
}
