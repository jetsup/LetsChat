import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../components"
import "../themes"

Rectangle {
    id: root
    color: Theme.background
    
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: Theme.surface
            border.color: Theme.border
            border.width: 1
            
            Label {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Select a contact to start chatting")
                font.pixelSize: 16
                color: Theme.textSecondary
            }
        }
        
        ListView {
            id: messageListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: messageModel
            clip: true
            spacing: 4
            
            delegate: MessageBubble {
                width: messageListView.width
            }
        }
        
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
        }
        
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 70
            color: Theme.surface
            
            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                
                TextField {
                    id: messageField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Type a message...")
                    onAccepted: sendMessage()
                }
                
                Button {
                    text: qsTr("Send")
                    onClicked: sendMessage()
                }
            }
        }
    }
    
    function sendMessage() {
        if (messageField.text.trim() !== "") {
            // Send message logic
            messageField.clear()
        }
    }
}
