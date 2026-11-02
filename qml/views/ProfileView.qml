import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Controls.Material
import QtQuick.Layouts
import LetsChat

// What we know about a contact; also where we give them a private nickname.
Rectangle {
    id: root
    color: Theme.background

    property string userId
    property real topInset: 0
    property real bottomInset: 0
    signal closeRequested()

    property var info: Peers.info(userId)
    property var sec: Security.peerSecurity(userId)
    Connections {
        target: Peers
        function onPeerChanged(id) { if (id === root.userId) root.info = Peers.info(id) }
    }
    Connections {
        target: Security
        function onPeerSecurityChanged(id) { if (id === root.userId) root.sec = Security.peerSecurity(id) }
        function onKeyChanged(id) { if (id === root.userId) root.sec = Security.peerSecurity(id) }
    }
    // the "connected since" clock
    Timer { interval: 30000; running: root.visible; repeat: true; onTriggered: root.info = Peers.info(root.userId) }

    function when(iso) {
        if (!iso) return qsTr("—")
        var d = new Date(iso)
        if (isNaN(d.getTime())) return qsTr("—")
        var now = new Date()
        var time = Qt.formatTime(d, "hh:mm")
        if (d.toDateString() === now.toDateString()) return qsTr("Today, %1").arg(time)
        return Qt.formatDate(d, "d MMM yyyy") + ", " + time
    }
    function modeText() {
        if (!info.isOnline) return qsTr("Not connected")
        var parts = []
        if (info.lanOnline) parts.push(qsTr("Wi-Fi"))
        if (info.btOnline) parts.push(qsTr("Bluetooth"))
        return parts.join(" + ")
    }

    // a labelled value
    component Field: ColumnLayout {
        property string label
        property string value
        property bool mono: false
        Layout.fillWidth: true
        spacing: 1
        Label { text: parent.label; font.pixelSize: 12; color: Theme.textSecondary }
        Label {
            Layout.fillWidth: true
            text: parent.value
            wrapMode: Text.WrapAnywhere
            font.pixelSize: 15
            font.family: parent.mono ? "monospace" : font.family
            color: Theme.text
        }
    }
    component Card: Rectangle {
        default property alias content: inner.data
        property string title
        Layout.fillWidth: true
        implicitHeight: inner.implicitHeight + 24 + (head.visible ? head.height + 6 : 0)
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border
        Label {
            id: head
            visible: parent.title !== ""
            x: 16; y: 12
            text: parent.title
            font.pixelSize: 12
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 0.8
            color: Theme.textSecondary
        }
        ColumnLayout {
            id: inner
            x: 16
            y: 12 + (head.visible ? head.height + 6 : 0)
            width: parent.width - 32
            spacing: 12
        }
    }

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
                spacing: 4
                IconButton { iconName: "back"; onClicked: root.closeRequested() }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Contact info")
                    font.pixelSize: 20
                    font.bold: true
                    color: Theme.text
                }
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
        }

        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: page.implicitHeight + 32 + root.bottomInset
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            ColumnLayout {
                id: page
                x: Math.max(Theme.pageMargin, (parent.width - 640) / 2)
                y: 16
                width: Math.min(parent.width - 2 * Theme.pageMargin, 640)
                spacing: 14

                // ---- identity
                Item {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    implicitHeight: identity.implicitHeight
                    ColumnLayout {
                        id: identity
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.width
                        spacing: 6
                        Avatar {
                            Layout.alignment: Qt.AlignHCenter
                            name: root.info.displayName || ""
                            seed: root.userId
                            source: root.info.avatarUrl || ""
                            size: 112
                        }
                        Label {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            text: root.info.displayName || ""
                            font.pixelSize: 22
                            font.weight: Font.DemiBold
                            color: Theme.text
                        }
                        Label {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            visible: (root.info.nickname || "") !== ""
                            text: qsTr("Calls themselves “%1”").arg(root.info.broadcastName || "")
                            font.pixelSize: 13
                            color: Theme.textSecondary
                        }
                        Label {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            text: root.info.isOnline ? qsTr("Online") : qsTr("Offline")
                            font.pixelSize: 13
                            color: root.info.isOnline ? Theme.success : Theme.textSecondary
                        }
                    }
                }

                // ---- nickname
                Card {
                    title: qsTr("Nickname")
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("Only you see this name. It is shown instead of the name they broadcast.")
                        font.pixelSize: 13
                        color: Theme.textSecondary
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Basic.TextField {
                            id: nick
                            Layout.fillWidth: true
                            text: root.info.nickname || ""
                            placeholderText: root.info.broadcastName || qsTr("Nickname")
                            placeholderTextColor: Theme.textSecondary
                            color: Theme.text
                            maximumLength: 40
                            font.pixelSize: 15
                            leftPadding: 16
                            rightPadding: 16
                            topPadding: 12
                            bottomPadding: 12
                            background: Rectangle {
                                radius: height / 2
                                color: Theme.surfaceAlt
                                border.width: nick.activeFocus ? 1 : 0
                                border.color: Theme.accent
                            }
                            onAccepted: save.clicked()
                        }
                        Button {
                            id: save
                            text: qsTr("Save")
                            highlighted: true
                            enabled: nick.text.trim() !== (root.info.nickname || "")
                            onClicked: { Peers.setNickname(root.userId, nick.text); nick.focus = false }
                        }
                    }
                }

                // ---- encryption / identity
                Card {
                    title: qsTr("Encryption")

                    // a contact whose key changed: nothing is exchanged until the user decides
                    Rectangle {
                        visible: root.sec.changed === true
                        Layout.fillWidth: true
                        implicitHeight: warnCol.implicitHeight + 24
                        radius: 10
                        color: Qt.rgba(Theme.error.r, Theme.error.g, Theme.error.b, 0.14)
                        border.color: Theme.error
                        ColumnLayout {
                            id: warnCol
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8
                            RowLayout {
                                spacing: 8
                                Icon { name: "warning"; size: 20; color: Theme.error }
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Safety number changed")
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                    color: Theme.error
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                font.pixelSize: 13
                                color: Theme.text
                                text: qsTr("Someone is connecting as %1 with a different security key (new key %2). This happens when the contact reinstalls LetsChat or changes device, but it could also be an impostor. Messages to and from this contact are blocked until you accept the new key. Compare the new safety number with them first.")
                                      .arg(root.info.displayName || "").arg(root.sec.newFingerprint || "")
                            }
                            Button {
                                text: qsTr("Accept new key")
                                highlighted: true
                                Material.accent: Theme.error
                                onClicked: Security.acceptNewKey(root.userId)
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Icon { name: "lock"; size: 20; color: Theme.success }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font.pixelSize: 14
                            color: Theme.text
                            text: root.sec.hasKey
                                  ? qsTr("Messages and files are end-to-end encrypted. Only you and this contact can read them.")
                                  : qsTr("Encryption starts the first time you exchange a message or connect.")
                        }
                    }

                    ColumnLayout {
                        visible: root.sec.hasKey === true
                        Layout.fillWidth: true
                        spacing: 6
                        Label {
                            text: qsTr("Safety number")
                            font.pixelSize: 12
                            color: Theme.textSecondary
                        }
                        // 12 groups of 5 digits, four rows of three
                        Label {
                            Layout.fillWidth: true
                            text: {
                                var g = (root.sec.safetyNumber || "").split(" ")
                                var rows = []
                                for (var i = 0; i < g.length; i += 3)
                                    rows.push(g.slice(i, i + 3).join("   "))
                                return rows.join("\n")
                            }
                            font.family: "monospace"
                            font.pixelSize: 15
                            lineHeight: 1.25
                            color: Theme.text
                        }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font.pixelSize: 12
                            color: Theme.textSecondary
                            text: qsTr("To make sure nobody is listening in, compare these numbers with the ones on %1's device (in person or over a call). If they match, mark the contact as verified.")
                                  .arg(root.info.displayName || "")
                        }
                        Field { label: qsTr("Their key fingerprint"); value: root.sec.fingerprint || ""; mono: true }
                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                Layout.fillWidth: true
                                text: root.sec.verified ? qsTr("Verified") : qsTr("Not verified")
                                font.pixelSize: 15
                                color: root.sec.verified ? Theme.success : Theme.text
                            }
                            Switch {
                                checked: root.sec.verified === true
                                onToggled: Security.setVerified(root.userId, checked)
                            }
                        }
                    }
                }

                // ---- connection
                Card {
                    title: qsTr("Connection")
                    Field { label: qsTr("Mode"); value: root.modeText() }
                    Field {
                        visible: (root.info.lanOnline || false) || ((root.info.ipAddress || "") !== "" && !(root.info.btOnline || false))
                        label: root.info.lanOnline ? qsTr("IP address (Wi-Fi)") : qsTr("Last known IP address")
                        value: root.info.ipAddress || ""
                        mono: true
                    }
                    Field {
                        visible: (root.info.btAddress || "") !== ""
                        label: qsTr("Bluetooth address")
                        value: root.info.btAddress || ""
                        mono: true
                    }
                    Field {
                        label: root.info.isOnline ? qsTr("Connected since") : qsTr("Last seen")
                        value: root.when(root.info.isOnline ? root.info.connectedSince : root.info.lastSeen)
                    }
                    Field { label: qsTr("First connected"); value: root.when(root.info.firstSeen) }
                }

                // ---- device
                Card {
                    title: qsTr("Device")
                    Field { label: qsTr("Device"); value: root.info.deviceName || "—" }
                    Field { label: qsTr("Name they broadcast"); value: root.info.alias || root.info.deviceName || "—" }
                    Field { label: qsTr("User ID"); value: (root.userId || "").substring(0, 13) + "…"; mono: true }
                }

                Button {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Delete chat…")
                    flat: true
                    Material.foreground: Theme.error
                    onClicked: confirm.open()
                }
            }
        }
    }

    Popup {
        id: confirm
        parent: Overlay.overlay
        x: parent ? (parent.width - width) / 2 : 0
        y: parent ? Math.max(60, parent.height / 4) : 0
        width: Math.min(parent ? parent.width - 32 : 340, 380)
        modal: true
        focus: true
        padding: 20
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onOpened: forceActiveFocus()
        background: Rectangle { radius: Theme.radius; color: Theme.surface; border.color: Theme.border }
        ColumnLayout {
            width: parent.width
            spacing: 14
            Label { text: qsTr("Delete this chat?"); font.pixelSize: 18; font.weight: Font.DemiBold; color: Theme.text }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("All messages and files with %1 are removed from this device.").arg(root.info.displayName || "")
                color: Theme.textSecondary
            }
            RowLayout {
                Layout.fillWidth: true
                Button { text: qsTr("Cancel"); flat: true; onClicked: confirm.close() }
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Delete")
                    highlighted: true
                    Material.accent: Theme.error
                    onClicked: { confirm.close(); Chat.deleteConversation(root.userId) }
                }
            }
        }
    }
}
