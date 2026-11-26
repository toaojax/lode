import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Mods, resource packs and shaders in one instance.
ColumnLayout {
    id: tab
    property string instanceId: ""
    property var info: ({})
    spacing: 14

    readonly property var categories: [
        { value: "mods", label: qsTr("Mods"), type: "mod" },
        { value: "resourcepacks", label: qsTr("Resource packs"), type: "resourcepack" },
        { value: "shaderpacks", label: qsTr("Shaders"), type: "shader" }
    ]
    readonly property var current: categories.find(c => c.value === content.category) || categories[0]

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 4
        spacing: 10
        Segmented {
            options: tab.categories
            value: content.category
            onPicked: value => content.category = value
        }
        Item { Layout.fillWidth: true }
        TButton {
            kind: "text"
            compact: true
            glyph: "refresh"
            text: content.checkedUpdates && content.updateCount === 0 ? qsTr("Up to date") : qsTr("Check for updates")
            enabled: !content.busy && content.count > 0
            onClicked: content.checkUpdates()
        }
        TButton {
            visible: content.updateCount > 0
            kind: "tonal"
            compact: true
            glyph: "upgrade"
            text: qsTr("Update all (%1)").arg(content.updateCount)
            enabled: !content.busy
            onClicked: content.updateAll()
        }
        TButton {
            kind: "filled"
            compact: true
            glyph: "add"
            text: qsTr("Add %1").arg(tab.current.label.toLowerCase())
            onClicked: {
                browsePage.target = tab.instanceId
                catalog.projectType = tab.current.type
                win.showPage(1)
            }
        }
    }

    Surface {
        visible: content.count === 0
        level: 1
        Layout.fillWidth: true
        Layout.preferredHeight: 180
        ColumnLayout {
            anchors.centerIn: parent
            spacing: 10
            Icon { name: tab.current.value === "mods" ? "extension" : tab.current.value === "shaderpacks" ? "wb_sunny" : "texture"; size: 40; color: theme.muted; Layout.alignment: Qt.AlignHCenter }
            Text {
                text: tab.info.loader === "vanilla" && tab.current.value === "mods"
                      ? qsTr("Vanilla instances don't load mods. Make one with Fabric, Quilt, Forge or NeoForge.")
                      : qsTr("No %1 yet. Add some from Browse, or drop files into the folder.").arg(tab.current.label.toLowerCase())
                color: theme.textSoft
                Layout.alignment: Qt.AlignHCenter
            }
            TButton { kind: "text"; compact: true; glyph: "folder_open"; text: qsTr("Open folder"); Layout.alignment: Qt.AlignHCenter; onClicked: backend.openFolder(content.folderPath()) }
        }
    }

    ListView {
        id: list
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: content.count > 0
        clip: true
        model: content
        spacing: 4
        reuseItems: true
        bottomMargin: 24
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        delegate: Surface {
            id: row
            required property string file
            required property string title
            required property string version
            required property string icon
            required property bool active
            required property string provider
            required property string newer
            required property int index
            width: list.width
            height: 64
            level: rowHover.hovered ? 2 : 1
            radius: Math.min(16, theme.radius)
            HoverHandler { id: rowHover }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 8
                spacing: 14
                Avatar { size: 40; source: row.icon; label: row.title; opacity: row.active ? 1 : 0.45 }
                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true
                    Text {
                        text: row.title
                        color: row.active ? theme.text : theme.muted
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: row.version !== "" ? row.version + "  ·  " + row.file : row.file
                        color: theme.muted
                        font.pixelSize: 12
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                }
                Tag {
                    visible: row.provider !== ""
                    text: row.provider === "curseforge" ? "CurseForge" : "Modrinth"
                    tint: theme.surfaceHighest
                    ink: theme.textSoft
                }
                TButton {
                    visible: row.newer !== ""
                    kind: "tonal"
                    compact: true
                    glyph: "upgrade"
                    text: row.newer
                    hint: qsTr("Update to %1").arg(row.newer)
                    enabled: !content.busy
                    onClicked: content.update(row.file)
                }
                Switch {
                    checked: row.active
                    onToggled: content.toggle(row.file)
                    ToolTip.visible: hovered
                    ToolTip.delay: 600
                    ToolTip.text: row.active ? qsTr("Turn off") : qsTr("Turn on")
                }
                TButton {
                    kind: "text"
                    compact: true
                    glyph: "delete"
                    hint: qsTr("Move to the trash")
                    opacity: rowHover.hovered ? 1 : 0
                    onClicked: content.remove(row.file)
                }
            }
        }
    }
}
