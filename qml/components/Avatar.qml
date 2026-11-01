import QtQuick
import QtQuick.Controls
import LetsChat

Item {
    id: root
    property string name
    property string seed: name
    property bool showPresence: false
    property bool online: false
    property real size: 44

    implicitWidth: size
    implicitHeight: size

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: Theme.avatarColor(root.seed)

        Label {
            anchors.centerIn: parent
            text: Theme.initials(root.name)
            color: "white"
            font.bold: true
            font.pixelSize: root.size * 0.38
        }
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
