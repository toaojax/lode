import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One search result. Kept to plain items and one async image so a fast
// scroll through hundreds of them stays smooth.
Surface {
    id: card
    required property string projectId
    required property string title
    required property string summary
    required property string author
    required property string icon
    required property var downloads
    required property var follows
    required property var categories
    required property int index
    property bool installed: false
    property bool selected: false
    property bool canInstall: true
    property bool pending: false
    property string installLabel: qsTr("Install")
    signal open()
    signal install()

    height: 96
    level: selected ? 3 : hover.hovered ? 2 : 1
    border.width: selected ? 1 : 0
    border.color: theme.outline

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: card.open() }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        Avatar { size: 64; source: card.icon; label: card.title }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Text {
                    text: card.title
                    color: theme.text
                    font.pixelSize: 16
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                    // The title keeps its room; the author gives way first.
                    Layout.maximumWidth: Math.min(implicitWidth, card.width * 0.5)
                }
                Text {
                    text: card.author !== "" ? qsTr("by %1").arg(card.author) : ""
                    color: theme.muted
                    font.pixelSize: 13
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
            Text {
                text: card.summary
                color: theme.textSoft
                font.pixelSize: 13
                elide: Text.ElideRight
                maximumLineCount: 1
                Layout.fillWidth: true
            }
            RowLayout {
                spacing: 14
                Row {
                    spacing: 4
                    Icon { name: "download"; size: 16; color: theme.muted; anchors.verticalCenter: parent.verticalCenter }
                    Text { text: win.formatCount(card.downloads); color: theme.muted; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                }
                Row {
                    spacing: 4
                    Icon { name: "favorite"; size: 16; color: theme.muted; anchors.verticalCenter: parent.verticalCenter }
                    Text { text: win.formatCount(card.follows); color: theme.muted; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                }
                Text {
                    text: (card.categories || []).slice(0, 3).join(" · ")
                    color: theme.muted
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        TButton {
            kind: card.installed ? "outlined" : "tonal"
            compact: true
            glyph: card.installed ? "check" : card.pending ? "hourglass_top" : "download"
            text: card.installed ? qsTr("Installed") : card.installLabel
            enabled: !card.installed && !card.pending && card.canInstall
            onClicked: card.install()
        }
    }
}
