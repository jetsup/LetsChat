import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LetsChat

Rectangle {
    id: root
    color: Theme.background

    property real topInset: 0
    property real bottomInset: 0
    signal closeRequested()

    Component.onCompleted: if (Bluetooth.enabled) Bluetooth.startScan()
    Component.onDestruction: Bluetooth.stopScan()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.headerHeight + root.topInset
            color: Theme.surface

            RowLayout {
                anchors.fill: parent
                anchors.topMargin: root.topInset
                anchors.leftMargin: 4
                anchors.rightMargin: 6
                spacing: 4
                IconButton { iconName: "back"; onClicked: root.closeRequested() }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Label { text: qsTr("Nearby devices"); font.pixelSize: 20; font.bold: true; color: Theme.text }
                    Label {
                        text: Bluetooth.scanning ? qsTr("Scanning…") : qsTr("Bluetooth")
                        font.pixelSize: 12
                        color: Theme.textSecondary
                    }
                }
                BusyIndicator {
                    running: Bluetooth.scanning
                    visible: running
                    implicitWidth: 28
                    implicitHeight: 28
                }
                IconButton {
                    iconName: "refresh"
                    enabled: !Bluetooth.scanning && Bluetooth.enabled
                    onClicked: Bluetooth.startScan()
                }
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
        }

        // status / call-to-action
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: info.implicitHeight + 24
            visible: Bluetooth.statusText !== "" || !Bluetooth.enabled
            color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.15)
            RowLayout {
                id: info
                anchors.fill: parent
                anchors.margins: 12
                anchors.leftMargin: Theme.pageMargin
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: 13
                    color: Theme.text
                    text: !Bluetooth.enabled ? qsTr("Bluetooth messaging is switched off.") : Bluetooth.statusText
                }
                Button {
                    visible: !Bluetooth.enabled || Bluetooth.poweredOff
                    text: qsTr("Turn on")
                    flat: true
                    onClicked: {
                        if (!Bluetooth.enabled)
                            Bluetooth.enabled = true
                        if (Bluetooth.poweredOff)
                            Bluetooth.turnOn()
                        else
                            Bluetooth.startScan()
                    }
                }
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: Bluetooth.devices
            boundsBehavior: Flickable.StopAtBounds
            bottomMargin: root.bottomInset
            header: Item {
                width: list.width
                height: hint.implicitHeight + 28
                Label {
                    id: hint
                    x: Theme.pageMargin
                    y: 14
                    width: parent.width - 2 * Theme.pageMargin
                    wrapMode: Text.WordWrap
                    font.pixelSize: 13
                    color: Theme.textSecondary
                    text: qsTr("Open LetsChat on the other device, then connect to it below. Once connected it shows up in your chat list and messages travel over Bluetooth when Wi-Fi is not available.")
                }
            }

            delegate: ItemDelegate {
                id: row
                required property var modelData
                width: list.width
                implicitHeight: 68
                padding: 0
                background: Rectangle { color: row.down ? Theme.pressed : row.hovered ? Theme.hover : Theme.surface }

                contentItem: RowLayout {
                    spacing: 12
                    Rectangle {
                        Layout.leftMargin: Theme.pageMargin
                        Layout.alignment: Qt.AlignVCenter
                        implicitWidth: 44; implicitHeight: 44; radius: 22
                        color: Qt.rgba(Theme.bluetooth.r, Theme.bluetooth.g, Theme.bluetooth.b, 0.15)
                        Icon {
                            anchors.centerIn: parent
                            name: row.modelData.kind === "computer" ? "computer"
                                  : row.modelData.kind === "phone" ? "phone" : "bluetooth"
                            size: 22
                            color: Theme.bluetooth
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label {
                            Layout.fillWidth: true
                            text: row.modelData.name
                            elide: Text.ElideRight
                            font.pixelSize: 16
                            color: Theme.text
                        }
                        Label {
                            text: row.modelData.connected ? qsTr("Connected")
                                  : row.modelData.connecting ? qsTr("Connecting…")
                                  : row.modelData.paired ? qsTr("Paired · %1").arg(row.modelData.address)
                                  : row.modelData.address
                            font.pixelSize: 12
                            color: row.modelData.connected ? Theme.success : Theme.textSecondary
                        }
                    }
                    Button {
                        Layout.rightMargin: Theme.pageMargin - 6
                        Layout.alignment: Qt.AlignVCenter
                        enabled: !row.modelData.connecting
                        flat: row.modelData.connected
                        highlighted: !row.modelData.connected
                        text: row.modelData.connected ? qsTr("Disconnect") : qsTr("Connect")
                        onClicked: row.modelData.connected
                                   ? Bluetooth.disconnectDevice(row.modelData.address)
                                   : Bluetooth.connectToDevice(row.modelData.address)
                    }
                }
                onClicked: if (!row.modelData.connected && !row.modelData.connecting)
                               Bluetooth.connectToDevice(row.modelData.address)
            }

            footer: Item {
                width: list.width
                height: 90
                Button {
                    anchors.centerIn: parent
                    text: qsTr("Make this device visible")
                    flat: true
                    enabled: Bluetooth.enabled
                    onClicked: Bluetooth.makeDiscoverable()
                }
            }

            Label {
                anchors.centerIn: parent
                visible: list.count === 0 && !Bluetooth.scanning
                width: parent.width - 48
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: Theme.textSecondary
                text: qsTr("No devices found yet. Make sure Bluetooth is on and the other device is visible, then scan again.")
            }
        }
    }
}
