import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page

    InstanceView {
        anchors.fill: parent
        visible: win.selectedId !== ""
        instanceId: win.selectedId
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        anchors.bottomMargin: 0
        spacing: 20
        visible: win.selectedId === ""

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            ColumnLayout {
                spacing: 4
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Library")
                    color: theme.text
                    font.pixelSize: 32
                    font.weight: Font.Normal
                }
                Text {
                    text: instances.count === 0 ? qsTr("Nothing here yet") : qsTr("%n instance(s)", "", instances.count)
                    color: theme.muted
                    font.pixelSize: 14
                }
            }
            TButton { kind: "outlined"; glyph: "inventory_2"; text: qsTr("Find a modpack"); onClicked: { catalog.projectType = "modpack"; win.showPage(1) } }
            TButton { kind: "filled"; glyph: "add"; text: qsTr("New instance"); onClicked: newInstance.open() }
        }

        // First run: say what to do instead of showing an empty grid.
        Surface {
            visible: instances.count === 0
            level: 1
            Layout.fillWidth: true
            Layout.preferredHeight: 260
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 14
                width: Math.min(parent.width - 48, 460)
                Icon { name: "deployed_code"; size: 56; color: theme.accent; Layout.alignment: Qt.AlignHCenter }
                Text {
                    text: qsTr("Make your first instance")
                    color: theme.text
                    font.pixelSize: 22
                    Layout.alignment: Qt.AlignHCenter
                }
                Text {
                    text: qsTr("An instance is its own Minecraft: a version, a mod loader, and its own mods, worlds and settings. Start from scratch, install a modpack, or drop a .mrpack file here.")
                    color: theme.textSoft
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }
            }
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: instances.count > 0
            clip: true
            model: instances
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds
            readonly property int columns: Math.max(1, Math.floor(width / 260))
            cellWidth: Math.floor(width / columns)
            cellHeight: 212
            bottomMargin: 24
            ScrollBar.vertical: ScrollBar {}

            delegate: Item {
                id: cell
                required property string instanceId
                required property string name
                required property string minecraft
                required property string loader
                required property string icon
                required property var lastPlayed
                required property int index
                width: grid.cellWidth
                height: grid.cellHeight
                readonly property string state_: { launcher.running; launcher.preparingId; return launcher.stateOf(instanceId) }

                Surface {
                    id: card
                    anchors.fill: parent
                    anchors.margins: 6
                    level: hover.hovered ? 2 : 1
                    Behavior on color { ColorAnimation { duration: 100 } }

                    HoverHandler { id: hover }
                    TapHandler { onTapped: win.openInstance(cell.instanceId) }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 10
                        RowLayout {
                            Layout.fillWidth: true
                            Avatar { size: 56; source: cell.icon; label: cell.name }
                            Item { Layout.fillWidth: true }
                            Rectangle {
                                visible: cell.state_ !== "idle"
                                implicitWidth: stateText.implicitWidth + 20
                                implicitHeight: 26
                                radius: Math.min(13, theme.radius)
                                color: cell.state_ === "running" ? Qt.rgba(theme.green.r, theme.green.g, theme.green.b, 0.18) : theme.accentContainer
                                Text {
                                    id: stateText
                                    anchors.centerIn: parent
                                    text: cell.state_ === "running" ? qsTr("Running") : qsTr("Preparing")
                                    color: cell.state_ === "running" ? theme.green : theme.onAccentContainer
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                }
                            }
                        }
                        Item { Layout.fillHeight: true }
                        Text {
                            text: cell.name
                            color: theme.text
                            font.pixelSize: 16
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Text {
                                text: cell.loader === "vanilla" ? cell.minecraft : cell.minecraft + " · " + backend.loaderName(cell.loader)
                                color: theme.textSoft
                                font.pixelSize: 13
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            TButton {
                                kind: cell.state_ === "running" ? "danger" : "filled"
                                compact: true
                                glyph: cell.state_ === "running" ? "stop" : "play_arrow"
                                hint: cell.state_ === "running" ? qsTr("Stop") : qsTr("Play")
                                enabled: cell.state_ !== "preparing" && (cell.state_ === "running" || launcher.preparingId === "")
                                opacity: hover.hovered || cell.state_ !== "idle" ? 1 : 0
                                Behavior on opacity { NumberAnimation { duration: 100 } }
                                onClicked: cell.state_ === "running" ? launcher.kill(cell.instanceId) : win.play(cell.instanceId)
                            }
                        }
                        Text {
                            text: qsTr("Played %1").arg(win.relativeDate(cell.lastPlayed))
                            color: theme.muted
                            font.pixelSize: 12
                            visible: cell.lastPlayed && !isNaN(cell.lastPlayed.getTime())
                        }
                    }
                }
            }
        }
    }
}
