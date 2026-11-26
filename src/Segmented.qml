import QtQuick
import QtQuick.Layouts

// Material 3 segmented buttons: one choice out of a few, side by side.
// options is a list of {value, label}.
Rectangle {
    id: seg
    property var options: []
    property string value: ""
    signal picked(string value)

    implicitHeight: 36
    implicitWidth: row.implicitWidth
    radius: Math.min(height / 2, theme.radius)
    color: "transparent"
    border.width: 1
    border.color: theme.outlineVariant
    clip: true

    Row {
        id: row
        anchors.fill: parent
        Repeater {
            model: seg.options
            delegate: Rectangle {
                id: cell
                required property var modelData
                required property int index
                readonly property bool on: seg.value === modelData.value
                width: label.implicitWidth + (on ? 52 : 32)
                height: seg.height
                color: on ? theme.accentContainer : hover.hovered ? Qt.rgba(theme.text.r, theme.text.g, theme.text.b, 0.06) : "transparent"
                Behavior on width { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

                Rectangle {
                    visible: cell.index > 0
                    width: 1
                    height: parent.height
                    color: theme.outlineVariant
                }
                Row {
                    anchors.centerIn: parent
                    spacing: 6
                    Icon {
                        visible: cell.on
                        name: "check"
                        size: 18
                        color: theme.onAccentContainer
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        id: label
                        text: cell.modelData.label
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        color: cell.on ? theme.onAccentContainer : theme.text
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
                TapHandler {
                    // The owner sets value, so a binding to it survives the tap.
                    onTapped: seg.picked(cell.modelData.value)
                }
            }
        }
    }
}
