import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// A Material 3 dialog in the tonal style: the highest surface, the window's
// rounding, a dimmed scrim behind.
Popup {
    id: sheet
    property string title: ""
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    padding: 24
    width: Math.min(520, parent ? parent.width - 64 : 520)
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    Material.roundedScale: theme.radius

    background: Surface { level: 3 }
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.5) }

    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120 } }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 90 } }
}
