import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LetsChat

// One chat message. `status`: 0 sending, 1 delivered, 2 failed.
// `messageType`: 0 text, 1 image, 2 file.
Item {
    id: root

    required property int index
    required property string messageId
    required property string content
    required property bool isOwnMessage
    required property string timeString
    required property int status
    required property int messageType
    required property string attachmentName
    required property string attachmentUrl
    required property string attachmentSize
    required property string caption
    required property string durationText
    property bool playing: false          // voice notes: set by the chat view
    property real playProgress: 0
    signal voiceToggled(string messageId, url source)
    property bool searchMatch: false      // matches the in-chat search
    property bool searchCurrent: false    // the match the user is looking at
    signal contextRequested(string messageId, bool own, string text)
    property real maxBubbleWidth: Math.min(width * 0.78, 520)
    signal imageClicked(url source)

    readonly property bool isImage: messageType === 1
    readonly property bool isFile: messageType === 2
    readonly property bool isVoice: messageType === 3
    readonly property real imageWidth: Math.min(260, maxBubbleWidth - 8)

    implicitHeight: bubble.height + 6
    height: implicitHeight

    function openFile() {
        if (Device.isDesktop)
            Qt.openUrlExternally(attachmentUrl)
    }

    Rectangle {
        id: bubble
        readonly property real pad: root.isImage ? 4 : 12
        readonly property real contentW: root.isImage ? root.imageWidth
                                       : root.isFile ? Math.min(fileRow.implicitWidth, root.maxBubbleWidth - 24)
                                       : root.isVoice ? 220
                                       : text.implicitWidth
        width: Math.min(root.maxBubbleWidth, Math.max(contentW, meta.implicitWidth) + pad * 2)
        height: col.implicitHeight + (root.isImage ? 8 : 16)
        x: root.isOwnMessage ? root.width - width - Theme.pageMargin : Theme.pageMargin
        radius: 16
        topLeftRadius: root.isOwnMessage ? 16 : 4
        topRightRadius: root.isOwnMessage ? 4 : 16
        color: root.isOwnMessage ? Theme.bubbleOwn : Theme.bubbleOther
        border.width: root.searchCurrent ? 3 : (root.searchMatch ? 2 : (root.isOwnMessage || Theme.dark ? 0 : 1))
        border.color: root.searchCurrent ? Theme.warning : (root.searchMatch ? Qt.darker(Theme.warning, 1.3) : Theme.border)

        // right click (desktop) / long press (touch) -> message menu
        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: root.contextRequested(root.messageId, root.isOwnMessage,
                                            root.messageType === 0 ? root.content : root.caption)
        }
        TapHandler {
            acceptedButtons: Qt.LeftButton
            gesturePolicy: TapHandler.ReleaseWithinBounds
            longPressThreshold: 0.5
            onLongPressed: root.contextRequested(root.messageId, root.isOwnMessage,
                                                 root.messageType === 0 ? root.content : root.caption)
        }

        Column {
            id: col
            x: bubble.pad
            y: root.isImage ? 4 : 8
            width: bubble.width - bubble.pad * 2
            spacing: 2

            // ---- text
            Label {
                id: text
                visible: root.messageType === 0
                width: parent.width
                height: visible ? implicitHeight : 0
                text: root.content
                wrapMode: Text.Wrap
                font.pixelSize: 15
                color: root.isOwnMessage ? Theme.bubbleOwnText : Theme.bubbleOtherText
                textFormat: Text.PlainText
            }

            // ---- image
            Image {
                id: picture
                visible: root.isImage
                width: parent.width
                height: !visible ? 0
                        : status === Image.Ready ? Math.min(320, width * implicitHeight / implicitWidth) : 140
                source: visible ? root.attachmentUrl : ""
                sourceSize.width: 600
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                autoTransform: true
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.imageClicked(root.attachmentUrl)
                }
                Label {
                    anchors.centerIn: parent
                    visible: picture.status === Image.Error
                    text: qsTr("Image unavailable")
                    color: Theme.textSecondary
                }
            }

            // ---- caption of a photo
            Label {
                visible: root.isImage && root.caption !== ""
                width: parent.width - 8
                x: 4
                height: visible ? implicitHeight : 0
                topPadding: 4
                text: root.caption
                wrapMode: Text.Wrap
                font.pixelSize: 15
                textFormat: Text.PlainText
                color: root.isOwnMessage ? Theme.bubbleOwnText : Theme.bubbleOtherText
            }

            // ---- voice note
            VoiceNote {
                visible: root.isVoice
                width: parent.width
                height: visible ? implicitHeight : 0
                own: root.isOwnMessage
                playing: root.playing
                progress: root.playProgress
                durationText: root.durationText
                onToggled: root.voiceToggled(root.messageId, root.attachmentUrl)
            }

            // ---- generic file
            RowLayout {
                id: fileRow
                visible: root.isFile
                width: parent.width
                height: visible ? implicitHeight : 0
                spacing: 10
                Rectangle {
                    implicitWidth: 40
                    implicitHeight: 40
                    radius: 10
                    color: Qt.rgba(1, 1, 1, root.isOwnMessage ? 0.2 : 0)
                    border.width: root.isOwnMessage ? 0 : 1
                    border.color: Theme.border
                    Icon {
                        anchors.centerIn: parent
                        name: "file"
                        size: 22
                        color: root.isOwnMessage ? Theme.bubbleOwnText : Theme.accent
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Label {
                        Layout.fillWidth: true
                        text: root.attachmentName
                        elide: Text.ElideMiddle
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        color: root.isOwnMessage ? Theme.bubbleOwnText : Theme.bubbleOtherText
                    }
                    Label {
                        text: root.attachmentSize
                        font.pixelSize: 12
                        color: root.isOwnMessage ? Qt.rgba(1, 1, 1, 0.7) : Theme.textSecondary
                    }
                }
                TapHandler { enabled: root.isFile; onTapped: root.openFile() }
            }

            Row {
                id: meta
                anchors.right: parent.right
                anchors.rightMargin: root.isImage ? 6 : 0
                spacing: 4
                Label {
                    visible: root.isOwnMessage && root.status === 2
                    text: qsTr("Not delivered")
                    font.pixelSize: 11
                    color: "#ffd0d0"
                }
                Label {
                    visible: root.isOwnMessage && root.status === 3
                    text: qsTr("Waiting to send…")
                    font.pixelSize: 11
                    color: Qt.rgba(1, 1, 1, 0.7)
                }
                Label {
                    visible: root.isOwnMessage && root.status === 0
                    text: qsTr("Sending…")
                    font.pixelSize: 11
                    color: Qt.rgba(1, 1, 1, 0.7)
                }
                Label {
                    text: root.timeString
                    font.pixelSize: 11
                    color: root.isOwnMessage ? Qt.rgba(1, 1, 1, 0.7) : Theme.textSecondary
                }
            }
        }
    }
}
