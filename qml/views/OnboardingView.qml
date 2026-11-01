import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LetsChat

Rectangle {
    id: root
    color: Theme.background

    Flickable {
        anchors.fill: parent
        contentHeight: Math.max(height, card.implicitHeight + 48)
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Rectangle {
            id: card
            anchors.centerIn: parent
            width: Math.min(root.width - 32, 440)
            implicitHeight: content.implicitHeight + 56
            height: implicitHeight
            radius: 20
            color: Theme.surface
            border.color: Theme.border

            ColumnLayout {
                id: content
                anchors.fill: parent
                anchors.margins: 28
                spacing: 18

                Image {
                    Layout.alignment: Qt.AlignHCenter
                    source: "qrc:/icons/app.png"
                    sourceSize: Qt.size(160, 160)
                    Layout.preferredWidth: 80
                    Layout.preferredHeight: 80
                    smooth: true
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Welcome to LetsChat")
                    font.pixelSize: 26
                    font.bold: true
                    color: Theme.text
                }

                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: qsTr("Chat with people around you — over your Wi-Fi network or directly via Bluetooth. No internet or accounts needed.")
                    font.pixelSize: 14
                    color: Theme.textSecondary
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    spacing: 6

                    Label {
                        text: qsTr("Your name")
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        color: Theme.text
                    }
                    TextField {
                        id: aliasField
                        Layout.fillWidth: true
                        placeholderText: Identity.deviceName
                        maximumLength: 40
                        onAccepted: start.clicked()
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("This is how others see you. Leave empty to use “%1”.").arg(Identity.deviceName)
                        font.pixelSize: 12
                        color: Theme.textSecondary
                    }
                }

                Button {
                    id: start
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    implicitHeight: 48
                    text: qsTr("Get started")
                    highlighted: true
                    onClicked: Identity.completeOnboarding(aliasField.text)
                }
            }
        }
    }
}
