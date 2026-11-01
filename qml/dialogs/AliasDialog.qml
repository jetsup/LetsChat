import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LetsChat

Dialog {
    id: root
    title: qsTr("Change your name")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(Overlay.overlay ? Overlay.overlay.width - 32 : 360, 380)
    standardButtons: Dialog.Ok | Dialog.Cancel

    onAboutToShow: {
        field.text = Identity.alias
        field.selectAll()
        field.forceActiveFocus()
    }
    onAccepted: Identity.alias = field.text

    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Others see this name next to your messages.")
            color: Theme.textSecondary
        }
        TextField {
            id: field
            Layout.fillWidth: true
            maximumLength: 40
            placeholderText: Identity.deviceName
            onAccepted: root.accept()
        }
    }
}
