import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LetsChat

// Small pill showing how a peer is reachable: "lan" (Wi-Fi) or "bluetooth".
Rectangle {
    id: root
    property string transport
    readonly property bool isBt: transport === "bluetooth"
    readonly property color tint: isBt ? Theme.bluetooth : Theme.wifi

    visible: transport === "lan" || transport === "bluetooth"
    implicitHeight: 20
    implicitWidth: row.implicitWidth + 12
    radius: height / 2
    color: Qt.rgba(tint.r, tint.g, tint.b, Theme.dark ? 0.2 : 0.14)

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 3
        Icon {
            name: root.isBt ? "bluetooth" : "wifi"
            size: 12
            color: root.tint
        }
        Label {
            text: root.isBt ? qsTr("Bluetooth") : qsTr("Wi-Fi")
            font.pixelSize: 11
            font.weight: Font.Medium
            color: root.tint
        }
    }
}
