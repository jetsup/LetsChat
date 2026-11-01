import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import LetsChat

// Round avatar: the profile picture when there is one, otherwise coloured initials.
Item {
    id: root
    property string name
    property string seed: name
    property url source                       // profile picture (empty -> initials)
    property bool showPresence: false
    property bool online: false
    property real size: 44

    implicitWidth: size
    implicitHeight: size

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: Theme.avatarColor(root.seed)
        visible: picture.status !== Image.Ready

        Label {
            anchors.centerIn: parent
            text: Theme.initials(root.name)
            color: "white"
            font.bold: true
            font.pixelSize: root.size * 0.38
        }
    }

    Image {
        id: picture
        anchors.fill: parent
        source: root.source
        sourceSize: Qt.size(root.size * 3, root.size * 3)
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        autoTransform: true
        visible: status === Image.Ready
        layer.enabled: true
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: roundMask
        }
    }
    Item {
        id: roundMask
        anchors.fill: parent
        visible: false
        layer.enabled: true
        Rectangle { anchors.fill: parent; radius: width / 2 }
    }

    Rectangle {
        visible: root.showPresence
        width: root.size * 0.3
        height: width
        radius: width / 2
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: root.online ? Theme.success : Theme.textSecondary
        border.width: 2
        border.color: Theme.surface
    }
}
