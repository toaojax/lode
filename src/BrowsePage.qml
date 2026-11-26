import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Search Modrinth and CurseForge. Picking an instance to install into narrows
// results to its game version and loader; modpacks always make a new one.
Item {
    id: page
    property string target: ""
    property var detail: null
    readonly property bool detailOpen: detail !== null
    readonly property bool packs: catalog.projectType === "modpack"
    readonly property var targetInfo: { instances.count; return target !== "" ? instances.get(target) : ({}) }
    property var installedHere: []
    // Results waiting on their version list before a one-click install.
    property var pendingInstall: ({})

    function focusSearch() { search.input.forceActiveFocus(); search.input.selectAll() }
    function closeDetail() { detail = null; results.forceActiveFocus() }
    function openDetail(project) {
        detail = project
        catalog.fetchProject(project.provider, project.projectId)
    }
    function refreshInstalled() { installedHere = target !== "" ? content.projectsIn(target) : [] }
    function applyTarget() {
        if (packs) {
            catalog.loader = ""
        } else if (target !== "" && targetInfo.minecraft) {
            catalog.gameVersion = targetInfo.minecraft
            catalog.loader = catalog.projectType === "mod" ? targetInfo.loader : ""
        } else {
            catalog.gameVersion = ""
            catalog.loader = ""
        }
        refreshInstalled()
    }
    function quickInstall(project) {
        if (packs) return openDetail(project)
        if (target === "") {
            win.toast(qsTr("Pick an instance to install into first."), "error")
            return targetBox.popup.open()
        }
        const pending = pendingInstall
        pending[project.projectId] = project
        pendingInstall = pending
        catalog.fetchVersions(project.provider, project.projectId, targetInfo.minecraft, targetInfo.loader, catalog.projectType)
    }

    readonly property string type: catalog.projectType
    onTypeChanged: applyTarget()
    onTargetChanged: applyTarget()
    onPacksChanged: applyTarget()
    Component.onCompleted: { if (target === "" && instances.count > 0) target = instances.idAt(0); catalog.refresh() }
    onVisibleChanged: if (visible) refreshInstalled()

    Connections {
        target: catalog
        function onVersionsReady(id, versions, error) {
            const project = page.pendingInstall[id]
            if (!project) return
            const pending = page.pendingInstall
            delete pending[id]
            page.pendingInstall = pending
            if (error) return win.toast(error, "error")
            const release = versions.find(v => v.channel === "release") || versions[0]
            if (!release) return win.toast(qsTr("%1 has no version for %2.").arg(project.title).arg(page.targetInfo.name), "error")
            content.install(page.target, release, project)
        }
    }
    Connections {
        target: content
        function onInstalledChanged() { page.refreshInstalled() }
    }
    Connections {
        target: instances
        function onCountChanged() { if (page.target !== "" && instances.indexOf(page.target) < 0) page.target = "" }
    }

    Item {
        anchors.fill: parent

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 28
            anchors.bottomMargin: 0
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                spacing: 16
                Text { text: qsTr("Browse"); color: theme.text; font.pixelSize: 32 }
                Item { Layout.fillWidth: true }
                Segmented {
                    options: [{ value: "modrinth", label: "Modrinth" }, { value: "curseforge", label: "CurseForge" }]
                    value: catalog.provider
                    onPicked: value => { page.detail = null; catalog.provider = value }
                }
            }

            Field {
                id: search
                Layout.fillWidth: true
                icon: "search"
                placeholder: page.packs ? qsTr("Search modpacks") : catalog.projectType === "mod" ? qsTr("Search mods")
                           : catalog.projectType === "shader" ? qsTr("Search shaders") : qsTr("Search resource packs")
                text: catalog.query
                onEdited: catalog.query = text
                onAccepted: { catalog.query = text; results.forceActiveFocus(); results.currentIndex = 0 }
                input.Keys.onDownPressed: { results.forceActiveFocus(); results.currentIndex = 0 }
            }

            // Filters
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Repeater {
                    model: [
                        { value: "mod", label: qsTr("Mods"), icon: "extension" },
                        { value: "modpack", label: qsTr("Modpacks"), icon: "inventory_2" },
                        { value: "resourcepack", label: qsTr("Resource packs"), icon: "texture" },
                        { value: "shader", label: qsTr("Shaders"), icon: "wb_sunny" }
                    ]
                    delegate: Chip {
                        required property var modelData
                        text: modelData.label
                        glyph: modelData.icon
                        checked: catalog.projectType === modelData.value
                        onClicked: { checked = Qt.binding(() => catalog.projectType === modelData.value); page.detail = null; catalog.projectType = modelData.value }
                    }
                }
                Item { Layout.fillWidth: true }
                ComboBox {
                    id: targetBox
                    visible: !page.packs
                    Layout.preferredWidth: 240
                    textRole: "text"
                    valueRole: "value"
                    model: {
                        instances.count
                        const list = [{ text: qsTr("Any version"), value: "" }]
                        for (let i = 0; i < instances.count; ++i) {
                            const info = instances.get(instances.idAt(i))
                            list.push({ text: qsTr("For %1").arg(info.name), value: info.instanceId })
                        }
                        return list
                    }
                    currentIndex: Math.max(0, indexOfValue(page.target))
                    onActivated: page.target = currentValue
                }
                ComboBox {
                    Layout.preferredWidth: 170
                    textRole: "text"
                    valueRole: "value"
                    model: [
                        { text: qsTr("Relevance"), value: "relevance" },
                        { text: qsTr("Downloads"), value: "downloads" },
                        { text: qsTr("Follows"), value: "follows" },
                        { text: qsTr("Recently updated"), value: "updated" },
                        { text: qsTr("Newest"), value: "newest" }
                    ]
                    currentIndex: Math.max(0, indexOfValue(catalog.sort))
                    onActivated: catalog.sort = currentValue
                }
            }

            Text {
                visible: !page.packs && page.target !== "" && !!page.targetInfo.minecraft
                text: catalog.projectType === "mod"
                      ? qsTr("Showing %1 mods for Minecraft %2").arg(backend.loaderName(page.targetInfo.loader || "")).arg(page.targetInfo.minecraft)
                      : qsTr("Showing packs for Minecraft %1").arg(page.targetInfo.minecraft)
                color: theme.muted
                font.pixelSize: 12
            }

            // Errors and empty results say what to do.
            Surface {
                visible: catalog.error !== "" || (!catalog.loading && results.count === 0)
                Layout.fillWidth: true
                Layout.preferredHeight: 140
                level: 1
                ColumnLayout {
                    anchors.centerIn: parent
                    width: parent.width - 48
                    spacing: 12
                    Text {
                        text: catalog.error !== "" ? catalog.error : qsTr("Nothing matches. Try other words or another filter.")
                        color: catalog.error !== "" ? theme.red : theme.textSoft
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                        Layout.fillWidth: true
                    }
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        TButton { visible: catalog.error !== "" && catalog.provider === "curseforge" && !settings.hasCurseForge; kind: "tonal"; compact: true; text: qsTr("Open Settings"); onClicked: win.showPage(3) }
                        TButton { visible: catalog.error !== ""; kind: "text"; compact: true; glyph: "refresh"; text: qsTr("Try again"); onClicked: catalog.refresh() }
                    }
                }
            }

            ListView {
                id: results
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: count > 0
                clip: true
                model: catalog
                spacing: 6
                reuseItems: true
                cacheBuffer: 600
                bottomMargin: 24
                boundsBehavior: Flickable.StopAtBounds
                keyNavigationEnabled: true
                highlightMoveDuration: 0
                currentIndex: -1
                ScrollBar.vertical: ScrollBar {}
                // The next page loads before the end is reached.
                onContentYChanged: if (contentHeight > 0 && contentY + height > contentHeight - 900) catalog.loadMore()
                Keys.onReturnPressed: if (currentIndex >= 0) page.openDetail(catalog.at(currentIndex))
                Keys.onUpPressed: event => { if (currentIndex <= 0) search.input.forceActiveFocus(); else decrementCurrentIndex() }

                footer: Item {
                    width: results.width
                    height: catalog.loading && results.count > 0 ? 56 : 0
                    BusyIndicator { anchors.centerIn: parent; running: parent.height > 0; implicitWidth: 32; implicitHeight: 32 }
                }

                delegate: ResultCard {
                    width: results.width
                    installed: page.installedHere.indexOf(projectId) >= 0
                    selected: (page.detail && page.detail.projectId === projectId) || (results.activeFocus && ListView.isCurrentItem)
                    pending: !!page.pendingInstall[projectId]
                    installLabel: page.packs ? qsTr("View") : qsTr("Install")
                    onOpen: page.openDetail(catalog.at(index))
                    onInstall: page.quickInstall(catalog.at(index))
                }
            }

            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                visible: catalog.loading && results.count === 0
                running: visible
            }
        }

        // A side sheet over the results, so the filters keep their room.
        ProjectPanel {
            id: panel
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            width: Math.min(540, page.width * 0.5)
            visible: page.detailOpen
            project: page.detail
            target: page.target
            targetInfo: page.targetInfo
            installed: page.detail ? page.installedHere.indexOf(page.detail.projectId) >= 0 : false
            onClose: page.closeDetail()
        }
    }
}
