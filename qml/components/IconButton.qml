import QtQuick
import QtQuick.Controls
import LetsChat

AbstractButton {
    id: root
    property string iconName
    property color iconColor: Theme.text
    property real iconSize: 22

    implicitWidth: 44
    implicitHeight: 44
    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    opacity: enabled ? 1 : 0.4

    background: Rectangle {
        radius: width / 2
        color: root.down ? Theme.pressed : (root.hovered ? Theme.hover : "transparent")
    }
    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            name: root.iconName
            color: root.iconColor
            size: root.iconSize
        }
    }
}
