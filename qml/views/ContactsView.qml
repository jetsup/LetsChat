import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import LetsChat

Rectangle {
    id: root
    color: Theme.surface

    property string selectedId: ""
    property real topInset: 0
    signal openSettings()
    signal openBluetooth()
    signal peerChosen(string userId)

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---- header
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.headerHeight + root.topInset

            RowLayout {
                anchors.fill: parent
                anchors.topMargin: root.topInset
                anchors.leftMargin: Theme.pageMargin
                anchors.rightMargin: 6
                spacing: 4

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Label {
                        text: qsTr("LetsChat")
                        font.pixelSize: 22
                        font.bold: true
                        color: Theme.text
                    }
                    Label {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        font.pixelSize: 12
                        color: Theme.textSecondary
                        text: {
                            var online = qsTr("%n online", "", Peers.onlineCount)
                            return Chat.lanConnected ? online + " · " + Chat.localIp : online + " · " + qsTr("no Wi-Fi")
                        }
                    }
                }

                IconButton {
                    iconName: "bluetooth"
                    iconColor: Bluetooth.enabled && Bluetooth.listening ? Theme.bluetooth : Theme.textSecondary
                    onClicked: root.openBluetooth()
                }
                IconButton {
                    iconName: "settings"
                    iconColor: Theme.textSecondary
                    onClicked: root.openSettings()
                }
            }
        }

        // ---- search
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            visible: Peers.peerCount > 3

            Basic.TextField {
                id: searchField
                anchors.fill: parent
                anchors.leftMargin: Theme.pageMargin
                anchors.rightMargin: Theme.pageMargin
                anchors.topMargin: 4
                anchors.bottomMargin: 8
                placeholderText: qsTr("Search people")
                placeholderTextColor: Theme.textSecondary
                color: Theme.text
                font.pixelSize: 14
                leftPadding: 16
                rightPadding: 16
                selectByMouse: true
                background: Rectangle {
                    radius: height / 2
                    color: Theme.surfaceAlt
                    border.width: searchField.activeFocus ? 1 : 0
                    border.color: Theme.accent
                }
                text: Peers.filterText
                onTextChanged: Peers.filterText = text
            }
        }

        // ---- warning when Wi-Fi is missing
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: banner.implicitHeight + 16
            visible: !Chat.lanConnected
            color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.15)
            RowLayout {
                id: banner
                anchors.fill: parent
                anchors.margins: 8
                anchors.leftMargin: Theme.pageMargin
                spacing: 8
                Icon { name: "wifi-off"; size: 18; color: Theme.warning }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: 12
                    color: Theme.text
                    text: qsTr("Not connected to a network. Join a Wi-Fi network, or use Bluetooth for nearby devices.")
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

        // ---- peers
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: Peers.model
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            delegate: PeerItem {
                width: list.width
                selected: userId === root.selectedId
                onClicked: root.peerChosen(userId)
            }

            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 48
                visible: list.count === 0
                spacing: 10

                Icon {
                    Layout.alignment: Qt.AlignHCenter
                    name: "chat"
                    size: 44
                    color: Theme.border
                }
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    font.pixelSize: 16
                    font.weight: Font.Medium
                    color: Theme.text
                    text: Peers.filterText !== "" ? qsTr("No matches") : qsTr("Looking for people nearby…")
                }
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    font.pixelSize: 13
                    color: Theme.textSecondary
                    visible: Peers.filterText === ""
                    text: qsTr("Open LetsChat on another device on the same Wi-Fi, or tap the Bluetooth icon to connect to a nearby device.")
                }
                Button {
                    Layout.alignment: Qt.AlignHCenter
                    visible: Peers.filterText === ""
                    text: qsTr("Find via Bluetooth")
                    flat: true
                    onClicked: root.openBluetooth()
                }
            }
        }
    }
}
