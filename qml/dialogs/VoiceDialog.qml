import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import LetsChat

// Confirmation shown after a voice note was recorded: listen, then send or discard.
Popup {
    id: root
    property url source
    property int durationMs: 0
    signal sendRequested(url source, int durationMs)
    signal discarded(url source)

    parent: Overlay.overlay
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? Math.max(12, (parent.height - height) / 3) : 0   // upper third: clear of the keyboard
    modal: true
    closePolicy: Popup.NoAutoClose
    padding: 20
    width: Math.min(parent ? parent.width - 32 : 360, 380)

    function fmt(ms) {
        var s = Math.round(ms / 1000)
        return Math.floor(s / 60) + ":" + ("0" + (s % 60)).slice(-2)
    }

    onAboutToShow: Qt.inputMethod.hide()
    onClosed: player.stop()

    background: Rectangle {
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border
    }

    MediaPlayer {
        id: player
        source: root.source
        audioOutput: AudioOutput {}
    }

    ColumnLayout {
        width: parent.width
        spacing: 16

        Label {
            text: qsTr("Send voice message?")
            font.pixelSize: 18
            font.weight: Font.DemiBold
            color: Theme.text
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Rectangle {
                implicitWidth: 48
                implicitHeight: 48
                radius: 24
                color: Theme.accent
                Icon {
                    anchors.centerIn: parent
                    name: player.playbackState === MediaPlayer.PlayingState ? "pause" : "play"
                    size: 26
                    color: Theme.textOnAccent
                }
                TapHandler {
                    onTapped: {
                        if (player.playbackState === MediaPlayer.PlayingState)
                            player.pause()
                        else
                            player.play()
                    }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 4
                    radius: 2
                    color: Theme.border
                    Rectangle {
                        width: player.duration > 0 ? parent.width * player.position / player.duration : 0
                        height: parent.height
                        radius: 2
                        color: Theme.accent
                    }
                }
                Label {
                    text: root.fmt(player.playbackState === MediaPlayer.PlayingState ? player.position : root.durationMs)
                    font.pixelSize: 13
                    color: Theme.textSecondary
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Button {
                text: qsTr("Discard")
                flat: true
                onClicked: { root.discarded(root.source); root.close() }
            }
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Send")
                highlighted: true
                onClicked: { root.sendRequested(root.source, root.durationMs); root.close() }
            }
        }
    }
}
