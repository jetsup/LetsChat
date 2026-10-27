import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../components"
import "../themes"

Rectangle {
    id: root
    color: Theme.background
    
    signal onboardingComplete()
    
    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.8, 400)
        spacing: 24
        
        Label {
            text: qsTr("Welcome to LetsChat")
            font.pixelSize: 32
            font.bold: true
            color: Theme.text
            Layout.alignment: Qt.AlignHCenter
        }
        
        Label {
            text: qsTr("Chat with people on your local network")
            font.pixelSize: 16
            color: Theme.textSecondary
            Layout.alignment: Qt.AlignHCenter
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }
        
        ColumnLayout {
            spacing: 8
            Layout.fillWidth: true
            
            Label {
                text: qsTr("Your Device Name")
                font.pixelSize: 14
                color: Theme.text
            }
            
            TextField {
                id: deviceNameField
                text: deviceInfo.getDeviceName()
                enabled: false
                Layout.fillWidth: true
            }
        }
        
        ColumnLayout {
            spacing: 8
            Layout.fillWidth: true
            
            Label {
                text: qsTr("Choose an Alias (optional)")
                font.pixelSize: 14
                color: Theme.text
            }
            
            TextField {
                id: aliasField
                placeholderText: qsTr("Enter alias or leave empty to use device name")
                Layout.fillWidth: true
            }
        }
        
        Button {
            text: qsTr("Get Started")
            Layout.fillWidth: true
            Layout.topMargin: 16
            
            onClicked: {
                if (aliasField.text !== "") {
                    identityManager.alias = aliasField.text
                }
                onboardingComplete()
            }
        }
    }
}
