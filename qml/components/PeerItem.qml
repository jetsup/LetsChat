import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LetsChat

// One row of the chat list.  Tapping the picture raises avatarClicked() (quick-view dialog),
// tapping anywhere else opens the chat.
ItemDelegate {
    id: root

    required property string userId
    required property string displayName
    required property string lastMessage
    required property string lastActivity
    required property string transport
    required property string avatarUrl
    required property bool isOnline
    required property int unread
    property bool selected: false

    signal avatarClicked()

    implicitHeight: 72
    padding: 0
    hoverEnabled: true

    background: Rectangle {
        color: root.selected ? Theme.surfaceAlt
               : root.down ? Theme.pressed
               : root.hovered ? Theme.hover : "transparent"
    }

    contentItem: RowLayout {
        spacing: 12

        Avatar {
            Layout.leftMargin: Theme.pageMargin
            Layout.alignment: Qt.AlignVCenter
            name: root.displayName
            seed: root.userId
            source: root.avatarUrl
            size: 48
            showPresence: true
            online: root.isOnline

            // tapping the picture opens the quick-view dialog instead of the chat
            MouseArea {
                anchors.fill: parent
                onClicked: root.avatarClicked()
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 3

            RowLayout {
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: root.displayName
                    elide: Text.ElideRight
                    font.pixelSize: 16
                    font.weight: root.unread > 0 ? Font.Bold : Font.Medium
                    color: Theme.text
                }
                Label {
                    text: Theme.shortTime(root.lastActivity)
                    visible: root.lastMessage !== ""
                    font.pixelSize: 12
                    color: root.unread > 0 ? Theme.accent : Theme.textSecondary
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Label {
                    Layout.fillWidth: true
                    text: root.lastMessage !== "" ? root.lastMessage
                          : (root.isOnline ? qsTr("Say hello") : qsTr("Offline"))
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    font.pixelSize: 13
                    color: root.unread > 0 ? Theme.text : Theme.textSecondary
                }
                TransportBadge { transport: root.transport }
                Rectangle {
                    visible: root.unread > 0
                    implicitWidth: Math.max(20, badge.implicitWidth + 10)
                    implicitHeight: 20
                    radius: 10
                    color: Theme.accent
                    Label {
                        id: badge
                        anchors.centerIn: parent
                        text: root.unread > 99 ? "99+" : root.unread
                        color: Theme.textOnAccent
                        font.pixelSize: 11
                        font.bold: true
                    }
                }
            }
        }

        Item { implicitWidth: Theme.pageMargin - 12 }
    }
}
