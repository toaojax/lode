import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A filled text field in the tonal style, with an optional leading icon.
// The label sits above the box rather than floating inside it.
ColumnLayout {
    id: field
    property alias text: input.text
    property alias placeholder: placeholderText.text
    property alias input: input
    property string label: ""
    property string icon: ""
    property string help: ""
    property bool secret: false
    signal accepted()
    signal edited()

    spacing: 6

    Text {
        visible: field.label !== ""
        text: field.label
        color: theme.textSoft
        font.pixelSize: 12
        font.weight: Font.Medium
    }
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 44
        radius: Math.min(12, theme.radius)
        color: theme.surfaceHighest
        border.width: input.activeFocus ? 2 : 0
        border.color: theme.accent

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: field.icon !== "" ? 12 : 14
            anchors.rightMargin: 12
            spacing: 10
            Icon {
                visible: field.icon !== ""
                name: field.icon
                color: theme.muted
            }
            TextInput {
                id: input
                Layout.fillWidth: true
                color: theme.text
                selectionColor: theme.accentContainer
                selectedTextColor: theme.onAccentContainer
                font.pixelSize: 14
                font.family: win.font.family
                clip: true
                echoMode: field.secret && !reveal.checked ? TextInput.Password : TextInput.Normal
                selectByMouse: true
                onAccepted: field.accepted()
                onTextEdited: field.edited()
                Text {
                    id: placeholderText
                    anchors.fill: parent
                    verticalAlignment: Text.AlignVCenter
                    visible: input.text === "" && !input.preeditText
                    color: theme.muted
                    font: input.font
                    elide: Text.ElideRight
                }
            }
            TButton {
                id: reveal
                visible: field.secret
                kind: "text"
                compact: true
                checkable: true
                glyph: checked ? "visibility_off" : "visibility"
                hint: checked ? qsTr("Hide") : qsTr("Show")
                implicitHeight: 28
            }
        }
        TapHandler { onTapped: input.forceActiveFocus() }
    }
    Text {
        visible: field.help !== ""
        text: field.help
        color: theme.muted
        font.pixelSize: 12
        wrapMode: Text.Wrap
        Layout.fillWidth: true
        textFormat: Text.StyledText
        linkColor: theme.accent
        onLinkActivated: link => backend.openUrl(link)
    }
}
