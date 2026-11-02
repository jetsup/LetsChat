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
    // "", "settings", "bluetooth" or "profile" (of profileUserId)
    property string overlay: ""
    property string profileUserId: ""
    onOverlayChanged: Qt.inputMethod.hide()
    function showProfile(id) { profileUserId = id; overlay = "profile" }

    // The Android window already sits between the system bars, so no extra insets are needed.
    readonly property real safeTop: 0
    readonly property real safeBottom: 0

    // Back (Android) always steps back one screen: page -> chat list, chat -> list, list -> background.
    property Item shell: null            // set by the main screen so Back can close its dialogs
    readonly property int backDepth: (shell && shell.dialogOpen ? 1 : 0)
                                     + (overlay !== "" ? 1 : 0)
                                     + (!wide && Chat.hasCurrentPeer ? 1 : 0)
    Binding { target: Background; property: "backDepth"; value: window.backDepth; when: Device.isAndroid }
    Connections {
        target: Background
        function onBackRequested() { window.handleBack() }
        // a notification was tapped: leave any page and show that conversation
        function onOpenChatRequested(peerId) {
            window.overlay = ""
            Chat.selectPeer(peerId)
        }
    }

    function handleBack() {
        if (shell && shell.dialogOpen) {
            shell.closeDialogs()
        } else if (overlay !== "") {
            overlay = ""
        } else if (!wide && Chat.hasCurrentPeer) {
            Chat.closeChat()
        } else if (Device.isAndroid && Background.shouldHideOnClose()) {
            Background.moveToBackground()
        } else if (Device.isAndroid) {
            Qt.quit()
        }
    }

    Item {
        anchors.fill: parent
        focus: true
        // On Android MainActivity owns the Back key (see MainActivity.java); swallow Qt's copy of it
        Keys.onBackPressed: (event) => { if (!Device.isAndroid) window.handleBack(); event.accepted = true }
        // Esc (desktop): close the top-most thing; with nothing open it leaves the chat, which shows the
        // "select a conversation" panel again.  Popups handle their own Esc first.
        Keys.onEscapePressed: (event) => {
            if (!Device.isAndroid) {
                if (shell && shell.dialogOpen) shell.closeDialogs()
                else if (overlay !== "") overlay = ""
                else if (Chat.hasCurrentPeer) Chat.closeChat()
            }
            event.accepted = true
        }

        Loader {
            anchors.fill: parent
            sourceComponent: Identity.onboarded ? shell : onboarding
        }
    }

    Component {
        id: onboarding
        OnboardingView {}
    }

    Component {
        id: shell
        Item {
            id: shellRoot
            property bool dialogOpen: chatView.dialogOpen || contacts.dialogOpen
            function closeDialogs() { chatView.closeDialogs(); contacts.closeDialogs() }
            Component.onCompleted: window.shell = shellRoot
            Component.onDestruction: if (window.shell === shellRoot) window.shell = null

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
                onProfileRequested: (id) => window.showProfile(id)
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
                id: chatView
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
                onProfileRequested: window.showProfile(Chat.currentPeerId)
            }

            Loader {
                anchors.fill: parent
                z: 10
                active: window.overlay !== ""
                sourceComponent: window.overlay === "settings" ? settingsPage
                                 : window.overlay === "profile" ? profilePage : bluetoothPage
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
        id: profilePage
        ProfileView {
            userId: window.profileUserId
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

    // Window close button. On Android the Back key normally arrives as Keys.onBackPressed above;
    // this is the fallback when the system closes the window instead.
    onClosing: (close) => {
        if (Device.isAndroid) {
            close.accepted = false      // never closed by Qt: MainActivity decides
        } else if (Background.shouldHideOnClose()) {
            close.accepted = false
            window.hide()
        }
    }
}
