import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import LetsChat

ApplicationWindow {
    id: window
    width: 1000
    height: 700
    minimumWidth: 360
    minimumHeight: 480
    visible: true
    title: qsTr("LetsChat")
    color: Theme.background

    Material.theme: Theme.dark ? Material.Dark : Material.Light
    Material.accent: Theme.accent
    Material.primary: Theme.accent
    Material.background: Theme.background
    Material.foreground: Theme.text

    // Wide windows show the list and the chat side by side, narrow ones one at a time.
    readonly property bool wide: width >= 820
    // "", "settings" or "bluetooth"
    property string overlay: ""

    // The Android window already sits between the system bars, so no extra insets are needed.
    readonly property real safeTop: 0
    readonly property real safeBottom: 0

    Loader {
        anchors.fill: parent
        sourceComponent: Identity.onboarded ? shell : onboarding
    }

    Component {
        id: onboarding
        OnboardingView {}
    }

    Component {
        id: shell
        Item {
            ContactsView {
                id: contacts
                topInset: window.safeTop
                visible: window.wide || !Chat.hasCurrentPeer
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                width: window.wide ? 340 : parent.width
                selectedId: Chat.currentPeerId
                onPeerChosen: (id) => Chat.selectPeer(id)
                onOpenSettings: window.overlay = "settings"
                onOpenBluetooth: window.overlay = "bluetooth"
            }

            Rectangle {
                visible: window.wide
                anchors.left: contacts.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: Theme.border
            }

            ChatView {
                visible: window.wide || Chat.hasCurrentPeer
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.left: window.wide ? contacts.right : parent.left
                anchors.leftMargin: window.wide ? 1 : 0
                showBack: !window.wide
                topInset: window.safeTop
                bottomInset: window.safeBottom
                onBackRequested: Chat.closeChat()
            }

            Loader {
                anchors.fill: parent
                z: 10
                active: window.overlay !== ""
                sourceComponent: window.overlay === "settings" ? settingsPage : bluetoothPage
            }
        }
    }

    Component {
        id: settingsPage
        SettingsView {
            topInset: window.safeTop
            bottomInset: window.safeBottom
            onCloseRequested: window.overlay = ""
        }
    }

    Component {
        id: bluetoothPage
        BluetoothView {
            topInset: window.safeTop
            bottomInset: window.safeBottom
            onCloseRequested: window.overlay = ""
        }
    }

    Toast { id: toast }

    Connections {
        target: Chat
        function onNotice(text) { toast.show(text) }
    }

    Connections {
        target: Background
        function onShowWindowRequested() {
            window.show()
            window.raise()
            window.requestActivate()
        }
    }

    // Window close button (desktop) and the Back key (Android)
    onClosing: (close) => {
        if (Device.isAndroid) {
            if (overlay !== "") {
                overlay = ""
                close.accepted = false
            } else if (!wide && Chat.hasCurrentPeer) {
                Chat.closeChat()
                close.accepted = false
            } else if (Background.shouldHideOnClose()) {
                Background.moveToBackground()
                close.accepted = false
            }
        } else if (Background.shouldHideOnClose()) {
            close.accepted = false
            window.hide()
        }
    }
}
