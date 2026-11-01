import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import LetsChat

Rectangle {
    id: root
    color: Theme.background

    property real topInset: 0
    property real bottomInset: 0
    signal closeRequested()

    AliasDialog { id: aliasDialog }

    FileDialog {
        id: photoPicker
        title: qsTr("Choose a profile photo")
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.gif *.webp *.bmp)"), qsTr("All files (*)")]
        onAccepted: if (!Identity.setAvatar(selectedFile)) Chat.notice(qsTr("Could not use that picture"))
    }

    // a titled card that stacks its children
    component Card: Rectangle {
        default property alias content: inner.data
        property string title
        Layout.fillWidth: true
        implicitHeight: inner.implicitHeight + 24 + (titleLabel.visible ? titleLabel.height + 4 : 0)
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border

        Label {
            id: titleLabel
            visible: parent.title !== ""
            x: 16; y: 12
            text: parent.title
            font.pixelSize: 12
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 0.8
            color: Theme.textSecondary
        }
        ColumnLayout {
            id: inner
            x: 16
            y: 12 + (titleLabel.visible ? titleLabel.height + 4 : 0)
            width: parent.width - 32
            spacing: 4
        }
    }

    // a label + description with a control on the right
    component Row2: RowLayout {
        property string title
        property string subtitle
        default property alias control: slot.data
        Layout.fillWidth: true
        spacing: 12
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1
            Label { text: title; font.pixelSize: 15; color: Theme.text; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Label {
                visible: subtitle !== ""
                text: subtitle
                font.pixelSize: 12
                color: Theme.textSecondary
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }
        RowLayout { id: slot }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.headerHeight + root.topInset
            color: Theme.surface
            RowLayout {
                anchors.fill: parent
                anchors.topMargin: root.topInset
                anchors.leftMargin: 4
                spacing: 4
                IconButton { iconName: "back"; onClicked: root.closeRequested() }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Settings")
                    font.pixelSize: 20
                    font.bold: true
                    color: Theme.text
                }
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
        }

        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: page.implicitHeight + 32 + root.bottomInset
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            ColumnLayout {
                id: page
                x: Math.max(Theme.pageMargin, (parent.width - 640) / 2)
                y: 16
                width: Math.min(parent.width - 2 * Theme.pageMargin, 640)
                spacing: 14

                // ---------------------------------------------------------------- profile
                Card {
                    title: qsTr("Profile")
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 14
                        Avatar {
                            name: Identity.alias
                            seed: Identity.userId
                            source: Identity.avatarUrl
                            size: 64
                            TapHandler { onTapped: photoPicker.open() }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Label { text: Identity.alias; font.pixelSize: 18; font.weight: Font.DemiBold; color: Theme.text }
                            Label { text: Identity.deviceName; font.pixelSize: 12; color: Theme.textSecondary }
                            Label {
                                text: qsTr("ID %1").arg(Identity.userId.substring(0, 8))
                                font.pixelSize: 11
                                color: Theme.textSecondary
                            }
                        }
                        Button { text: qsTr("Edit"); flat: true; onClicked: aliasDialog.open() }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Button { text: qsTr("Change photo"); flat: true; onClicked: photoPicker.open() }
                        Button {
                            text: qsTr("Remove photo")
                            flat: true
                            visible: Identity.avatarUrl !== ""
                            onClicked: Identity.clearAvatar()
                        }
                        Item { Layout.fillWidth: true }
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("Your photo is shared with people you chat with.")
                        font.pixelSize: 12
                        color: Theme.textSecondary
                    }
                }

                // ---------------------------------------------------------------- appearance
                Card {
                    title: qsTr("Appearance")
                    Row2 {
                        title: qsTr("Theme")
                        ComboBox {
                            id: themeBox
                            model: [qsTr("System"), qsTr("Light"), qsTr("Dark")]
                            readonly property var keys: ["system", "light", "dark"]
                            currentIndex: Math.max(0, keys.indexOf(ThemeManager.theme))
                            onActivated: ThemeManager.theme = keys[currentIndex]
                            implicitWidth: 130
                        }
                    }
                }

                // ---------------------------------------------------------------- background + notifications
                Card {
                    title: qsTr("Background & notifications")
                    Row2 {
                        title: qsTr("Keep running in background")
                        subtitle: Device.isAndroid
                                  ? qsTr("Shows a small status notification and keeps receiving messages while the app is closed.")
                                  : (Background.trayAvailable
                                     ? qsTr("Closing the window keeps LetsChat in the system tray.")
                                     : qsTr("No system tray found on this desktop; the window will close normally."))
                        Switch {
                            checked: Background.runInBackground
                            onToggled: Background.runInBackground = checked
                        }
                    }
                    Row2 {
                        visible: Device.isAndroid && Background.runInBackground
                        title: qsTr("Messages only arrive when I open the app?")
                        subtitle: qsTr("Some phones (%1) freeze background apps on their own, ignoring the normal settings. Allow LetsChat to run in the background in your phone's battery / app manager. Messages sent meanwhile are queued and delivered as soon as the phone wakes up.")
                                  .arg(Background.manufacturer)
                        Button {
                            text: qsTr("App settings")
                            flat: true
                            onClicked: Background.openAppSettings()
                        }
                    }
                    Row2 {
                        title: qsTr("Message notifications")
                        Switch {
                            checked: Notifier.enabled
                            onToggled: Notifier.enabled = checked
                        }
                    }
                    Row2 {
                        visible: Device.isAndroid && Background.runInBackground
                        title: qsTr("Unrestricted battery use")
                        subtitle: Background.batteryUnrestricted
                                  ? qsTr("Allowed — Android will not pause LetsChat while the screen is off.")
                                  : qsTr("Recommended so Android does not pause the connection when the screen is off.")
                        Button {
                            visible: !Background.batteryUnrestricted
                            text: qsTr("Allow")
                            flat: true
                            onClicked: Background.requestBatteryExemption()
                        }
                    }
                }

                // ---------------------------------------------------------------- bluetooth
                Card {
                    title: qsTr("Bluetooth")
                    Row2 {
                        title: qsTr("Bluetooth messaging")
                        subtitle: Bluetooth.statusText !== "" ? Bluetooth.statusText
                                  : (!Bluetooth.available ? qsTr("No Bluetooth adapter on this device")
                                     : Bluetooth.listening ? qsTr("Ready — nearby devices can connect")
                                     : qsTr("Off"))
                        Switch {
                            enabled: Bluetooth.available
                            checked: Bluetooth.enabled
                            onToggled: Bluetooth.enabled = checked
                        }
                    }
                }

                // ---------------------------------------------------------------- network
                Card {
                    title: qsTr("Network")
                    Row2 {
                        title: qsTr("This device")
                        subtitle: Chat.lanConnected ? Chat.localIp : qsTr("Not connected")
                    }
                    Row2 {
                        title: qsTr("Ports")
                        subtitle: qsTr("UDP %1 (discovery), TCP %2 (messages)").arg(45454).arg(45455)
                    }
                }

                // ---------------------------------------------------------------- about
                Card {
                    title: qsTr("About")
                    Row2 {
                        title: AppConfig.appName
                        subtitle: qsTr("Version %1 · local-network messenger").arg(AppConfig.appVersion)
                    }
                    Button {
                        visible: !Device.isAndroid
                        text: qsTr("Quit LetsChat")
                        flat: true
                        onClicked: Background.quit()
                    }
                }
            }
        }
    }
}
