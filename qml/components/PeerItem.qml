import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../themes"

Rectangle {
    id: root
    height: 60
    color: mouseArea.containsMouse ? Theme.surface : "transparent"
    
    property var peerData
    
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        
        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10
            
            Rectangle {
                width: 40
                height: 40
                radius: 20
                color: Theme.accent
                
                Label {
                    anchors.centerIn: parent
                    text: (peerData.displayName || "U").substring(0, 2).toUpperCase()
                    color: "white"
                    font.bold: true
                }
            }
            
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                
                Label {
                    text: peerData.displayName || peerData.deviceName
                    color: Theme.text
                    font.pixelSize: 14
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                
                RowLayout {
                    spacing: 4
                    
                    StatusIndicator {
                        online: peerData.isOnline || false
                    }
                    
                    Label {
                        text: peerData.isOnline ? qsTr("Online") : qsTr("Offline")
                        color: peerData.isOnline ? Theme.success : Theme.textSecondary
                        font.pixelSize: 12
                    }
                }
            }
        }
    }
}
