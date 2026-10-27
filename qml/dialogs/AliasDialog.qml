import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../themes"

Dialog {
    id: root
    title: qsTr("Change Alias")
    modal: true
    anchors.centerIn: parent
    width: Math.min(parent.width * 0.9, 350)
    
    ColumnLayout {
        anchors.fill: parent
        spacing: 12
        
        Label {
            text: qsTr("Enter a new alias for this device")
            color: Theme.text
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        
        TextField {
            id: aliasInput
            text: identityManager.alias
            placeholderText: qsTr("Alias")
            Layout.fillWidth: true
        }
    }
    
    standardButtons: Dialog.Ok | Dialog.Cancel
    
    onAccepted: {
        identityManager.alias = aliasInput.text
    }
}
