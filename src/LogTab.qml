import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The game's output. It follows new lines while you're at the bottom and
// stays put when you scroll up to read.
ColumnLayout {
    id: tab
    property var logModel: null
    spacing: 12

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 4
        spacing: 8
        Text {
            text: tab.logModel && tab.logModel.count > 0 ? qsTr("%n line(s)", "", tab.logModel.count) : qsTr("Nothing logged yet. Press Play.")
            color: theme.muted
            font.pixelSize: 13
            Layout.fillWidth: true
        }
        TButton { kind: "text"; compact: true; glyph: "content_copy"; text: qsTr("Copy"); enabled: tab.logModel && tab.logModel.count > 0; onClicked: { tab.logModel.copy(); win.toast(qsTr("Log copied"), "ok") } }
        TButton { kind: "text"; compact: true; glyph: "delete"; text: qsTr("Clear"); enabled: tab.logModel && tab.logModel.count > 0; onClicked: tab.logModel.clear() }
    }

    Surface {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.bottomMargin: 20
        level: 0
        ListView {
            id: list
            anchors.fill: parent
            anchors.margins: 12
            clip: true
            model: tab.logModel
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds
            property bool follow: true
            onMovementEnded: follow = atYEnd
            onCountChanged: if (follow) Qt.callLater(positionViewAtEnd)
            ScrollBar.vertical: ScrollBar {}
            delegate: Text {
                required property string line
                required property string level
                width: list.width - 12
                text: line
                wrapMode: Text.WrapAnywhere
                font.family: "monospace"
                font.pixelSize: 12
                textFormat: Text.PlainText
                color: level === "error" ? theme.red : level === "warn" ? theme.yellow : level === "lode" ? theme.accent : theme.textSoft
            }
        }
    }
}
