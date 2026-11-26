import QtQuick
import QtQuick.Layouts

// Every shortcut, toggled with ?.
Rectangle {
    id: overlay
    signal close()
    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.55)

    TapHandler { onTapped: overlay.close() }

    readonly property var keys: [
        ["Space", qsTr("Play the selected instance")],
        ["Ctrl+N", qsTr("New instance")],
        ["/ or Ctrl+F", qsTr("Search mods and packs")],
        ["1 – 4", qsTr("Library, Browse, Accounts, Settings")],
        ["Enter", qsTr("Open the highlighted result")],
        ["Esc", qsTr("Back, or close what's open")],
        ["?", qsTr("Show or hide this list")],
        ["Q", qsTr("Quit")]
    ]

    Surface {
        anchors.centerIn: parent
        level: 3
        width: 460
        height: column.implicitHeight + 48
        TapHandler {}

        ColumnLayout {
            id: column
            anchors.fill: parent
            anchors.margins: 24
            spacing: 14
            Text {
                text: qsTr("Keyboard shortcuts")
                color: theme.text
                font.pixelSize: 22
                font.weight: Font.DemiBold
            }
            Repeater {
                model: overlay.keys
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 16
                    Rectangle {
                        implicitWidth: 110
                        implicitHeight: 28
                        radius: Math.min(8, theme.radius)
                        color: theme.accentContainer
                        Text {
                            anchors.centerIn: parent
                            text: modelData[0]
                            color: theme.onAccentContainer
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }
                    }
                    Text {
                        text: modelData[1]
                        color: theme.textSoft
                        font.pixelSize: 14
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }
}
