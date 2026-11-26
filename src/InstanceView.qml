import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// One instance: play it, manage what's in it, read its log, tune it.
Item {
    id: view
    property string instanceId: ""
    property int tab: 0
    readonly property var info: { instances.count; launcher.running; return instanceId !== "" ? instances.get(instanceId) : ({}) }
    readonly property string state_: { launcher.running; launcher.preparingId; return launcher.stateOf(instanceId) }
    readonly property var logModel: instanceId !== "" ? launcher.log(instanceId) : null

    onInstanceIdChanged: {
        tab = 0
        if (instanceId !== "") content.instanceId = instanceId
    }
    onVisibleChanged: if (visible && instanceId !== "") { content.instanceId = instanceId; content.refresh() }
    // A failed launch opens the log, where the reason is.
    Connections {
        target: launcher
        function onFailed(id, message) { if (id === view.instanceId) view.tab = 1 }
        function onExited(id, code, crashed) { if (id === view.instanceId && crashed) view.tab = 1 }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        anchors.bottomMargin: 0
        spacing: 20

        // Header
        RowLayout {
            Layout.fillWidth: true
            spacing: 16
            TButton { kind: "text"; glyph: "arrow_back"; hint: qsTr("Back to the library (Esc)"); onClicked: win.selectedId = "" }
            Avatar { size: 64; source: view.info.icon || ""; label: view.info.name || "" }
            ColumnLayout {
                spacing: 4
                Layout.fillWidth: true
                Text {
                    text: view.info.name || ""
                    color: theme.text
                    font.pixelSize: 28
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                RowLayout {
                    spacing: 8
                    Tag { text: view.info.minecraft || "" }
                    Tag { visible: !!view.info.loader && view.info.loader !== "vanilla"; text: backend.loaderName(view.info.loader || "") + " " + (view.info.loaderVersion || "").replace((view.info.minecraft || "") + "-", "") }
                    Tag { visible: !!view.info.pack && !!view.info.pack.provider; text: view.info.pack ? qsTr("Modpack %1").arg(view.info.pack.number || "") : "" }
                    Text {
                        text: win.formatPlaytime(view.info.playSeconds || 0, view.info.lastPlayed)
                        color: theme.muted
                        font.pixelSize: 13
                    }
                }
            }
            TButton {
                kind: "outlined"
                glyph: "folder_open"
                hint: qsTr("Open the game folder")
                onClicked: backend.openFolder(view.info.folder)
            }
            TButton {
                kind: view.state_ === "running" ? "danger" : "filled"
                glyph: view.state_ === "running" ? "stop" : view.state_ === "preparing" ? "hourglass_top" : "play_arrow"
                text: view.state_ === "running" ? qsTr("Stop") : view.state_ === "preparing" ? qsTr("Preparing") : qsTr("Play")
                implicitHeight: 48
                implicitWidth: 140
                enabled: view.state_ === "running" || (view.state_ === "idle" && launcher.preparingId === "")
                onClicked: view.state_ === "running"
                           ? win.confirm(qsTr("Stop the game?"), qsTr("Anything not saved in the game will be lost."), qsTr("Stop"), true, () => launcher.kill(view.instanceId))
                           : win.play(view.instanceId)
            }
        }

        // Material 3 primary tabs.
        RowLayout {
            Layout.fillWidth: true
            spacing: 0
            Repeater {
                model: [qsTr("Content"), qsTr("Log"), qsTr("Options")]
                delegate: AbstractButton {
                    id: tabButton
                    required property string modelData
                    required property int index
                    readonly property bool on: view.tab === index
                    implicitHeight: 48
                    implicitWidth: Math.max(96, tabLabel.implicitWidth + 32)
                    hoverEnabled: true
                    onClicked: view.tab = index
                    background: Rectangle {
                        color: tabButton.hovered ? Qt.rgba(theme.text.r, theme.text.g, theme.text.b, 0.05) : "transparent"
                        radius: Math.min(8, theme.radius)
                    }
                    contentItem: Item {
                        Text {
                            id: tabLabel
                            anchors.centerIn: parent
                            text: tabButton.modelData
                            color: tabButton.on ? theme.accent : theme.textSoft
                            font.pixelSize: 14
                            font.weight: Font.Medium
                        }
                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: tabLabel.implicitWidth + 8
                            height: 3
                            radius: 1.5
                            color: theme.accent
                            visible: tabButton.on
                        }
                    }
                }
            }
            Item { Layout.fillWidth: true }
        }
        Rectangle { Layout.fillWidth: true; Layout.topMargin: -20; implicitHeight: 1; color: theme.outlineVariant }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: view.tab

            ContentTab { instanceId: view.instanceId; info: view.info }
            LogTab { logModel: view.logModel }
            OptionsTab { instanceId: view.instanceId; info: view.info }
        }
    }
}
