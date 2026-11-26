import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// A new instance from scratch: a game version, a loader and its version.
Sheet {
    id: dialog
    width: Math.min(600, parent ? parent.width - 64 : 600)
    property string loader: "fabric"
    property bool snapshots: false
    property var loaderVersions: []
    property string loaderError: ""
    property bool loadingLoaders: false
    property string pickedVersion: ""

    readonly property var versions: {
        const all = backend.gameVersions
        return snapshots ? all : all.filter(v => v.type === "release")
    }
    readonly property string minecraft: pickedVersion !== "" ? pickedVersion : backend.latestRelease

    function refreshLoaders() {
        loaderVersions = []
        loaderError = ""
        if (loader === "vanilla" || minecraft === "") return
        loadingLoaders = true
        backend.listLoaderVersions(loader, minecraft)
    }
    function create() {
        const lv = loader === "vanilla" ? "" : (loaderBox.currentValue || "")
        if (loader !== "vanilla" && lv === "") return
        const id = backend.createInstance(nameField.text, minecraft, loader, lv)
        if (id === "") return
        close()
        win.openInstance(id)
    }

    onOpened: {
        nameField.text = ""
        pickedVersion = ""
        if (backend.gameVersions.length === 0) backend.loadGameVersions()
        refreshLoaders()
        nameField.input.forceActiveFocus()
    }
    onLoaderChanged: refreshLoaders()
    onMinecraftChanged: if (visible) refreshLoaders()

    Connections {
        target: backend
        function onLoaderVersionsReady(loader, minecraft, versions, error) {
            if (loader !== dialog.loader || minecraft !== dialog.minecraft) return
            dialog.loadingLoaders = false
            dialog.loaderVersions = versions.map(v => ({ text: v.stable ? v.version : qsTr("%1 (beta)").arg(v.version), value: v.version, stable: v.stable }))
            dialog.loaderError = error
            const stable = dialog.loaderVersions.findIndex(v => v.stable)
            loaderBox.currentIndex = stable >= 0 ? stable : 0
        }
    }

    contentItem: ColumnLayout {
        spacing: 18
        Text { text: qsTr("New instance"); color: theme.text; font.pixelSize: 24 }

        Field {
            id: nameField
            Layout.fillWidth: true
            label: qsTr("Name")
            placeholder: dialog.loader === "vanilla" ? qsTr("Minecraft %1").arg(dialog.minecraft)
                                                     : qsTr("%1 %2").arg(backend.loaderName(dialog.loader)).arg(dialog.minecraft)
            onAccepted: dialog.create()
        }

        ColumnLayout {
            spacing: 8
            Text { text: qsTr("Mod loader"); color: theme.textSoft; font.pixelSize: 12; font.weight: Font.Medium }
            Segmented {
                options: [
                    { value: "vanilla", label: qsTr("Vanilla") }, { value: "fabric", label: "Fabric" },
                    { value: "quilt", label: "Quilt" }, { value: "forge", label: "Forge" }, { value: "neoforge", label: "NeoForge" }
                ]
                value: dialog.loader
                onPicked: value => dialog.loader = value
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 16
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                Text { text: qsTr("Minecraft version"); color: theme.textSoft; font.pixelSize: 12; font.weight: Font.Medium }
                ComboBox {
                    id: versionBox
                    Layout.fillWidth: true
                    model: dialog.versions.map(v => v.id)
                    currentIndex: Math.max(0, model.indexOf(dialog.minecraft))
                    onActivated: dialog.pickedVersion = currentText
                }
                RowLayout {
                    spacing: 4
                    CheckBox {
                        checked: dialog.snapshots
                        onToggled: dialog.snapshots = checked
                        padding: 0
                    }
                    Text { text: qsTr("Show snapshots and old versions"); color: theme.textSoft; font.pixelSize: 13 }
                }
            }
            ColumnLayout {
                visible: dialog.loader !== "vanilla"
                Layout.fillWidth: true
                spacing: 6
                Text { text: qsTr("%1 version").arg(backend.loaderName(dialog.loader)); color: theme.textSoft; font.pixelSize: 12; font.weight: Font.Medium }
                ComboBox {
                    id: loaderBox
                    Layout.fillWidth: true
                    enabled: dialog.loaderVersions.length > 0
                    textRole: "text"
                    valueRole: "value"
                    model: dialog.loaderVersions
                    displayText: dialog.loadingLoaders ? qsTr("Loading…") : dialog.loaderVersions.length === 0 ? qsTr("None") : currentText
                }
                Item { implicitHeight: 32 }
            }
        }

        Text {
            visible: dialog.loaderError !== ""
            text: dialog.loaderError
            color: theme.red
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Text {
            text: dialog.loader === "forge" || dialog.loader === "neoforge"
                  ? qsTr("The first launch runs the %1 installer, which takes a minute.").arg(backend.loaderName(dialog.loader))
                  : qsTr("Files download the first time you press Play.")
            color: theme.muted
            font.pixelSize: 12
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 8
            TButton { kind: "text"; text: qsTr("Cancel"); onClicked: dialog.close() }
            TButton {
                kind: "filled"
                glyph: "add"
                text: qsTr("Create")
                enabled: dialog.minecraft !== "" && (dialog.loader === "vanilla" || (dialog.loaderVersions.length > 0 && !dialog.loadingLoaders))
                onClicked: dialog.create()
            }
        }
    }
}
