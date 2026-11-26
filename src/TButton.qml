import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// The app's one button, in Material 3's tonal family: "filled" wears the
// accent, "tonal" the accent's container, "outlined" and "text" stay quiet,
// "danger" is for things that lose data.
AbstractButton {
    id: control
    property string kind: "tonal"
    property string glyph: ""
    property string hint: ""
    property bool compact: false

    readonly property color fill: {
        switch (kind) {
        case "filled": return theme.accent
        case "tonal": return theme.accentContainer
        case "danger": return Qt.rgba(theme.red.r, theme.red.g, theme.red.b, 0.16)
        default: return "transparent"
        }
    }
    readonly property color ink: {
        switch (kind) {
        case "filled": return theme.accentForeground
        case "tonal": return theme.onAccentContainer
        case "danger": return theme.red
        case "text": return theme.accent
        default: return theme.text
        }
    }

    implicitHeight: compact ? 32 : 40
    implicitWidth: text === "" ? implicitHeight : row.implicitWidth + (compact ? 24 : 32)
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    opacity: enabled ? 1 : 0.38
    font.pixelSize: compact ? 13 : 14
    font.weight: Font.Medium

    ToolTip.visible: hint !== "" && hovered
    ToolTip.delay: 600
    ToolTip.text: hint
    Accessible.name: text !== "" ? text : hint

    background: Rectangle {
        radius: Math.min(height / 2, theme.radius)
        color: control.fill
        border.width: control.kind === "outlined" ? 1 : 0
        border.color: theme.outline

        // Material's state layer: the ink color at low opacity on hover and press.
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: control.ink
            opacity: control.down ? 0.16 : control.hovered ? 0.08 : control.visualFocus ? 0.12 : 0
            Behavior on opacity { NumberAnimation { duration: 90 } }
        }
    }

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight
        RowLayout {
            id: row
            anchors.centerIn: parent
            spacing: 8
            Icon {
                visible: control.glyph !== ""
                name: control.glyph
                size: control.compact ? 18 : 20
                color: control.ink
                filled: control.kind === "filled"
            }
            Text {
                visible: control.text !== ""
                text: control.text
                font: control.font
                color: control.ink
                elide: Text.ElideRight
            }
        }
    }
}
