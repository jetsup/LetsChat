import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import LetsChat

// Preview of a photo about to be sent, with an optional caption.  Buttons and caption are at the
// top so the on-screen keyboard (which opens for the caption) never covers them.
Popup {
    id: root
    property url source
    signal sendRequested(url source, string caption)
    signal discarded(url source)

    parent: Overlay.overlay
    modal: true
    closePolicy: Popup.CloseOnEscape
    padding: 0
    width: Math.min(parent ? parent.width - 24 : 400, 560)
    height: Math.min(parent ? parent.height - 120 : 600, 760)
    x: parent ? (parent.width - width) / 2 : 0
    y: 56

    onAboutToShow: { caption.text = ""; Qt.inputMethod.hide() }

    background: Rectangle {
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Button {
                text: qsTr("Cancel")
                flat: true
                onClicked: { root.discarded(root.source); root.close() }
            }
            Item { Layout.fillWidth: true }
            Button {
                id: send
                text: qsTr("Send")
                highlighted: true
                onClicked: { root.sendRequested(root.source, caption.text); root.close() }
            }
        }

        Basic.TextField {
            id: caption
            Layout.fillWidth: true
            placeholderText: qsTr("Add a caption (optional)")
            placeholderTextColor: Theme.textSecondary
            color: Theme.text
            font.pixelSize: 15
            leftPadding: 16
            rightPadding: 16
            topPadding: 12
            bottomPadding: 12
            background: Rectangle {
                radius: height / 2
                color: Theme.surfaceAlt
                border.width: caption.activeFocus ? 1 : 0
                border.color: Theme.accent
            }
            onAccepted: send.clicked()
        }

        Image {
            Layout.fillWidth: true
            Layout.fillHeight: true
            source: root.source
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            autoTransform: true
            sourceSize.width: 1200
        }
    }
}
