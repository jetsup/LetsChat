pragma Singleton
import QtQuick
import LetsChat

QtObject {
    id: theme

    readonly property bool dark: ThemeManager.isDarkMode

    // surfaces
    readonly property color background: dark ? "#0d1015" : "#f2f4f8"
    readonly property color surface: dark ? "#151921" : "#ffffff"
    readonly property color surfaceAlt: dark ? "#1c212b" : "#eceff5"
    readonly property color border: dark ? "#262c38" : "#e1e5ec"
    readonly property color hover: dark ? "#1b212b" : "#eef1f7"
    readonly property color pressed: dark ? "#232a36" : "#e4e8f1"

    // text
    readonly property color text: dark ? "#e7eaf1" : "#151922"
    readonly property color textSecondary: dark ? "#8b93a6" : "#6b7385"
    readonly property color textOnAccent: "#ffffff"

    // brand + semantic
    readonly property color accent: dark ? "#6b84ff" : "#4361ee"
    readonly property color success: "#22c55e"
    readonly property color warning: "#f59e0b"
    readonly property color error: "#ef4444"
    readonly property color wifi: "#14b8a6"
    readonly property color bluetooth: "#2d7ff9"

    // chat bubbles
    readonly property color bubbleOwn: dark ? "#4a63e8" : "#4361ee"
    readonly property color bubbleOwnText: "#ffffff"
    readonly property color bubbleOther: dark ? "#1f2530" : "#ffffff"
    readonly property color bubbleOtherText: text

    // metrics
    readonly property int radius: 14
    readonly property int spacing: 12
    readonly property int pageMargin: 16
    readonly property int headerHeight: 64

    readonly property var avatarColors: ["#4361ee", "#7209b7", "#e11d74", "#f59e0b",
                                         "#0d9488", "#dc2626", "#0284c7", "#65a30d"]

    function avatarColor(seed) {
        var h = 0
        var s = seed || "?"
        for (var i = 0; i < s.length; ++i)
            h = (h * 31 + s.charCodeAt(i)) & 0x7fffffff
        return avatarColors[h % avatarColors.length]
    }

    function initials(name) {
        var parts = (name || "?").trim().split(/\s+/)
        var r = parts[0].charAt(0)
        if (parts.length > 1)
            r += parts[parts.length - 1].charAt(0)
        return r.toUpperCase()
    }

    function shortTime(iso) {
        if (!iso)
            return ""
        var d = new Date(iso)
        if (isNaN(d.getTime()))
            return ""
        var now = new Date()
        if (d.toDateString() === now.toDateString())
            return Qt.formatTime(d, "hh:mm")
        var diff = (now - d) / 86400000
        if (diff < 7)
            return Qt.formatDate(d, "ddd")
        return Qt.formatDate(d, "d MMM")
    }
}
