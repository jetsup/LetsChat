import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import QtQuick.Controls.Material
import LetsChat

Rectangle {
    id: root
    color: Theme.surface

    property string selectedId: ""
    property real topInset: 0
    signal openSettings()
    signal openBluetooth()
    signal peerChosen(string userId)
    signal profileRequested(string userId)

    // pull the list down (or press the refresh button) to look for people again
    property bool refreshing: false
    function doRefresh() {
        Chat.refreshPeers()
        refreshing = true
        refreshTimer.restart()
    }
    Timer { id: refreshTimer; interval: 2500; onTriggered: root.refreshing = false }

    // True while a dialog of this page is open (so Esc / Back close it first)
    readonly property bool dialogOpen: quickView.opened || deleteDialog.opened
    function closeDialogs() { quickView.close(); deleteDialog.close() }
    function askDelete(id, name) {
        deleteDialog.peerId = id
        deleteDialog.peerName = name
        deleteDialog.open()
    }

    // Delete the conversation and forget the contact: when they appear again it is a brand new contact.
    Popup {
        id: deleteDialog
        property string peerId
        property string peerName
        parent: Overlay.overlay
        x: parent ? (parent.width - width) / 2 : 0
        y: parent ? Math.max(60, parent.height / 4) : 0
        width: Math.min(parent ? parent.width - 32 : 340, 380)
        modal: true
        focus: true
        padding: 20
        closePolicy: (Device.isAndroid ? Popup.NoAutoClose : Popup.CloseOnEscape) | Popup.CloseOnPressOutside
        onOpened: forceActiveFocus()
        onClosed: { root.forceActiveFocus(); Qt.inputMethod.hide() }
        background: Rectangle { radius: Theme.radius; color: Theme.surface; border.color: Theme.border }
        ColumnLayout {
            width: parent.width
            spacing: 14
            Label {
                text: qsTr("Delete conversation?")
                font.pixelSize: 18
                font.weight: Font.DemiBold
                color: Theme.text
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textSecondary
                text: qsTr("All messages and files with %1 are removed from this device, and %1 is removed from your list together with the nickname and the saved security key. If they appear again you will meet as new contacts. They keep their own copy.")
                      .arg(deleteDialog.peerName)
            }
            RowLayout {
                Layout.fillWidth: true
                Button { text: qsTr("Cancel"); flat: true; onClicked: deleteDialog.close() }
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Delete")
                    highlighted: true
                    Material.accent: Theme.error
                    onClicked: { deleteDialog.close(); Chat.removeContact(deleteDialog.peerId) }
                }
            }
        }
    }

    // Tap on a picture: big picture (or initials) with "message" and "info" buttons.
    Popup {
        id: quickView
        property string peerId
        property string peerName
        property string peerAvatar
        property bool peerOnline
        parent: Overlay.overlay
        x: parent ? (parent.width - width) / 2 : 0
        y: parent ? Math.max(60, (parent.height - height) / 3) : 0
        width: Math.min(parent ? parent.width - 48 : 320, 320)
        modal: true
        focus: true
        padding: 24
        closePolicy: (Device.isAndroid ? Popup.NoAutoClose : Popup.CloseOnEscape) | Popup.CloseOnPressOutside
        onOpened: forceActiveFocus()
        // A popup hands focus back to whatever had it before - a text field, which pops the keyboard
        // up (and keeps it up).  Give focus to a plain item instead and make sure no keyboard stays.
        onClosed: { root.forceActiveFocus(); Qt.inputMethod.hide() }
        background: Rectangle { radius: Theme.radius; color: Theme.surface; border.color: Theme.border }

        ColumnLayout {
            width: parent.width
            spacing: 14

            Avatar {
                Layout.alignment: Qt.AlignHCenter
                name: quickView.peerName
                seed: quickView.peerId
                source: quickView.peerAvatar
                size: Math.min(220, quickView.width - 48)
            }
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: quickView.peerName
                elide: Text.ElideRight
                font.pixelSize: 20
                font.weight: Font.DemiBold
                color: Theme.text
            }
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: quickView.peerOnline ? qsTr("Online") : qsTr("Offline")
                font.pixelSize: 13
                color: quickView.peerOnline ? Theme.success : Theme.textSecondary
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 28
                ColumnLayout {
                    spacing: 4
                    IconButton {
                        Layout.alignment: Qt.AlignHCenter
                        implicitWidth: 52
                        implicitHeight: 52
                        iconName: "chat"
                        iconSize: 26
                        iconColor: Theme.accent
                        onClicked: { quickView.close(); root.peerChosen(quickView.peerId) }
                    }
                    Label { Layout.alignment: Qt.AlignHCenter; text: qsTr("Message"); font.pixelSize: 12; color: Theme.textSecondary }
                }
                ColumnLayout {
                    spacing: 4
                    IconButton {
                        Layout.alignment: Qt.AlignHCenter
                        implicitWidth: 52
                        implicitHeight: 52
                        iconName: "info"
                        iconSize: 26
                        iconColor: Theme.accent
                        onClicked: { quickView.close(); root.profileRequested(quickView.peerId) }
                    }
                    Label { Layout.alignment: Qt.AlignHCenter; text: qsTr("Info"); font.pixelSize: 12; color: Theme.textSecondary }
                }
                ColumnLayout {
                    spacing: 4
                    IconButton {
                        Layout.alignment: Qt.AlignHCenter
                        implicitWidth: 52
                        implicitHeight: 52
                        iconName: "delete"
                        iconSize: 26
                        iconColor: Theme.error
                        onClicked: { quickView.close(); root.askDelete(quickView.peerId, quickView.peerName) }
                    }
                    Label { Layout.alignment: Qt.AlignHCenter; text: qsTr("Delete"); font.pixelSize: 12; color: Theme.textSecondary }
                }
            }
        }
    }

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
                    iconName: "refresh"
                    iconColor: Theme.textSecondary
                    onClicked: root.doRefresh()
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

        // ---- "searching" strip while refreshing / being pulled
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: (root.refreshing || list.pulled) ? 34 : 0
            clip: true
            Behavior on Layout.preferredHeight { NumberAnimation { duration: 120 } }
            RowLayout {
                anchors.centerIn: parent
                spacing: 8
                BusyIndicator { running: root.refreshing; implicitWidth: 22; implicitHeight: 22; visible: root.refreshing }
                Label {
                    font.pixelSize: 12
                    color: Theme.textSecondary
                    text: root.refreshing ? qsTr("Looking for people…") : qsTr("Release to refresh")
                }
            }
        }

        // ---- peers
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: Peers.model
            // dragging past the top (touch or mouse) and letting go refreshes
            boundsBehavior: Flickable.DragOverBounds
            flickableDirection: Flickable.VerticalFlick
            property bool pulled: false
            onContentYChanged: if (dragging && contentY - originY < -70) pulled = true
            onDraggingChanged: if (!dragging) {
                if (pulled) root.doRefresh()
                pulled = false
            }
            ScrollBar.vertical: ScrollBar {}

            delegate: PeerItem {
                width: list.width
                selected: userId === root.selectedId
                onClicked: root.peerChosen(userId)
                // desktop: right-click a row to delete it (long-press stays disabled on touch screens)
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    acceptedDevices: PointerDevice.Mouse
                    onTapped: root.askDelete(userId, displayName)
                }
                onAvatarClicked: {
                    quickView.peerId = userId
                    quickView.peerName = displayName
                    quickView.peerAvatar = avatarUrl
                    quickView.peerOnline = isOnline
                    quickView.open()
                }
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
