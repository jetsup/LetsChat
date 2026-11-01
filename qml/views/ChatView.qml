import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import QtQuick.Dialogs
import LetsChat

Rectangle {
    id: root
    color: Theme.background

    property bool showBack: false
    property real topInset: 0
    property real bottomInset: 0
    signal backRequested()

    // ---- empty state (wide layouts, nothing selected)
    ColumnLayout {
        anchors.centerIn: parent
        visible: !Chat.hasCurrentPeer
        spacing: 12
        Icon { Layout.alignment: Qt.AlignHCenter; name: "chat"; size: 64; color: Theme.border }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Select a conversation")
            font.pixelSize: 20
            font.weight: Font.Medium
            color: Theme.text
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Pick someone from the list to start chatting.")
            font.pixelSize: 14
            color: Theme.textSecondary
        }
    }

    ColumnLayout {
        anchors.fill: parent
        visible: Chat.hasCurrentPeer
        spacing: 0

        // ---- header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.headerHeight + root.topInset
            color: Theme.surface

            RowLayout {
                anchors.fill: parent
                anchors.topMargin: root.topInset
                anchors.leftMargin: root.showBack ? 4 : Theme.pageMargin
                anchors.rightMargin: Theme.pageMargin
                spacing: 10

                IconButton {
                    visible: root.showBack
                    iconName: "back"
                    onClicked: root.backRequested()
                }
                Avatar {
                    name: Chat.currentPeerName
                    seed: Chat.currentPeerId
                    size: 40
                    showPresence: true
                    online: Chat.currentPeerOnline
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Label {
                        Layout.fillWidth: true
                        text: Chat.currentPeerName
                        elide: Text.ElideRight
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        color: Theme.text
                    }
                    Label {
                        text: Chat.currentPeerOnline ? qsTr("Online") : qsTr("Offline")
                        font.pixelSize: 12
                        color: Chat.currentPeerOnline ? Theme.success : Theme.textSecondary
                    }
                }
                TransportBadge { transport: Chat.currentPeerTransport }
            }

            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
        }

        // ---- messages
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: Chat.messages
            spacing: 2
            topMargin: 8
            bottomMargin: 8
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            section.property: "dayString"
            section.delegate: Item {
                required property string section
                width: list.width
                height: 36
                Rectangle {
                    anchors.centerIn: parent
                    width: dayLabel.implicitWidth + 20
                    height: 22
                    radius: 11
                    color: Theme.surfaceAlt
                    Label {
                        id: dayLabel
                        anchors.centerIn: parent
                        text: parent.parent.section
                        font.pixelSize: 11
                        color: Theme.textSecondary
                    }
                }
            }

            delegate: MessageBubble {
                width: list.width
                onImageClicked: (source) => { viewer.source = source; viewer.open() }
            }

            function toEnd() { positionViewAtEnd() }
            onCountChanged: Qt.callLater(toEnd)
            onHeightChanged: Qt.callLater(toEnd)
            Component.onCompleted: Qt.callLater(toEnd)

            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                text: qsTr("No messages yet")
                color: Theme.textSecondary
                font.pixelSize: 14
            }
        }

        // ---- offline hint
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            visible: !Chat.currentPeerOnline
            color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.15)
            Label {
                anchors.centerIn: parent
                text: qsTr("%1 is offline — messages may not be delivered").arg(Chat.currentPeerName)
                font.pixelSize: 12
                color: Theme.text
            }
        }

        // ---- composer
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: composerRow.implicitHeight + 20 + root.bottomInset
            color: Theme.surface

            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.border }

            RowLayout {
                id: composerRow
                x: 8
                y: 10
                width: parent.width - 16
                spacing: 4

                IconButton {
                    id: attachButton
                    Layout.alignment: Qt.AlignBottom
                    iconName: "attach"
                    iconColor: Theme.textSecondary
                    onClicked: attachMenu.open()

                    Menu {
                        id: attachMenu
                        y: -height - 6
                        MenuItem {
                            text: qsTr("Photo")
                            onTriggered: root.pick(true)
                        }
                        MenuItem {
                            text: qsTr("File")
                            onTriggered: root.pick(false)
                        }
                    }
                }

                // message field: pill that grows with the text (max ~5 lines)
                Rectangle {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignBottom
                    implicitHeight: Math.min(132, Math.max(44, input.contentHeight + 24))
                    radius: 22
                    color: Theme.surfaceAlt
                    border.width: input.activeFocus ? 1 : 0
                    border.color: Theme.accent

                    Flickable {
                        id: flick
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 12
                        clip: true
                        contentWidth: width
                        contentHeight: input.height
                        boundsBehavior: Flickable.StopAtBounds

                        Basic.TextArea.flickable: Basic.TextArea {
                            id: input
                            width: flick.width
                            wrapMode: TextEdit.Wrap
                            placeholderText: qsTr("Message")
                            placeholderTextColor: Theme.textSecondary
                            color: Theme.text
                            font.pixelSize: 15
                            selectByMouse: true
                            topPadding: 12
                            bottomPadding: 12
                            leftPadding: 0
                            rightPadding: 0
                            background: null
                            // Desktop: Enter sends, Shift+Enter adds a line. Android: Enter adds a line.
                            Keys.onPressed: (event) => {
                                if (Device.isDesktop && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                                        && !(event.modifiers & Qt.ShiftModifier)) {
                                    root.send()
                                    event.accepted = true
                                }
                            }
                        }
                        Basic.ScrollBar.vertical: Basic.ScrollBar {}
                    }
                }

                RoundButton {
                    Layout.alignment: Qt.AlignBottom
                    implicitWidth: 44
                    implicitHeight: 44
                    enabled: input.text.trim() !== ""
                    highlighted: true
                    focusPolicy: Qt.NoFocus
                    onClicked: root.send()
                    contentItem: Item {
                        Icon {
                            anchors.centerIn: parent
                            name: "send"
                            size: 20
                            color: Theme.textOnAccent
                        }
                    }
                }
            }
        }
    }

    FileDialog {
        id: picker
        title: qsTr("Choose what to send")
        onAccepted: Chat.sendAttachment(selectedFile)
    }

    function pick(photo) {
        picker.nameFilters = photo
            ? [qsTr("Images (*.png *.jpg *.jpeg *.gif *.webp *.bmp)"), qsTr("All files (*)")]
            : [qsTr("All files (*)")]
        picker.open()
    }

    // full-screen image viewer
    Popup {
        id: viewer
        property alias source: big.source
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: parent ? parent.width : 0
        height: parent ? parent.height : 0
        modal: true
        padding: 0
        background: Rectangle { color: "#E6000000" }
        Image {
            id: big
            anchors.fill: parent
            anchors.margins: 12
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            autoTransform: true
        }
        MouseArea { anchors.fill: parent; onClicked: viewer.close() }
        IconButton {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 8
            iconName: "close"
            iconColor: "white"
            onClicked: viewer.close()
        }
    }

    function send() {
        if (input.text.trim() === "")
            return
        Chat.sendMessage(input.text)
        input.clear()
        input.forceActiveFocus()
    }

    Connections {
        target: Chat
        function onCurrentPeerChanged() {
            if (Chat.hasCurrentPeer && !Qt.inputMethod.visible)
                input.forceActiveFocus()
        }
    }
}
