import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ScrollView {
    id: tab
    property string instanceId: ""
    property var info: ({})
    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        width: Math.min(tab.availableWidth, 720)
        spacing: 16

        Surface {
            Layout.fillWidth: true
            Layout.topMargin: 4
            level: 1
            implicitHeight: general.implicitHeight + 40
            ColumnLayout {
                id: general
                anchors.fill: parent
                anchors.margins: 20
                spacing: 16
                Text { text: qsTr("General"); color: theme.text; font.pixelSize: 16; font.weight: Font.Medium }
                Field {
                    Layout.fillWidth: true
                    label: qsTr("Name")
                    text: tab.info.name || ""
                    onAccepted: instances.rename(tab.instanceId, text)
                    input.onActiveFocusChanged: if (!input.activeFocus && text !== tab.info.name) instances.rename(tab.instanceId, text)
                }
            }
        }

        Surface {
            Layout.fillWidth: true
            level: 1
            implicitHeight: java.implicitHeight + 40
            ColumnLayout {
                id: java
                anchors.fill: parent
                anchors.margins: 20
                spacing: 14
                Text { text: qsTr("Java"); color: theme.text; font.pixelSize: 16; font.weight: Font.Medium }

                RowLayout {
                    Layout.fillWidth: true
                    Switch {
                        id: ownMemory
                        checked: (tab.info.memoryMb || 0) > 0
                        onToggled: instances.setMemory(tab.instanceId, checked ? settings.memoryMb : 0)
                    }
                    Text {
                        text: ownMemory.checked ? qsTr("Memory for this instance") : qsTr("Memory: the default (%1 GB)").arg((settings.memoryMb / 1024).toLocaleString(Qt.locale(), 'f', 1))
                        color: theme.text
                        Layout.fillWidth: true
                    }
                    Text {
                        visible: ownMemory.checked
                        text: qsTr("%1 GB").arg((memory.value / 1024).toLocaleString(Qt.locale(), 'f', 1))
                        color: theme.accent
                        font.weight: Font.Medium
                    }
                }
                Slider {
                    id: memory
                    visible: ownMemory.checked
                    Layout.fillWidth: true
                    from: 1024
                    to: Math.max(2048, settings.totalMemoryMb - 1024)
                    stepSize: 512
                    snapMode: Slider.SnapAlways
                    value: (tab.info.memoryMb || 0) > 0 ? tab.info.memoryMb : settings.memoryMb
                    onPressedChanged: if (!pressed) instances.setMemory(tab.instanceId, value)
                }
                Field {
                    Layout.fillWidth: true
                    label: qsTr("Extra Java arguments")
                    placeholder: "-XX:+UseZGC"
                    text: tab.info.javaArgs || ""
                    help: qsTr("Added after the defaults in Settings.")
                    onAccepted: instances.setJavaArgs(tab.instanceId, text)
                    input.onActiveFocusChanged: if (!input.activeFocus) instances.setJavaArgs(tab.instanceId, text)
                }
            }
        }

        Surface {
            Layout.fillWidth: true
            Layout.bottomMargin: 24
            level: 1
            implicitHeight: manage.implicitHeight + 40
            ColumnLayout {
                id: manage
                anchors.fill: parent
                anchors.margins: 20
                spacing: 14
                Text { text: qsTr("Manage"); color: theme.text; font.pixelSize: 16; font.weight: Font.Medium }
                Text {
                    text: qsTr("Duplicating copies worlds, mods and settings. Deleting moves the whole instance to the trash, so worlds can still be recovered.")
                    color: theme.textSoft
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                RowLayout {
                    spacing: 8
                    TButton {
                        kind: "outlined"
                        glyph: "content_copy"
                        text: qsTr("Duplicate")
                        onClicked: {
                            const id = instances.duplicate(tab.instanceId)
                            if (id) win.openInstance(id)
                            else win.toast(qsTr("Couldn't duplicate the instance"), "error")
                        }
                    }
                    TButton {
                        kind: "danger"
                        glyph: "delete"
                        text: qsTr("Delete")
                        enabled: launcher.stateOf(tab.instanceId) === "idle"
                        onClicked: win.confirm(qsTr("Delete %1?").arg(tab.info.name), qsTr("The instance, with its worlds and mods, goes to the trash."), qsTr("Delete"), true, () => {
                            if (instances.remove(tab.instanceId)) win.selectedId = ""
                            else win.toast(qsTr("Couldn't move it to the trash"), "error")
                        })
                    }
                }
            }
        }
    }
}
