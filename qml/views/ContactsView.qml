import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import LetsChat 1.0
import "../components"
import "../themes"
import "../dialogs"

Rectangle {
    id: root
    color: Theme.surface
    border.color: Theme.border
    border.width: 1
    
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: Theme.surface
            
            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                
                Label {
                    text: qsTr("Contacts")
                    font.pixelSize: 18
                    font.bold: true
                    color: Theme.text
                    Layout.fillWidth: true
                }
                
                Button {
                    text: "⋮"
                    flat: true
                    onClicked: settingsMenu.open()
                    
                    Menu {
                        id: settingsMenu
                        MenuItem {
                            text: qsTr("Settings")
                            onClicked: settingsDialog.open()
                        }
                        MenuItem {
                            text: qsTr("Change Alias")
                            onClicked: aliasDialog.open()
                        }
                    }
                }
            }
        }
        
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
        }
        
        ListView {
            id: peerListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: peerManager.peerModel
            clip: true
            
            delegate: PeerItem {
                width: peerListView.width
                peerData: model
            }
        }
        
        Label {
            text: qsTr("Scanning...")
            visible: peerManager.isScanning
            color: Theme.textSecondary
            Layout.alignment: Qt.AlignHCenter
            Layout.margins: 8
        }
    }
    
    AliasDialog {
        id: aliasDialog
    }
    
    Dialog {
        id: settingsDialog
        title: qsTr("Settings")
        modal: true
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.9, 400)
        
        ColumnLayout {
            anchors.fill: parent
            
            SettingsView {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }
}
