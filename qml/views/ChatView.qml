import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import QtMultimedia
import LetsChat

Rectangle {
    id: root
    color: Theme.background

    property bool showBack: false
    property real topInset: 0
    property real bottomInset: 0
    signal backRequested()

    signal profileRequested()

    readonly property bool typing: input.text !== ""
    // anything Back / Esc should close before leaving the chat itself
    readonly property bool dialogOpen: photoDialog.opened || voiceDialog.opened || viewer.opened
                                       || deleteChatDialog.opened || actionSheet.opened || forwardSheet.opened
                                       || searchOpen || Chat.replyActive

    function closeDialogs() {
        if (viewer.opened) viewer.close()
        else if (photoDialog.opened) { Media.discardFile(photoDialog.source); photoDialog.close() }
        else if (voiceDialog.opened) { Media.discardFile(voiceDialog.source); voiceDialog.close() }
        else if (deleteChatDialog.opened) deleteChatDialog.close()
        else if (forwardSheet.opened) forwardSheet.close()
        else if (actionSheet.opened) actionSheet.close()
        else if (searchOpen) closeSearch()
        else if (Chat.replyActive) Chat.clearReply()
    }

    // After any popup closes Qt hands the focus back to the message field, which makes the on-screen
    // keyboard pop up. Park the focus on the page instead (phones); desktop gets the field back.
    Timer {
        id: hideKeyboardAgain
        interval: 120
        repeat: true
        property int left: 0
        // Android re-opens the keyboard a moment after the popup closed; keep hiding it briefly.
        onTriggered: {
            if (input.activeFocus) { stop(); return }
            Qt.inputMethod.hide()
            if (--left <= 0) stop()
        }
    }
    function afterPopup() {
        if (Device.isAndroid) {
            input.focus = false
            root.forceActiveFocus()
            Qt.inputMethod.hide()
            hideKeyboardAgain.left = 5
            hideKeyboardAgain.restart()
        } else {
            input.forceActiveFocus()
        }
    }

    // jump to a quoted message and flash it
    property int flashRow: -1
    Timer { id: flashTimer; interval: 1600; onTriggered: root.flashRow = -1 }
    function jumpTo(messageId) {
        var row = Chat.rowOf(messageId)
        if (row < 0) {
            Chat.notice(qsTr("The original message is no longer available"))
            return
        }
        list.positionViewAtIndex(row, ListView.Center)
        root.flashRow = row
        flashTimer.restart()
    }
    function startReply(messageId) {
        Chat.setReply(messageId)
        input.forceActiveFocus()
    }

    // ---- who we are talking to (nickname / picture / state), refreshed on changes
    property var peerInfo: Peers.info(Chat.currentPeerId)
    property var peerSec: Security.peerSecurity(Chat.currentPeerId)
    Connections {
        target: Chat
        function onCurrentPeerChanged() {
            root.peerInfo = Peers.info(Chat.currentPeerId)
            root.peerSec = Security.peerSecurity(Chat.currentPeerId)
        }
    }
    Connections {
        target: Security
        function onPeerSecurityChanged(id) { if (id === Chat.currentPeerId) root.peerSec = Security.peerSecurity(id) }
        function onKeyChanged(id) { if (id === Chat.currentPeerId) root.peerSec = Security.peerSecurity(id) }
    }

    // ---- in-chat search
    property string shownPeer: ""
    property bool searchOpen: false
    property var matches: []
    property var matchSet: ({})
    property int matchIndex: -1
    readonly property int currentRow: matchIndex >= 0 && matchIndex < matches.length ? matches[matchIndex] : -1

    function openSearch() { searchOpen = true; searchField.forceActiveFocus() }
    function closeSearch() { searchOpen = false; searchField.text = ""; runSearch() }
    function runSearch() {
        matches = Chat.searchMessages(searchField.text)
        var set = {}
        for (var i = 0; i < matches.length; ++i) set[matches[i]] = true
        matchSet = set
        matchIndex = matches.length > 0 ? matches.length - 1 : -1   // start at the newest match
        showMatch()
    }
    function step(delta) {
        if (matches.length === 0) return
        matchIndex = (matchIndex + delta + matches.length) % matches.length
        showMatch()
    }
    function showMatch() {
        if (currentRow >= 0) list.positionViewAtIndex(currentRow, ListView.Center)
    }

    // ---- empty state (wide layouts, nothing selected)
    ColumnLayout {
        anchors.centerIn: parent
        visible: !Chat.hasCurrentPeer
        spacing: 12
        Icon { Layout.alignment: Qt.AlignHCenter; name: "chat"; size: 64; color: Theme.border }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Select a conversation")
            font.pixelSize: 20
            font.weight: Font.Medium
            color: Theme.text
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Pick someone from the list to start chatting.")
            font.pixelSize: 14
            color: Theme.textSecondary
        }
    }

    ColumnLayout {
        anchors.fill: parent
        visible: Chat.hasCurrentPeer
        spacing: 0

        // ---- header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.headerHeight + root.topInset
            color: Theme.surface

            RowLayout {
                anchors.fill: parent
                anchors.topMargin: root.topInset
                anchors.leftMargin: root.showBack ? 4 : Theme.pageMargin
                anchors.rightMargin: Theme.pageMargin
                spacing: 10

                IconButton {
                    visible: root.showBack
                    iconName: "back"
                    onClicked: root.backRequested()
                }
                Avatar {
                    name: Chat.currentPeerName
                    seed: Chat.currentPeerId
                    source: root.peerInfo.avatarUrl || ""
                    size: 40
                    showPresence: true
                    online: Chat.currentPeerOnline
                    TapHandler { onTapped: root.profileRequested() }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    TapHandler { onTapped: root.profileRequested() }
                    Label {
                        Layout.fillWidth: true
                        text: Chat.currentPeerName
                        elide: Text.ElideRight
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        color: Theme.text
                    }
                    RowLayout {
                        spacing: 4
                        Label {
                            text: Chat.currentPeerOnline ? qsTr("Online") : qsTr("Offline")
                            font.pixelSize: 12
                            color: Chat.currentPeerOnline ? Theme.success : Theme.textSecondary
                        }
                        Icon {
                            name: "lock"
                            size: 12
                            color: root.peerSec.verified ? Theme.success : Theme.textSecondary
                        }
                        Label {
                            text: root.peerSec.verified ? qsTr("verified") : qsTr("encrypted")
                            font.pixelSize: 12
                            color: root.peerSec.verified ? Theme.success : Theme.textSecondary
                        }
                    }
                }
                TransportBadge { transport: Chat.currentPeerTransport }

                IconButton {
                    id: moreButton
                    iconName: "more"
                    iconColor: Theme.textSecondary
                    onClicked: chatMenu.popup(moreButton, moreButton.width - chatMenu.width, moreButton.height)

                    Menu {
                        id: chatMenu
                        background: Rectangle {
                            implicitWidth: 200
                            radius: 12
                            color: Theme.surfaceAlt
                            border.color: Theme.border
                        }
                        MenuItem { text: qsTr("Search"); onTriggered: root.openSearch() }
                        MenuItem { text: qsTr("Contact info"); onTriggered: root.profileRequested() }
                        MenuItem { text: qsTr("Delete chat…"); onTriggered: deleteChatDialog.open() }
                    }
                }
            }

            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
        }

        // ---- the contact's key changed: say so, loudly
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.peerSec.changed === true ? 44 : 0
            visible: root.peerSec.changed === true
            color: Qt.rgba(Theme.error.r, Theme.error.g, Theme.error.b, 0.16)
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.pageMargin
                anchors.rightMargin: Theme.pageMargin
                spacing: 8
                Icon { name: "warning"; size: 18; color: Theme.error }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Safety number changed — messages are blocked. Tap to review.")
                    font.pixelSize: 13
                    color: Theme.text
                    elide: Text.ElideRight
                }
            }
            TapHandler { onTapped: root.profileRequested() }
        }

        // ---- search bar (option menu -> Search)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.searchOpen ? 56 : 0
            visible: root.searchOpen
            color: Theme.surface
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.pageMargin
                anchors.rightMargin: 4
                spacing: 4
                Basic.TextField {
                    id: searchField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Search in this chat")
                    placeholderTextColor: Theme.textSecondary
                    color: Theme.text
                    font.pixelSize: 15
                    leftPadding: 16
                    rightPadding: 16
                    topPadding: 8
                    bottomPadding: 8
                    background: Rectangle {
                        radius: height / 2
                        color: Theme.surfaceAlt
                        border.width: searchField.activeFocus ? 1 : 0
                        border.color: Theme.accent
                    }
                    onTextChanged: root.runSearch()
                    onAccepted: root.step(-1)          // Enter: next older match
                }
                Label {
                    text: root.matches.length === 0
                          ? (searchField.text === "" ? "" : qsTr("0"))
                          : qsTr("%1/%2").arg(root.matchIndex + 1).arg(root.matches.length)
                    font.pixelSize: 13
                    color: Theme.textSecondary
                    Layout.minimumWidth: 36
                    horizontalAlignment: Text.AlignHCenter
                }
                IconButton { iconName: "up"; enabled: root.matches.length > 0; onClicked: root.step(-1) }
                IconButton { iconName: "down"; enabled: root.matches.length > 0; onClicked: root.step(1) }
                IconButton { iconName: "close"; onClicked: root.closeSearch() }
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
        }

        // ---- messages
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: Chat.messages
            spacing: 2
            topMargin: 8
            bottomMargin: 8
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            section.property: "dayString"
            section.delegate: Item {
                required property string section
                width: list.width
                height: 36
                Rectangle {
                    anchors.centerIn: parent
                    width: dayLabel.implicitWidth + 20
                    height: 22
                    radius: 11
                    color: Theme.surfaceAlt
                    Label {
                        id: dayLabel
                        anchors.centerIn: parent
                        text: parent.parent.section
                        font.pixelSize: 11
                        color: Theme.textSecondary
                    }
                }
            }

            delegate: MessageBubble {
                width: list.width
                playing: root.currentVoice === messageId
                         && voicePlayer.playbackState === MediaPlayer.PlayingState
                playProgress: root.currentVoice === messageId && voicePlayer.duration > 0
                              ? voicePlayer.position / voicePlayer.duration : 0
                searchMatch: root.matchSet[index] === true
                searchCurrent: index === root.currentRow || index === root.flashRow
                onReplyRequested: (id) => root.startReply(id)
                onQuoteClicked: (id) => root.jumpTo(id)
                onImageClicked: (source) => { viewer.source = source; viewer.open() }
                onVoiceToggled: (id, source) => root.toggleVoice(id, source)
                onContextRequested: (id, own, text, deleted) => {
                    Qt.inputMethod.hide()
                    actionSheet.messageId = id
                    actionSheet.own = own
                    actionSheet.text = text
                    actionSheet.deleted = deleted
                    actionSheet.open()
                }
            }

            function toEnd() { positionViewAtEnd() }
            onCountChanged: Qt.callLater(toEnd)
            onHeightChanged: Qt.callLater(toEnd)
            Component.onCompleted: Qt.callLater(toEnd)

            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                text: qsTr("No messages yet")
                color: Theme.textSecondary
                font.pixelSize: 14
            }
        }

        // ---- offline hint
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            visible: !Chat.currentPeerOnline
            color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.15)
            Label {
                anchors.centerIn: parent
                text: qsTr("%1 is offline. Messages are sent when they are back").arg(Chat.currentPeerName)
                font.pixelSize: 12
                color: Theme.text
            }
        }

        // ---- "replying to ..." bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Chat.replyActive ? 58 : 0
            visible: Chat.replyActive
            color: Theme.surface
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.border }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.pageMargin
                anchors.rightMargin: 4
                spacing: 10
                Rectangle { Layout.preferredWidth: 4; Layout.fillHeight: true; Layout.topMargin: 10; Layout.bottomMargin: 10; radius: 2; color: Theme.accent }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Label {
                        Layout.fillWidth: true
                        text: Chat.replyName === qsTr("You") ? qsTr("Replying to yourself") : qsTr("Replying to %1").arg(Chat.replyName)
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: Theme.accent
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        text: Chat.replyText
                        font.pixelSize: 13
                        color: Theme.textSecondary
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }
                }
                IconButton { iconName: "close"; iconColor: Theme.textSecondary; onClicked: Chat.clearReply() }
            }
        }

        // ---- composer
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: composerRow.implicitHeight + 20 + root.bottomInset
            color: Theme.surface

            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.border }

            RowLayout {
                id: composerRow
                x: 8
                y: 10
                width: parent.width - 16
                spacing: 4

                IconButton {
                    id: attachButton
                    Layout.alignment: Qt.AlignBottom
                    iconName: "attach"
                    iconColor: Theme.textSecondary
                    visible: !Media.recording
                    onClicked: attachMenu.open()

                    Menu {
                        id: attachMenu
                        y: -height - 6
                        background: Rectangle {
                            implicitWidth: 180
                            radius: 12
                            color: Theme.surfaceAlt
                            border.color: Theme.border
                        }
                        MenuItem { text: qsTr("Photo"); onTriggered: root.pick(true) }
                        MenuItem { text: qsTr("File"); onTriggered: root.pick(false) }
                    }
                }

                // message field: a pill that grows with the text (max ~5 lines)
                Rectangle {
                    id: pill
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignBottom
                    implicitHeight: Math.min(132, Math.max(44, input.contentHeight + 24))
                    radius: 22
                    color: Theme.surfaceAlt
                    border.width: input.activeFocus ? 1 : 0
                    border.color: Theme.accent

                    Flickable {
                        id: flick
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: 16
                        // camera + microphone only take room while the field is empty
                        anchors.right: tools.left
                        anchors.rightMargin: 4
                        clip: true
                        contentWidth: width
                        contentHeight: input.height
                        boundsBehavior: Flickable.StopAtBounds
                        visible: !Media.recording

                        Basic.TextArea.flickable: Basic.TextArea {
                            id: input
                            width: flick.width
                            wrapMode: TextEdit.Wrap
                            placeholderText: qsTr("Message")
                            placeholderTextColor: Theme.textSecondary
                            color: Theme.text
                            font.pixelSize: 15
                            selectByMouse: true
                            topPadding: 12
                            bottomPadding: 12
                            leftPadding: 0
                            rightPadding: 0
                            background: null
                            // Desktop: Enter sends, Shift+Enter adds a line. Android: Enter adds a line.
                            Keys.onPressed: (event) => {
                                if (Device.isDesktop && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                                        && !(event.modifiers & Qt.ShiftModifier)) {
                                    root.send()
                                    event.accepted = true
                                }
                            }
                        }
                        Basic.ScrollBar.vertical: Basic.ScrollBar {}
                    }

                    // recording feedback replaces the text field while the microphone is held
                    RowLayout {
                        anchors.left: parent.left
                        anchors.leftMargin: 18
                        anchors.right: tools.left
                        anchors.verticalCenter: parent.verticalCenter
                        visible: Media.recording
                        spacing: 10
                        Rectangle {
                            width: 10; height: 10; radius: 5
                            color: Theme.error
                            SequentialAnimation on opacity {
                                running: Media.recording
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.25; duration: 500 }
                                NumberAnimation { to: 1; duration: 500 }
                            }
                        }
                        Label {
                            text: root.fmt(Media.elapsedMs)
                            font.pixelSize: 16
                            color: Theme.text
                        }
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Release to finish")
                            elide: Text.ElideRight
                            font.pixelSize: 13
                            color: Theme.textSecondary
                        }
                    }

                    Row {
                        id: tools
                        anchors.right: parent.right
                        anchors.rightMargin: 4
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 0
                        spacing: 0
                        // hidden while typing so the text area gets the room
                        visible: !root.typing || Media.recording
                        width: visible ? implicitWidth : 0

                        IconButton {
                            visible: Media.cameraAvailable && !Media.recording
                            iconName: "camera"
                            iconColor: Theme.textSecondary
                            onClicked: Media.capturePhoto()
                        }
                        IconButton {
                            id: micButton
                            iconName: "mic"
                            iconColor: Media.recording ? Theme.error : Theme.textSecondary
                            iconSize: Media.recording ? 28 : 22
                            // press and hold to record, release to stop
                            onPressed: Media.startRecording()
                            onReleased: Media.stopRecording()
                            onCanceled: Media.stopRecording()
                        }
                    }
                }

                RoundButton {
                    Layout.alignment: Qt.AlignBottom
                    implicitWidth: 44
                    implicitHeight: 44
                    visible: root.typing
                    highlighted: true
                    focusPolicy: Qt.NoFocus
                    onClicked: root.send()
                    contentItem: Item {
                        Icon {
                            anchors.centerIn: parent
                            name: "send"
                            size: 20
                            color: Theme.textOnAccent
                        }
                    }
                }
            }
        }
    }

    // ------------------------------------------------------------------ attachments

    FileDialog {
        id: picker
        property bool photoMode: false
        title: photoMode ? qsTr("Choose a photo") : qsTr("Choose a file")
        onAccepted: {
            if (photoMode) {
                photoDialog.source = selectedFile
                photoDialog.open()
            } else {
                Chat.sendAttachment(selectedFile)
            }
        }
    }

    function pick(photo) {
        picker.photoMode = photo
        picker.nameFilters = photo
            ? [qsTr("Images (*.png *.jpg *.jpeg *.gif *.webp *.bmp)"), qsTr("All files (*)")]
            : [qsTr("All files (*)")]
        picker.open()
    }

    Connections {
        target: photoDialog
        function onClosed() { root.afterPopup() }
    }

    PhotoDialog {
        id: photoDialog
        onSendRequested: (source, caption) => Chat.sendAttachment(source, caption)
    }

    Connections {
        target: voiceDialog
        function onClosed() { root.afterPopup() }
    }

    VoiceDialog {
        id: voiceDialog
        onSendRequested: (source, ms) => Chat.sendVoice(source, ms)
        onDiscarded: (source) => Media.discardFile(source)
    }

    Connections {
        target: Media
        function onRecordingFinished(file, ms) {
            voiceDialog.source = file
            voiceDialog.durationMs = ms
            voiceDialog.open()
        }
        function onPhotoCaptured(file) {
            photoDialog.source = file
            photoDialog.open()
        }
        function onProblem(text) { Chat.notice(text) }
    }

    // ------------------------------------------------------------------ voice playback

    MediaPlayer {
        id: voicePlayer
        audioOutput: AudioOutput {}
        // at the end rewind, so the next tap on play starts from the beginning
        onMediaStatusChanged: if (mediaStatus === MediaPlayer.EndOfMedia) { stop() }
    }

    // Id of the message whose voice note is loaded in the player.  (Comparing URLs is unreliable:
    // file names with spaces come back percent-encoded from one side and not from the other.)
    property string currentVoice: ""

    function toggleVoice(messageId, source) {
        if (currentVoice === messageId) {
            // same note: pause / resume where it stopped
            if (voicePlayer.playbackState === MediaPlayer.PlayingState)
                voicePlayer.pause()
            else
                voicePlayer.play()
        } else {
            voicePlayer.stop()
            currentVoice = messageId
            voicePlayer.source = source
            voicePlayer.play()
        }
    }

    function fmt(ms) {
        var s = Math.floor(ms / 1000)
        return Math.floor(s / 60) + ":" + ("0" + (s % 60)).slice(-2)
    }

    // full-screen image viewer
    Popup {
        id: viewer
        property alias source: big.source
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: parent ? parent.width : 0
        height: parent ? parent.height : 0
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: 0
        onOpened: forceActiveFocus()
        onClosed: root.afterPopup()
        background: Rectangle { color: "#E6000000" }
        Image {
            id: big
            anchors.fill: parent
            anchors.margins: 12
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            autoTransform: true
        }
        MouseArea { anchors.fill: parent; onClicked: viewer.close() }
        IconButton {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 8
            iconName: "close"
            iconColor: "white"
            onClicked: viewer.close()
        }
    }

    // ------------------------------------------------------------------ message menu / delete chat

    // ---- message actions: a sheet at the bottom of the screen on phones, a card in the middle on desktop
    component SheetRow: ItemDelegate {
        property string iconName
        property color tint: Theme.text
        Layout.fillWidth: true
        implicitHeight: 52
        padding: 0
        contentItem: RowLayout {
            spacing: 16
            Icon { Layout.leftMargin: 20; name: parent.parent.iconName; size: 22; color: parent.parent.tint }
            Label {
                Layout.fillWidth: true
                text: parent.parent.text
                font.pixelSize: 16
                color: parent.parent.tint
            }
        }
        background: Rectangle { color: parent.down ? Theme.pressed : parent.hovered ? Theme.hover : "transparent" }
    }

    Popup {
        id: actionSheet
        property string messageId
        property bool own
        property bool deleted
        property string text
        readonly property bool phone: Device.isAndroid || root.width < 600

        // Parented to the chat page (not the full-screen overlay) so the sheet ends above the
        // system navigation bar instead of sliding underneath it.
        parent: root
        modal: true
        focus: true
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: phone ? root.width : 320
        x: (root.width - width) / 2
        y: phone ? root.height - height : Math.max(60, (root.height - height) / 3)
        onOpened: forceActiveFocus()
        onClosed: root.afterPopup()
        enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120 } }

        background: Rectangle {
            color: Theme.surface
            radius: Theme.radius
            // square off the bottom corners of the phone sheet
            Rectangle { visible: actionSheet.phone; anchors.bottom: parent.bottom; width: parent.width; height: parent.radius; color: parent.color }
            border.color: Theme.border
        }

        ColumnLayout {
            width: parent.width
            spacing: 0
            Item { Layout.preferredHeight: 8 }
            SheetRow {
                text: qsTr("Reply"); iconName: "reply"
                visible: !actionSheet.deleted
                Layout.preferredHeight: visible ? 52 : 0
                onClicked: { actionSheet.close(); root.startReply(actionSheet.messageId) }
            }
            SheetRow {
                text: qsTr("Forward"); iconName: "forward"
                visible: !actionSheet.deleted
                Layout.preferredHeight: visible ? 52 : 0
                onClicked: { actionSheet.close(); forwardSheet.messageId = actionSheet.messageId; forwardSheet.open() }
            }
            SheetRow {
                text: qsTr("Copy"); iconName: "copy"
                visible: actionSheet.text !== ""
                Layout.preferredHeight: visible ? 52 : 0
                onClicked: { actionSheet.close(); Chat.copyText(actionSheet.text) }
            }
            SheetRow {
                text: qsTr("Delete for me"); iconName: "delete"
                onClicked: { actionSheet.close(); Chat.deleteMessage(actionSheet.messageId, false) }
            }
            SheetRow {
                text: qsTr("Delete for everyone"); iconName: "delete"; tint: Theme.error
                visible: actionSheet.own && !actionSheet.deleted
                Layout.preferredHeight: visible ? 52 : 0
                onClicked: { actionSheet.close(); Chat.deleteMessage(actionSheet.messageId, true) }
            }
            Item { Layout.preferredHeight: 8 }
        }
    }

    // ---- forward picker
    Popup {
        id: forwardSheet
        property string messageId
        property string query: ""
        readonly property bool phone: Device.isAndroid || root.width < 600

        parent: root
        modal: true
        focus: true
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: phone ? root.width : 380
        height: phone ? root.height * 0.72 : Math.min(root.height - 120, 560)
        x: (root.width - width) / 2
        y: phone ? root.height - height : (root.height - height) / 2
        onAboutToShow: { query = ""; searchBox.text = ""; Qt.inputMethod.hide() }
        onOpened: forceActiveFocus()
        onClosed: root.afterPopup()

        background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.border
            Rectangle { visible: forwardSheet.phone; anchors.bottom: parent.bottom; width: parent.width; height: parent.radius; color: parent.color } }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    text: qsTr("Forward to…")
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    color: Theme.text
                }
                IconButton { iconName: "close"; iconColor: Theme.textSecondary; onClicked: forwardSheet.close() }
            }
            Basic.TextField {
                id: searchBox
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 8
                placeholderText: qsTr("Search people")
                placeholderTextColor: Theme.textSecondary
                color: Theme.text
                font.pixelSize: 15
                leftPadding: 16
                rightPadding: 16
                topPadding: 10
                bottomPadding: 10
                background: Rectangle { radius: height / 2; color: Theme.surfaceAlt
                    border.width: searchBox.activeFocus ? 1 : 0; border.color: Theme.accent }
                onTextChanged: forwardSheet.query = text.trim().toLowerCase()
            }
            ListView {
                id: targets
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: Peers.model
                boundsBehavior: Flickable.StopAtBounds
                delegate: ItemDelegate {
                    required property string userId
                    required property string displayName
                    required property string avatarUrl
                    required property bool isOnline
                    required property string searchText
                    readonly property bool shown: forwardSheet.query === "" || searchText.indexOf(forwardSheet.query) >= 0
                    visible: shown
                    width: targets.width
                    height: shown ? 64 : 0
                    padding: 0
                    onClicked: {
                        forwardSheet.close()
                        Chat.forwardMessage(forwardSheet.messageId, userId)
                    }
                    background: Rectangle { color: parent.down ? Theme.pressed : parent.hovered ? Theme.hover : "transparent" }
                    contentItem: RowLayout {
                        spacing: 14
                        Avatar { Layout.leftMargin: 20; name: displayName; seed: userId; source: avatarUrl; size: 40
                                 showPresence: true; online: isOnline }
                        Label { Layout.fillWidth: true; text: displayName; elide: Text.ElideRight
                                font.pixelSize: 16; color: Theme.text }
                        Label { Layout.rightMargin: 20; text: isOnline ? qsTr("Online") : qsTr("Offline")
                                font.pixelSize: 12; color: isOnline ? Theme.success : Theme.textSecondary }
                    }
                }
            }
            Item { Layout.preferredHeight: 4 }
        }
    }

    Popup {
        id: deleteChatDialog
        parent: Overlay.overlay
        x: parent ? (parent.width - width) / 2 : 0
        y: parent ? Math.max(60, parent.height / 4) : 0
        width: Math.min(parent ? parent.width - 32 : 340, 380)
        modal: true
        focus: true
        padding: 20
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onOpened: forceActiveFocus()
        onClosed: root.afterPopup()
        background: Rectangle { radius: Theme.radius; color: Theme.surface; border.color: Theme.border }
        ColumnLayout {
            width: parent.width
            spacing: 14
            Label {
                text: qsTr("Delete this chat?")
                font.pixelSize: 18
                font.weight: Font.DemiBold
                color: Theme.text
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("All messages and files with %1 are removed from this device. They keep their own copy.")
                      .arg(Chat.currentPeerName)
                color: Theme.textSecondary
            }
            RowLayout {
                Layout.fillWidth: true
                Button { text: qsTr("Cancel"); flat: true; onClicked: deleteChatDialog.close() }
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Delete")
                    highlighted: true
                    Material.accent: Theme.error
                    onClicked: { deleteChatDialog.close(); Chat.deleteConversation(Chat.currentPeerId) }
                }
            }
        }
    }

    function send() {
        if (input.text.trim() === "")
            return
        Chat.sendMessage(input.text)
        input.clear()
        input.forceActiveFocus()
    }

    Connections {
        target: Chat
        function onCurrentPeerChanged() {
            // this signal also fires for every state/nickname change; react to a *different* peer only
            if (Chat.currentPeerId === root.shownPeer)
                return
            root.shownPeer = Chat.currentPeerId
            if (!Chat.hasCurrentPeer) {      // chat closed: do not leave the keyboard hanging over the list
                input.focus = false
                Qt.inputMethod.hide()
            }
            root.closeSearch()
            voicePlayer.stop()
            root.currentVoice = ""
            if (Chat.hasCurrentPeer && !Qt.inputMethod.visible)
                input.forceActiveFocus()
        }
    }
}
