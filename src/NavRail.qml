import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Material 3's navigation rail: a floating action for the main thing to
// make, the destinations below it, and who's playing at the bottom.
Rectangle {
    id: rail
    property int current: 0
    signal selected(int index)
    signal create()

    implicitWidth: 88
    color: theme.surfaceLowest

    readonly property var items: [
        { icon: "sports_esports", label: qsTr("Library") },
        { icon: "explore", label: qsTr("Browse") },
        { icon: "account_circle", label: qsTr("Accounts") },
        { icon: "settings", label: qsTr("Settings") }
    ]

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 20
        anchors.bottomMargin: 16
        spacing: 12

        // The FAB: a new instance.
        AbstractButton {
            id: fab
            Layout.alignment: Qt.AlignHCenter
            implicitWidth: 56
            implicitHeight: 56
            hoverEnabled: true
            Accessible.name: qsTr("New instance")
            ToolTip.visible: hovered
            ToolTip.delay: 500
            ToolTip.text: qsTr("New instance (Ctrl+N)")
            onClicked: rail.create()
            background: Rectangle {
                radius: Math.min(16, theme.radius)
                color: theme.accentContainer
                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    color: theme.onAccentContainer
                    opacity: fab.down ? 0.16 : fab.hovered ? 0.08 : 0
                }
            }
            contentItem: Icon { name: "add"; size: 26; color: theme.onAccentContainer }
        }

        Item { implicitHeight: 12 }

        Repeater {
            model: rail.items
            delegate: AbstractButton {
                id: dest
                required property var modelData
                required property int index
                readonly property bool active: rail.current === index
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 80
                implicitHeight: 60
                hoverEnabled: true
                Accessible.name: modelData.label
                onClicked: rail.selected(index)

                contentItem: Column {
                    spacing: 4
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 56
                        height: 32
                        radius: Math.min(16, theme.radius)
                        color: dest.active ? theme.accentContainer
                             : dest.hovered ? Qt.rgba(theme.text.r, theme.text.g, theme.text.b, 0.08) : "transparent"
                        Behavior on color { ColorAnimation { duration: 120 } }
                        Icon {
                            anchors.centerIn: parent
                            name: dest.modelData.icon
                            filled: dest.active
                            size: 24
                            color: dest.active ? theme.onAccentContainer : theme.textSoft
                        }
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: dest.modelData.label
                        font.pixelSize: 12
                        font.weight: dest.active ? Font.DemiBold : Font.Medium
                        color: dest.active ? theme.text : theme.textSoft
                        width: 80
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }
                }
                background: Item {}
            }
        }

        Item { Layout.fillHeight: true }

        // The active account, a click away from switching.
        AbstractButton {
            id: who
            Layout.alignment: Qt.AlignHCenter
            implicitWidth: 48
            implicitHeight: 48
            hoverEnabled: true
            onClicked: rail.selected(2)
            ToolTip.visible: hovered
            ToolTip.delay: 400
            ToolTip.text: accounts.activeName !== "" ? qsTr("Playing as %1").arg(accounts.activeName) : qsTr("No account yet")
            background: Item {}
            contentItem: Avatar {
                size: 40
                label: accounts.activeName !== "" ? accounts.activeName : "?"
                radius: 20
                color: accounts.activeName !== "" ? theme.accentContainer : theme.surfaceHighest
            }
        }
    }
}
