import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Whatever is downloading right now, with a way to stop a launch.
Rectangle {
    id: strip
    readonly property string preparingName: launcher.preparingId !== "" ? (instances.get(launcher.preparingId).name || "") : ""
    readonly property string text: launcher.preparingId !== "" ? (launcher.status !== "" ? launcher.status : qsTr("Getting %1 ready").arg(preparingName))
                                 : modpacks.busy ? modpacks.status
                                 : content.busy ? content.status : ""
    readonly property real value: launcher.preparingId !== "" ? launcher.progress : modpacks.busy ? modpacks.progress : -1

    visible: text !== ""
    implicitHeight: visible ? 52 : 0
    color: theme.surfaceLowest

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 16
        spacing: 14

        BusyIndicator {
            running: strip.visible
            implicitWidth: 28
            implicitHeight: 28
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            Text {
                text: strip.preparingName !== "" ? strip.preparingName + " · " + strip.text : strip.text
                color: theme.text
                font.pixelSize: 13
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            // Material's linear indicator: a track and the accent over it.
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 4
                radius: 2
                color: theme.surfaceHighest
                visible: strip.value >= 0
                Rectangle {
                    width: parent.width * Math.max(0, Math.min(1, strip.value))
                    height: parent.height
                    radius: 2
                    color: theme.accent
                    Behavior on width { NumberAnimation { duration: 160 } }
                }
            }
        }
        TButton {
            visible: launcher.preparingId !== ""
            kind: "text"
            compact: true
            text: qsTr("Cancel")
            onClicked: launcher.cancel()
        }
    }
}
