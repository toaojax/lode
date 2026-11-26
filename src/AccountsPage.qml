import QtQuick
import QtQuick.Controls
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
            Text { text: qsTr("Accounts"); color: theme.text; font.pixelSize: 32 }
            Text { text: qsTr("Who plays. The highlighted account starts every game."); color: theme.muted }
        }

        Repeater {
            model: accounts
            delegate: Surface {
                id: row
                required property string name
                required property string kind
                required property bool active
                required property int index
                Layout.fillWidth: true
                implicitHeight: 72
                level: active ? 2 : 1
                border.width: active ? 1 : 0
                border.color: theme.accent

                TapHandler { onTapped: accounts.activeIndex = row.index }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 12
                    spacing: 14
                    Avatar { size: 44; label: row.name; radius: 22 }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text { text: row.name; color: theme.text; font.pixelSize: 16; font.weight: Font.Medium }
                        Text {
                            text: row.kind === "msa" ? qsTr("Microsoft account") : qsTr("Offline: singleplayer and offline-mode servers")
                            color: theme.muted
                            font.pixelSize: 12
                        }
                    }
                    Tag { visible: row.active; text: qsTr("Playing") }
                    TButton {
                        kind: "text"
                        compact: true
                        glyph: "delete"
                        hint: qsTr("Remove")
                        onClicked: win.confirm(qsTr("Remove %1?").arg(row.name),
                                               row.kind === "msa" ? qsTr("You can sign in again later.") : qsTr("Worlds stay; they belong to the instance."),
                                               qsTr("Remove"), true, () => accounts.remove(row.index))
                    }
                }
            }
        }

        // Microsoft
        Surface {
            Layout.fillWidth: true
            level: 1
            implicitHeight: ms.implicitHeight + 40
            ColumnLayout {
                id: ms
                anchors.fill: parent
                anchors.margins: 20
                spacing: 14
                RowLayout {
                    spacing: 10
                    Icon { name: "account_circle"; color: theme.accent; size: 24 }
                    Text { text: qsTr("Microsoft account"); color: theme.text; font.pixelSize: 16; font.weight: Font.Medium; Layout.fillWidth: true }
                }

                Text {
                    visible: !settings.hasMicrosoft
                    text: qsTr("Signing in needs your own Azure app's client ID, which Mojang has to approve for Minecraft. Add it in Settings; until then, offline accounts work.")
                    color: theme.textSoft
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                TButton { visible: !settings.hasMicrosoft; kind: "tonal"; text: qsTr("Open Settings"); onClicked: win.showPage(3) }

                // The device code, big enough to read across the room.
                ColumnLayout {
                    visible: accounts.loginState === "code"
                    spacing: 12
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Go to %1 and enter this code:").arg(accounts.verificationUrl)
                        color: theme.textSoft
                    }
                    RowLayout {
                        spacing: 12
                        Rectangle {
                            implicitWidth: codeText.implicitWidth + 40
                            implicitHeight: 64
                            radius: Math.min(16, theme.radius)
                            color: theme.accentContainer
                            Text {
                                id: codeText
                                anchors.centerIn: parent
                                text: accounts.userCode
                                color: theme.onAccentContainer
                                font.pixelSize: 30
                                font.weight: Font.DemiBold
                                font.letterSpacing: 4
                                font.family: "monospace"
                            }
                        }
                        TButton { kind: "outlined"; glyph: "content_copy"; text: qsTr("Copy"); onClicked: { accounts.copyCode(); win.toast(qsTr("Code copied"), "ok") } }
                        TButton { kind: "filled"; glyph: "open_in_new"; text: qsTr("Open the page"); onClicked: { accounts.copyCode(); backend.openUrl(accounts.verificationUrl) } }
                    }
                    RowLayout {
                        spacing: 10
                        BusyIndicator { running: true; implicitWidth: 24; implicitHeight: 24 }
                        Text { text: qsTr("Waiting for you to finish signing in…"); color: theme.muted }
                        TButton { kind: "text"; compact: true; text: qsTr("Cancel"); onClicked: accounts.cancelLogin() }
                    }
                }
                RowLayout {
                    visible: accounts.loginState === "working"
                    spacing: 10
                    BusyIndicator { running: parent.visible; implicitWidth: 24; implicitHeight: 24 }
                    Text { text: qsTr("Talking to Microsoft and Mojang…"); color: theme.muted }
                }
                Text {
                    visible: accounts.loginState === "error"
                    text: accounts.loginError
                    color: theme.red
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                TButton {
                    visible: settings.hasMicrosoft && (accounts.loginState === "idle" || accounts.loginState === "error")
                    kind: "filled"
                    glyph: "person_add"
                    text: accounts.loginState === "error" ? qsTr("Try again") : qsTr("Sign in with Microsoft")
                    onClicked: accounts.startMicrosoftLogin()
                }
            }
        }

        // Offline
        Surface {
            Layout.fillWidth: true
            Layout.bottomMargin: 28
            level: 1
            implicitHeight: offline.implicitHeight + 40
            ColumnLayout {
                id: offline
                anchors.fill: parent
                anchors.margins: 20
                spacing: 14
                RowLayout {
                    spacing: 10
                    Icon { name: "key"; color: theme.accent; size: 24 }
                    Text { text: qsTr("Offline account"); color: theme.text; font.pixelSize: 16; font.weight: Font.Medium }
                }
                Text {
                    text: qsTr("A name to play under without signing in: for singleplayer, LAN and offline-mode servers. 3 to 16 letters, digits or underscores.")
                    color: theme.textSoft
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Field {
                        id: offlineName
                        Layout.fillWidth: true
                        placeholder: qsTr("Player name")
                        onAccepted: addButton.clicked()
                    }
                    TButton {
                        id: addButton
                        kind: "tonal"
                        glyph: "add"
                        text: qsTr("Add")
                        enabled: /^[A-Za-z0-9_]{3,16}$/.test(offlineName.text.trim())
                        onClicked: if (enabled && accounts.addOffline(offlineName.text)) offlineName.text = ""
                    }
                }
            }
        }
    }
}
