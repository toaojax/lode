import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Files whose authors only allow downloads from CurseForge's site. lode
// doesn't route around that choice; it lists them with a link each and the
// folder they go in.
Sheet {
    id: dialog
    property string heading: ""
    property string message: ""
    property var files: []
    width: Math.min(620, parent ? parent.width - 64 : 620)

    function show(title, text, blocked) {
        heading = title
        message = text
        files = blocked
        open()
    }

    contentItem: ColumnLayout {
        spacing: 14
        Text {
            text: qsTr("%1 needs a hand").arg(dialog.heading)
            color: theme.text
            font.pixelSize: 22
            font.weight: Font.DemiBold
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        Text {
            text: dialog.message
            color: theme.textSoft
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 320)
            clip: true
            spacing: 6
            model: dialog.files
            boundsBehavior: Flickable.StopAtBounds
            delegate: Surface {
                required property var modelData
                level: 2
                width: ListView.view.width
                height: 56
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 8
                    spacing: 8
                    ColumnLayout {
                        spacing: 2
                        Layout.fillWidth: true
                        Text { text: modelData.title; color: theme.text; elide: Text.ElideRight; Layout.fillWidth: true }
                        Text { text: modelData.file; color: theme.muted; font.pixelSize: 12; elide: Text.ElideMiddle; Layout.fillWidth: true }
                    }
                    TButton { kind: "text"; compact: true; glyph: "open_in_new"; hint: qsTr("Open on CurseForge"); onClicked: backend.openUrl(modelData.url) }
                    TButton {
                        visible: !!modelData.folder
                        kind: "text"; compact: true; glyph: "folder_open"; hint: qsTr("Open the folder it goes in")
                        onClicked: backend.openFolder(modelData.folder)
                    }
                }
            }
        }
        TButton {
            Layout.alignment: Qt.AlignRight
            kind: "filled"
            text: qsTr("Done")
            onClicked: dialog.close()
        }
    }
}
