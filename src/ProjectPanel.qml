import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// A project's page: what it is, its files, and where to install them.
Rectangle {
    id: panel
    property var project: null
    property string target: ""
    property var targetInfo: ({})
    property bool installed: false
    property var details: null
    property var versions: []
    property string versionsError: ""
    property bool loadingVersions: false
    property int tab: 0
    signal close()

    readonly property bool packs: project && project.projectType === "modpack"
    color: theme.surfaceLowest

    function loadVersions() {
        if (!project) return
        loadingVersions = true
        versions = []
        versionsError = ""
        const forTarget = !packs && target !== ""
        catalog.fetchVersions(project.provider, project.projectId, forTarget ? targetInfo.minecraft : "",
                              forTarget ? targetInfo.loader : "", project.projectType)
    }
    function installVersion(version) {
        if (packs) return modpacks.installVersion(version, project)
        if (target === "") return win.toast(qsTr("Pick an instance to install into first."), "error")
        content.install(target, version, project)
    }

    onProjectChanged: { details = null; tab = 0; loadVersions() }
    onTargetChanged: loadVersions()

    Connections {
        target: catalog
        function onProjectReady(id, project) { if (panel.project && id === panel.project.projectId) panel.details = project }
        function onVersionsReady(id, versions, error) {
            if (!panel.project || id !== panel.project.projectId) return
            panel.loadingVersions = false
            panel.versions = versions
            panel.versionsError = error
        }
    }

    // The sheet sits over the results; clicks and scrolls stop here.
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        onWheel: wheel => wheel.accepted = true
    }

    // A divider on the side facing the results.
    Rectangle { anchors.left: parent.left; width: 1; height: parent.height; color: theme.outlineVariant }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        anchors.bottomMargin: 0
        spacing: 16
        visible: panel.project !== null

        RowLayout {
            Layout.fillWidth: true
            spacing: 14
            Avatar { size: 72; source: panel.project ? panel.project.icon : ""; label: panel.project ? panel.project.title : "" }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text {
                    text: panel.project ? panel.project.title : ""
                    color: theme.text
                    font.pixelSize: 22
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                RowLayout {
                    spacing: 8
                    Text {
                        text: panel.project && panel.project.author ? qsTr("by %1").arg(panel.project.author) : ""
                        color: theme.muted
                        font.pixelSize: 13
                    }
                    Tag {
                        visible: panel.installed
                        text: panel.target !== "" ? qsTr("Installed in %1").arg(panel.targetInfo.name) : qsTr("Installed")
                    }
                }
            }
            TButton { kind: "text"; glyph: "close"; hint: qsTr("Close (Esc)"); onClicked: panel.close() }
        }

        Text {
            text: panel.project ? panel.project.summary : ""
            color: theme.textSoft
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        Flow {
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: {
                    if (!panel.details) return []
                    const d = panel.details, links = []
                    if (d.website) links.push({ label: panel.project.provider === "curseforge" ? "CurseForge" : "Modrinth", url: d.website })
                    if (d.source) links.push({ label: qsTr("Source"), url: d.source })
                    if (d.issues) links.push({ label: qsTr("Issues"), url: d.issues })
                    if (d.wiki) links.push({ label: qsTr("Wiki"), url: d.wiki })
                    if (d.discord) links.push({ label: "Discord", url: d.discord })
                    return links
                }
                delegate: TButton {
                    required property var modelData
                    kind: "outlined"
                    compact: true
                    glyph: "open_in_new"
                    text: modelData.label
                    onClicked: backend.openUrl(modelData.url)
                }
            }
            Tag { visible: !!(panel.details && panel.details.license); text: panel.details ? panel.details.license || "" : ""; tint: theme.surfaceHigh; ink: theme.textSoft; height: 32 }
        }

        Segmented {
            options: [{ value: "versions", label: qsTr("Versions") }, { value: "description", label: qsTr("Description") }]
            value: panel.tab === 0 ? "versions" : "description"
            onPicked: value => panel.tab = value === "versions" ? 0 : 1
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: panel.tab

            // Versions
            ColumnLayout {
                spacing: 10
                Text {
                    text: panel.packs ? qsTr("Installing a modpack makes a new instance.")
                        : panel.target === "" ? qsTr("Pick an instance in the filters to see versions that fit it.")
                        : qsTr("Versions for %1 (%2 %3)").arg(panel.targetInfo.name).arg(backend.loaderName(panel.targetInfo.loader)).arg(panel.targetInfo.minecraft)
                    color: theme.muted
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                Text {
                    visible: panel.versionsError !== "" || (!panel.loadingVersions && panel.versions.length === 0)
                    text: panel.versionsError !== "" ? panel.versionsError : qsTr("No versions fit this instance.")
                    color: panel.versionsError !== "" ? theme.red : theme.textSoft
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                BusyIndicator { visible: panel.loadingVersions; running: visible; Layout.alignment: Qt.AlignHCenter }
                ListView {
                    id: versionList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: panel.versions
                    spacing: 4
                    reuseItems: true
                    bottomMargin: 24
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Surface {
                        id: vrow
                        required property var modelData
                        required property int index
                        width: versionList.width
                        height: 60
                        level: vhover.hovered ? 2 : 1
                        radius: Math.min(14, theme.radius)
                        HoverHandler { id: vhover }
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 8
                            spacing: 10
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                RowLayout {
                                    spacing: 8
                                    Text {
                                        text: vrow.modelData.number
                                        color: theme.text
                                        font.pixelSize: 14
                                        font.weight: Font.Medium
                                        elide: Text.ElideRight
                                        Layout.maximumWidth: versionList.width - 220
                                    }
                                    Tag {
                                        visible: vrow.modelData.channel !== "release"
                                        text: vrow.modelData.channel === "beta" ? qsTr("Beta") : qsTr("Alpha")
                                        tint: Qt.rgba(theme.yellow.r, theme.yellow.g, theme.yellow.b, 0.18)
                                        ink: theme.yellow
                                    }
                                }
                                Text {
                                    text: {
                                        const games = (vrow.modelData.gameVersions || [])
                                        const shown = games.length > 3 ? games.slice(0, 3).join(", ") + "…" : games.join(", ")
                                        const date = new Date(vrow.modelData.date)
                                        return [shown, (vrow.modelData.loaders || []).map(l => backend.loaderName(l)).join(", "),
                                                isNaN(date.getTime()) ? "" : date.toLocaleDateString(Qt.locale(), Locale.ShortFormat)]
                                            .filter(s => s !== "").join("  ·  ")
                                    }
                                    color: theme.muted
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                            TButton {
                                kind: vrow.index === 0 ? "filled" : "tonal"
                                compact: true
                                glyph: vrow.modelData.downloadable ? "download" : "open_in_new"
                                text: !vrow.modelData.downloadable ? qsTr("Website") : panel.packs ? qsTr("Create") : qsTr("Install")
                                enabled: panel.packs ? !modpacks.busy : (panel.target !== "" && !content.busy)
                                onClicked: vrow.modelData.downloadable ? panel.installVersion(vrow.modelData)
                                                                       : backend.openUrl(panel.details ? panel.details.website + "/files/" + vrow.modelData.id : "")
                            }
                        }
                    }
                }
            }

            // Description
            ScrollView {
                id: descriptionScroll
                clip: true
                contentWidth: availableWidth
                Text {
                    width: descriptionScroll.availableWidth - 8
                    text: panel.details ? panel.details.body : qsTr("Loading…")
                    textFormat: panel.details && panel.details.format === "html" ? Text.RichText : Text.MarkdownText
                    wrapMode: Text.Wrap
                    color: theme.textSoft
                    linkColor: theme.accent
                    font.pixelSize: 14
                    lineHeight: 1.2
                    bottomPadding: 24
                    onLinkActivated: link => backend.openUrl(link)
                    HoverHandler { cursorShape: parent.hoveredLink !== "" ? Qt.PointingHandCursor : Qt.ArrowCursor }
                }
            }
        }
    }
}
