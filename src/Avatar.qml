import QtQuick

// A project or instance picture, or its initial on a tonal tile when there's
// none. Images decode off the UI thread at the size they're shown, which is
// most of what keeps long result lists scrolling smoothly.
Rectangle {
    id: avatar
    property string source: ""
    property string label: ""
    property real size: 48
    width: size
    height: size
    radius: Math.min(size * 0.28, theme.radius)
    color: theme.accentContainer
    clip: true

    Text {
        anchors.centerIn: parent
        visible: image.status !== Image.Ready
        text: avatar.label.trim().length > 0 ? avatar.label.trim()[0].toUpperCase() : "?"
        color: theme.onAccentContainer
        font.pixelSize: Math.round(avatar.size * 0.42)
        font.weight: Font.DemiBold
    }
    Image {
        id: image
        anchors.fill: parent
        source: avatar.source
        asynchronous: true
        cache: true
        smooth: true
        mipmap: false
        fillMode: Image.PreserveAspectCrop
        sourceSize.width: Math.ceil(avatar.size * Screen.devicePixelRatio)
        sourceSize.height: Math.ceil(avatar.size * Screen.devicePixelRatio)
        opacity: status === Image.Ready ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 120 } }
    }
}
