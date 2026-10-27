import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../themes"

ScrollView {
    id: root
    contentWidth: availableWidth
    
    ColumnLayout {
        width: root.availableWidth
        spacing: 16
        
        GroupBox {
            title: qsTr("Appearance")
            Layout.fillWidth: true
            
            ColumnLayout {
                anchors.fill: parent
                
                RowLayout {
                    Layout.fillWidth: true
                    
                    Label {
                        text: qsTr("Theme")
                        Layout.fillWidth: true
                    }
                    
                    ComboBox {
                        model: ["System", "Light", "Dark"]
                        currentIndex: {
                            if (themeManager.theme === "system") return 0
                            if (themeManager.theme === "light") return 1
                            return 2
                        }
                        onCurrentTextChanged: {
                            if (currentText === "System") themeManager.theme = "system"
                            else if (currentText === "Light") themeManager.theme = "light"
                            else themeManager.theme = "dark"
                        }
                    }
                }
            }
        }
        
        GroupBox {
            title: qsTr("Notifications")
            Layout.fillWidth: true
            
            ColumnLayout {
                anchors.fill: parent
                
                Switch {
                    text: qsTr("Enable Notifications")
                    checked: notificationManager.enabled
                    onToggled: notificationManager.enabled = checked
                }
            }
        }
        
        GroupBox {
            title: qsTr("Network")
            Layout.fillWidth: true
            
            ColumnLayout {
                anchors.fill: parent
                
                Switch {
                    text: qsTr("Run in Background")
                    checked: settingsManager.startInBackground
                    onToggled: settingsManager.startInBackground = checked
                }
            }
        }
        
        GroupBox {
            title: qsTr("Identity")
            Layout.fillWidth: true
            
            ColumnLayout {
                anchors.fill: parent
                
                RowLayout {
                    Layout.fillWidth: true
                    
                    Label {
                        text: qsTr("Your ID:")
                        Layout.fillWidth: true
                    }
                    
                    Label {
                        text: identityManager.userId.substring(0, 8) + "..."
                        elide: Text.ElideRight
                    }
                }
                
                RowLayout {
                    Layout.fillWidth: true
                    
                    Label {
                        text: qsTr("Alias:")
                        Layout.fillWidth: true
                    }
                    
                    Label {
                        text: identityManager.alias
                    }
                }
            }
        }
    }
}
