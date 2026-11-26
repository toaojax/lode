import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1180
    height: 760
    minimumWidth: 880
    minimumHeight: 560
    visible: true
    title: "Lode"
    color: theme.background

    Material.theme: theme.dark ? Material.Dark : Material.Light
    Material.accent: theme.accent
    Material.primary: theme.accent
    Material.background: theme.surfaceHigh
    Material.foreground: theme.text
    Material.roundedScale: theme.radius

    // Tonal installs Google Sans Flex and Material Symbols; use them when
    // they're there, and the desktop's own font otherwise.
    readonly property bool hasSymbols: Qt.fontFamilies().indexOf("Material Symbols Rounded") >= 0
    font.family: Qt.fontFamilies().indexOf("Google Sans Flex") >= 0 ? "Google Sans Flex" : Qt.application.font.family
    font.pixelSize: 14

    property int page: 0
    property string selectedId: ""
    property bool helpVisible: false
    readonly property bool working: launcher.preparingId !== "" || modpacks.busy || content.busy
    readonly property var pageNames: [qsTr("Library"), qsTr("Browse"), qsTr("Accounts"), qsTr("Settings")]

    function showPage(index) {
        page = index
        helpVisible = false
    }
    function openInstance(id) {
        selectedId = id
        page = 0
    }
    function play(id) {
        if (!id) id = selectedId !== "" ? selectedId : instances.idAt(0)
        if (!id) return
        if (accounts.count === 0) {
            toast(qsTr("Add an account first: a Microsoft one, or an offline name."), "error")
            return showPage(2)
        }
        launcher.launch(id)
    }
    function toast(message, kind) { toasts.show(message, kind) }
    function confirm(title, message, acceptText, danger, action) { confirmDialog.ask(title, message, acceptText, danger, action) }
    function quit() {
        if (working)
            confirm(qsTr("Quit while working?"), qsTr("Downloads in progress will stop. Running games keep going."),
                    qsTr("Quit"), true, Qt.quit)
        else
            Qt.quit()
    }
    function formatCount(n) {
        if (n >= 1e9) return qsTr("%1B").arg((n / 1e9).toLocaleString(Qt.locale(), 'f', 1))
        if (n >= 1e6) return qsTr("%1M").arg((n / 1e6).toLocaleString(Qt.locale(), 'f', 1))
        if (n >= 1e3) return qsTr("%1K").arg((n / 1e3).toLocaleString(Qt.locale(), 'f', 1))
        return Number(n).toLocaleString(Qt.locale(), 'f', 0)
    }
    function formatPlaytime(seconds, lastPlayed) {
        const h = Math.floor(seconds / 3600), m = Math.floor(seconds % 3600 / 60)
        if (h > 0) return qsTr("Played %1h %2m").arg(h).arg(m)
        if (m > 0) return qsTr("Played %1m").arg(m)
        const played = lastPlayed && !isNaN(lastPlayed.getTime())
        return played ? qsTr("Played under a minute") : qsTr("Never played")
    }
    function relativeDate(date) {
        translator.language
        if (!date || isNaN(date.getTime())) return qsTr("Never")
        const days = Math.floor((Date.now() - date.getTime()) / 86400000)
        if (days < 1) return qsTr("Today")
        if (days < 2) return qsTr("Yesterday")
        if (days < 30) return qsTr("%n day(s) ago", "", days)
        return date.toLocaleDateString(Qt.locale(), Locale.ShortFormat)
    }

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.quit() }
    Shortcut { sequence: "Ctrl+Q"; context: Qt.ApplicationShortcut; onActivated: win.quit() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.play("") }
    Shortcut { sequence: "Ctrl+N"; context: Qt.ApplicationShortcut; onActivated: newInstance.open() }
    Shortcut { sequence: "Ctrl+F"; context: Qt.ApplicationShortcut; onActivated: { win.showPage(1); browsePage.focusSearch() } }
    Shortcut { sequence: "/"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: { win.showPage(1); browsePage.focusSearch() } }
    Shortcut { sequence: "1"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.showPage(0) }
    Shortcut { sequence: "2"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.showPage(1) }
    Shortcut { sequence: "3"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.showPage(2) }
    Shortcut { sequence: "4"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.showPage(3) }
    Shortcut {
        sequence: "Esc"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (win.helpVisible) win.helpVisible = false
            else if (win.page === 1 && browsePage.detailOpen) browsePage.closeDetail()
            else if (win.page === 0 && win.selectedId !== "") win.selectedId = ""
            contentRoot.forceActiveFocus()
        }
    }
    // Single-key shortcuts stand down while a text field has focus.
    readonly property bool typing: activeFocusItem !== null && activeFocusItem instanceof TextInput

    Connections {
        target: backend
        function onNotice(message, kind) { win.toast(message, kind) }
    }
    Connections {
        target: content
        function onInstallFailed(title, error, blocked) { if (blocked.length > 0) blockedDialog.show(title, error, blocked) }
    }
    Connections {
        target: modpacks
        function onFailed(name, error, blocked) { if (blocked.length > 0) blockedDialog.show(name, error, blocked) }
        function onImported(id, name) { win.openInstance(id) }
    }
    Connections {
        target: launcher
        function onStarted(id) { if (settings.closeOnLaunch) win.showMinimized() }
    }

    Item {
        id: contentRoot
        anchors.fill: parent
        focus: true
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        RowLayout {
            anchors.fill: parent
            spacing: 0

            NavRail {
                Layout.fillHeight: true
                current: win.page
                onSelected: index => win.showPage(index)
                onCreate: newInstance.open()
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: win.page

                    LibraryPage { id: libraryPage }
                    BrowsePage { id: browsePage }
                    AccountsPage {}
                    SettingsPage {}
                }

                BusyStrip { Layout.fillWidth: true }
            }
        }

        // Packs dropped anywhere on the window become instances.
        DropArea {
            id: drop
            anchors.fill: parent
            keys: ["text/uri-list"]
            onDropped: event => {
                for (const url of event.urls) {
                    const s = url.toString()
                    if (s.endsWith(".mrpack") || s.endsWith(".zip")) { modpacks.importFile(s); event.accept(); return }
                }
                win.toast(qsTr("Drop a .mrpack or CurseForge .zip to import it."), "error")
            }
            Rectangle {
                anchors.fill: parent
                anchors.margins: 12
                visible: drop.containsDrag
                radius: theme.radius
                color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.12)
                border.width: 2
                border.color: theme.accent
                Text {
                    anchors.centerIn: parent
                    text: qsTr("Drop to import the modpack")
                    color: theme.accent
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                }
            }
        }

        Toasts { id: toasts }
        HelpOverlay { visible: win.helpVisible; onClose: win.helpVisible = false }
    }

    NewInstanceDialog { id: newInstance }
    ConfirmDialog { id: confirmDialog }
    BlockedDialog { id: blockedDialog }

    Component.onCompleted: backend.loadGameVersions()
}
