import QtQuick
import QtQuick.Layouts

// Material snackbars: short notices that stack at the bottom and leave on
// their own. Errors stay a little longer.
Item {
    id: root
    anchors.fill: parent

    function show(message, kind) {
        model.append({ message: message, kind: kind || "info" })
        if (model.count > 3) model.remove(0)
    }

    ListModel { id: model }

    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 72
        spacing: 8
        width: Math.min(560, root.width - 140)

        Repeater {
            model: model
            delegate: Rectangle {
                id: snack
                required property string message
                required property string kind
                required property int index
                width: parent.width
                height: row.implicitHeight + 28
                radius: Math.min(12, theme.radius)
                color: theme.surfaceHighest
                border.width: 1
                border.color: kind === "error" ? Qt.rgba(theme.red.r, theme.red.g, theme.red.b, 0.5) : theme.outlineVariant

                RowLayout {
                    id: row
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12
                    Icon {
                        name: snack.kind === "error" ? "error" : snack.kind === "ok" ? "check_circle" : "info"
                        color: snack.kind === "error" ? theme.red : snack.kind === "ok" ? theme.green : theme.accent
                        filled: true
                    }
                    Text {
                        text: snack.message
                        color: theme.text
                        wrapMode: Text.Wrap
                        font.pixelSize: 13
                        Layout.fillWidth: true
                    }
                    Icon {
                        name: "close"
                        size: 18
                        color: theme.muted
                        TapHandler { onTapped: model.remove(snack.index) }
                    }
                }
                Timer {
                    running: true
                    interval: snack.kind === "error" ? 8000 : 4000
                    onTriggered: if (snack.index >= 0 && snack.index < model.count) model.remove(snack.index)
                }
            }
        }
    }
}
