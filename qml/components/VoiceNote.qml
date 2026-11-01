import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LetsChat

// Play button + progress bar + length of a voice note.
RowLayout {
    id: root
    property bool own: false
    property bool playing: false
    property real progress: 0
    property string durationText: "0:00"
    signal toggled()

    spacing: 10
    implicitWidth: 220

    readonly property color fg: own ? Theme.bubbleOwnText : Theme.bubbleOtherText
    readonly property color dim: own ? Qt.rgba(1, 1, 1, 0.35) : Theme.border

    Rectangle {
        implicitWidth: 40
        implicitHeight: 40
        radius: 20
        color: root.own ? Qt.rgba(1, 1, 1, 0.22) : Theme.accent
        Icon {
            anchors.centerIn: parent
            name: root.playing ? "pause" : "play"
            size: 22
            color: root.own ? Theme.bubbleOwnText : Theme.textOnAccent
        }
        TapHandler { onTapped: root.toggled() }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 4
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 4
            radius: 2
            color: root.dim
            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, root.progress))
                height: parent.height
                radius: 2
                color: root.fg
            }
        }
        Label {
            text: root.durationText
            font.pixelSize: 12
            color: root.own ? Qt.rgba(1, 1, 1, 0.75) : Theme.textSecondary
        }
    }
}
