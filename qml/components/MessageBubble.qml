import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../themes"

Item {
    id: root
    height: messageLayout.height + 16
    
    property bool isOwn: model.isOwnMessage || false
    
    RowLayout {
        id: messageLayout
        anchors.left: isOwn ? undefined : parent.left
        anchors.right: isOwn ? parent.right : undefined
        anchors.margins: 12
        anchors.top: parent.top
        anchors.topMargin: 8
        width: Math.min(parent.width * 0.7, 300)
        
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: messageText.height + 16
            radius: 12
            color: isOwn ? Theme.messageBubbleOwn : Theme.messageBubbleOther
            
            Label {
                id: messageText
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 12
                text: model.content || ""
                color: isOwn ? "white" : Theme.text
                wrapMode: Text.WordWrap
                font.pixelSize: 14
            }
        }
    }
}
