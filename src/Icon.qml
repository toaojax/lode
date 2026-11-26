import QtQuick

// A Material Symbols glyph by name, the icon set Tonal installs. Without the
// font, a plain symbol stands in so nothing reads as a stray word.
Text {
    id: icon
    property string name: ""
    property real size: 20
    property bool filled: false

    readonly property var fallback: ({
        "sports_esports": "◆", "explore": "◎", "account_circle": "●", "settings": "⚙", "play_arrow": "▶",
        "stop": "■", "add": "+", "close": "✕", "delete": "✕", "folder_open": "▤", "download": "↓",
        "check": "✓", "check_circle": "✓", "refresh": "↻", "more_vert": "⋮", "terminal": "›", "extension": "◇",
        "texture": "▦", "wb_sunny": "☀", "inventory_2": "▣", "upgrade": "↑", "content_copy": "⧉",
        "open_in_new": "↗", "arrow_back": "←", "help": "?", "error": "!", "info": "i", "search": "⌕",
        "content_copy_alt": "⧉", "edit": "✎", "tune": "≡", "schedule": "◷", "person_add": "+",
        "key": "⚿", "memory": "▥", "hourglass_top": "◷", "cloud_download": "↓", "warning": "!",
        "favorite": "♥", "file_download": "↓", "filter_list": "≡", "sort": "≡", "deployed_code": "▣",
        "library_add": "+", "link": "⛓", "visibility": "◉", "visibility_off": "◌", "upload_file": "↑"
    })

    text: win.hasSymbols ? name : (fallback[name] || "•")
    font.family: win.hasSymbols ? "Material Symbols Rounded" : win.font.family
    font.pixelSize: Math.round(win.hasSymbols ? size : size * 0.8)
    font.variableAxes: win.hasSymbols ? { "FILL": filled ? 1 : 0, "wght": filled ? 500 : 400, "opsz": Math.min(48, Math.max(20, size)) } : {}
    color: theme.text
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    renderType: Text.QtRendering
}
