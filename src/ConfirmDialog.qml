import QtQuick
import QtQuick.Layouts

Sheet {
    id: dialog
    property string heading: ""
    property string message: ""
    property string acceptText: ""
    property bool danger: false
    property var action: null

    function ask(title, text, accept, isDanger, callback) {
        heading = title
        message = text
        acceptText = accept
        danger = isDanger
        action = callback
        open()
    }

    contentItem: ColumnLayout {
        spacing: 16
        Text {
            text: dialog.heading
            color: theme.text
            font.pixelSize: 22
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Text {
            text: dialog.message
            color: theme.textSoft
            font.pixelSize: 14
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            Layout.topMargin: 8
            spacing: 8
            TButton { kind: "text"; text: qsTr("Cancel"); onClicked: dialog.close() }
            TButton {
                kind: dialog.danger ? "danger" : "filled"
                text: dialog.acceptText
                onClicked: {
                    dialog.close()
                    if (dialog.action) dialog.action()
                }
            }
        }
    }
}
