import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A Material 3 filter chip: outlined when off, filled with the secondary
// container when on.
AbstractButton {
    id: chip
    property string glyph: ""
    checkable: true
    hoverEnabled: true
    implicitHeight: 32
    implicitWidth: row.implicitWidth + 24
    font.pixelSize: 13
    font.weight: Font.Medium

    background: Rectangle {
        radius: Math.min(8, theme.radius)
        color: chip.checked ? theme.accentContainer : "transparent"
        border.width: chip.checked ? 0 : 1
        border.color: theme.outlineVariant
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: chip.checked ? theme.onAccentContainer : theme.text
            opacity: chip.down ? 0.14 : chip.hovered ? 0.07 : 0
        }
    }
    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight
        RowLayout {
            id: row
            anchors.centerIn: parent
            spacing: 6
            Icon {
                visible: chip.checked || chip.glyph !== ""
                name: chip.checked ? "check" : chip.glyph
                size: 18
                color: chip.checked ? theme.onAccentContainer : theme.textSoft
            }
            Text {
                text: chip.text
                font: chip.font
                color: chip.checked ? theme.onAccentContainer : theme.textSoft
            }
        }
    }
}
