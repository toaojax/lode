import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ScrollView {
    id: page
    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        width: Math.min(page.availableWidth - 56, 760)
        x: 28
        spacing: 20

        ColumnLayout {
            Layout.topMargin: 28
            spacing: 4
            Text { text: qsTr("Settings"); color: theme.text; font.pixelSize: 32 }
            Text { text: qsTr("Saved in %1").arg(settings.path); color: theme.muted; font.pixelSize: 12 }
        }

        Surface {
            Layout.fillWidth: true
            level: 1
            implicitHeight: game.implicitHeight + 40
            ColumnLayout {
                id: game
                anchors.fill: parent
                anchors.margins: 20
                spacing: 16
                Text { text: qsTr("Game"); color: theme.text; font.pixelSize: 16; font.weight: Font.Medium }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Default memory"); color: theme.text; Layout.fillWidth: true }
                    Text {
                        text: qsTr("%1 GB of %2 GB").arg((memory.value / 1024).toLocaleString(Qt.locale(), 'f', 1))
                                                    .arg((settings.totalMemoryMb / 1024).toLocaleString(Qt.locale(), 'f', 0))
                        color: theme.accent
                        font.weight: Font.Medium
                    }
                }
                Slider {
                    id: memory
                    Layout.fillWidth: true
                    from: 1024
                    to: Math.max(2048, settings.totalMemoryMb - 1024)
                    stepSize: 512
                    snapMode: Slider.SnapAlways
                    value: settings.memoryMb
                    onPressedChanged: if (!pressed) settings.memoryMb = value
                }
                Field {
                    Layout.fillWidth: true
                    label: qsTr("Java arguments for every instance")
                    placeholder: "-XX:+UseG1GC"
                    text: settings.javaArgs
                    input.onEditingFinished: settings.javaArgs = text
                }
                Field {
                    Layout.fillWidth: true
                    label: qsTr("Wrapper command")
                    placeholder: "gamemoderun"
                    help: qsTr("Runs in front of Java, like gamemoderun, mangohud or prime-run.")
                    text: settings.wrapperCommand
                    input.onEditingFinished: settings.wrapperCommand = text
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Field {
                        Layout.preferredWidth: 140
                        label: qsTr("Window width")
                        placeholder: qsTr("Default")
                        text: settings.windowWidth > 0 ? settings.windowWidth : ""
                        input.validator: IntValidator { bottom: 0; top: 16384 }
                        input.onEditingFinished: settings.windowWidth = parseInt(text) || 0
                    }
                    Field {
                        Layout.preferredWidth: 140
                        label: qsTr("Window height")
                        placeholder: qsTr("Default")
                        text: settings.windowHeight > 0 ? settings.windowHeight : ""
                        input.validator: IntValidator { bottom: 0; top: 16384 }
                        input.onEditingFinished: settings.windowHeight = parseInt(text) || 0
                    }
                    Item { Layout.fillWidth: true }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Minimize lode when a game starts"); color: theme.text; Layout.fillWidth: true }
                    Switch { checked: settings.closeOnLaunch; onToggled: settings.closeOnLaunch = checked }
                }
            }
        }

        Surface {
            Layout.fillWidth: true
            level: 1
            implicitHeight: keys.implicitHeight + 40
            ColumnLayout {
                id: keys
                anchors.fill: parent
                anchors.margins: 20
                spacing: 16
                Text { text: qsTr("Services"); color: theme.text; font.pixelSize: 16; font.weight: Font.Medium }
                Field {
                    Layout.fillWidth: true
                    label: qsTr("Microsoft client ID")
                    placeholder: "00000000-0000-0000-0000-000000000000"
                    text: settings.microsoftClientId
                    help: qsTr("From an Azure app registration with personal Microsoft accounts and public client flows allowed. Mojang must approve it for Minecraft at <a href=\"https://aka.ms/mce-reviewappid\">aka.ms/mce-reviewappid</a>.")
                    input.onEditingFinished: settings.microsoftClientId = text.trim()
                }
                Field {
                    Layout.fillWidth: true
                    label: qsTr("CurseForge API key")
                    secret: true
                    text: settings.curseForgeKey
                    help: qsTr("Free from <a href=\"https://console.curseforge.com/\">console.curseforge.com</a>. Modrinth needs no key.")
                    input.onEditingFinished: settings.curseForgeKey = text.trim()
                }
            }
        }

        Surface {
            Layout.fillWidth: true
            Layout.bottomMargin: 28
            level: 1
            implicitHeight: about.implicitHeight + 40
            ColumnLayout {
                id: about
                anchors.fill: parent
                anchors.margins: 20
                spacing: 12
                Text { text: qsTr("Storage"); color: theme.text; font.pixelSize: 16; font.weight: Font.Medium }
                Text {
                    text: qsTr("Instances, Java runtimes, libraries and game assets live in %1. Libraries and assets are shared, so a new instance of a version you already have downloads almost nothing.").arg(backend.dataPath)
                    color: theme.textSoft
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                RowLayout {
                    spacing: 8
                    TButton { kind: "outlined"; glyph: "folder_open"; text: qsTr("Open data folder"); onClicked: backend.openFolder(backend.dataPath) }
                    TButton { kind: "text"; glyph: "refresh"; text: qsTr("Refresh version list"); onClicked: backend.loadGameVersions(true) }
                }
                Text { text: qsTr("lode %1").arg(backend.version); color: theme.muted; font.pixelSize: 12 }
            }
        }
    }
}
