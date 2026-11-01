import QtQuick
import QtQuick.Controls
import LetsChat

// Transient message at the bottom of the window.  Call show("text").
Rectangle {
    id: root
    property alias text: label.text
    function show(message) {
        label.text = message
        opacity = 1
        hideTimer.restart()
    }

    z: 100
    opacity: 0
    visible: opacity > 0
    anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
    anchors.bottom: parent ? parent.bottom : undefined
    anchors.bottomMargin: 88
    width: Math.min(label.implicitWidth + 40, (parent ? parent.width : 400) - 32)
    height: label.implicitHeight + 24
    radius: height / 2
    color: Theme.dark ? "#e7eaf1" : "#202633"

    Behavior on opacity { NumberAnimation { duration: 180 } }
    Timer { id: hideTimer; interval: 3500; onTriggered: root.opacity = 0 }

    Label {
        id: label
        anchors.centerIn: parent
        width: parent.width - 32
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        color: Theme.dark ? "#151922" : "white"
        font.pixelSize: 14
    }
}
