import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LetsChat

// Full-screen photo viewer.  It does NOT close when the picture is tapped: use the X button, Esc
// (desktop) or Back (Android).  Zoom with pinch, mouse wheel, double-tap / double-click or the +/-
// buttons; drag to pan while zoomed; the download button saves the picture to the gallery / Pictures.
Popup {
    id: root
    property url source
    readonly property real minZoom: 1
    readonly property real maxZoom: 8

    parent: Overlay.overlay
    x: 0; y: 0
    width: parent ? parent.width : 0
    height: parent ? parent.height : 0
    modal: true
    focus: true
    padding: 0
    // Never on an outside press / tap on the picture.  On Android the Back key is owned by MainActivity,
    // which asks the UI to close the top-most dialog - if the popup also closed itself on Back, the
    // chat underneath would be closed by the same key press.
    closePolicy: Device.isAndroid ? Popup.NoAutoClose : Popup.CloseOnEscape
    background: Rectangle { color: "#F2000000" }

    // The popup covers the whole window, including the area under the status / navigation bars.
    // `anchorItem` is the page the viewer belongs to: whatever lies outside it is kept free.
    property Item anchorItem: null
    property real insetTop: 0
    property real insetBottom: 0
    function updateInsets() {
        if (!anchorItem || !parent) { insetTop = 0; insetBottom = 0; return }
        const p = anchorItem.mapToItem(parent, 0, 0)
        insetTop = Math.max(0, p.y)      // status bar; the keyboard must not count, so no bottom inset
    }
    onAboutToShow: { Qt.inputMethod.hide(); updateInsets(); resetZoom(); status.text = "" }
    onHeightChanged: updateInsets()
    onOpened: forceActiveFocus()

    function resetZoom() {
        img.scale = 1
        img.x = 0
        img.y = insetTop
    }
    function setZoom(z) {
        const s = Math.max(minZoom, Math.min(maxZoom, z))
        img.scale = s
        if (s <= 1.001) { img.x = 0; img.y = insetTop }
    }
    function save() {
        const where = Media.saveImage(root.source)
        status.text = where !== "" ? qsTr("Saved to %1").arg(where) : qsTr("Could not save the picture")
        statusTimer.restart()
    }

    contentItem: Item {
        clip: true
        focus: true
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Plus || event.key === Qt.Key_Equal) { setZoom(img.scale * 1.25); event.accepted = true }
            else if (event.key === Qt.Key_Minus) { setZoom(img.scale / 1.25); event.accepted = true }
            else if (event.key === Qt.Key_0) { resetZoom(); event.accepted = true }
        }

        Image {
            id: img
            x: 0
            y: root.insetTop
            width: parent.width
            height: parent.height - root.insetTop - root.insetBottom
            source: root.source
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            autoTransform: true
            smooth: true
            mipmap: true
            transformOrigin: Item.Center

            PinchHandler {
                id: pinch
                minimumScale: root.minZoom
                maximumScale: root.maxZoom
                minimumRotation: 0
                maximumRotation: 0
                onActiveChanged: if (!active && img.scale <= 1.001) { img.x = 0; img.y = root.insetTop }
            }
            DragHandler {
                // panning only makes sense when zoomed in
                enabled: img.scale > 1.001
                target: img
                xAxis.enabled: true
                yAxis.enabled: true
            }
            WheelHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: (event) => root.setZoom(img.scale * (event.angleDelta.y > 0 ? 1.15 : 1 / 1.15))
            }
            TapHandler {
                // double tap / double click: toggle between fit and 2.5x
                acceptedButtons: Qt.LeftButton
                gesturePolicy: TapHandler.ReleaseWithinBounds
                onDoubleTapped: { if (img.scale > 1.001) root.resetZoom(); else root.setZoom(2.5) }
            }
            Behavior on scale { enabled: !pinch.active; NumberAnimation { duration: 100 } }
        }

        BusyIndicator {
            anchors.centerIn: parent
            running: img.status === Image.Loading
            visible: running
        }

        // top bar: close on the left, tools on the right
        Rectangle {
            anchors.top: parent.top
            anchors.topMargin: root.insetTop
            anchors.left: parent.left
            anchors.right: parent.right
            height: 56
            gradient: Gradient {
                GradientStop { position: 0; color: "#99000000" }
                GradientStop { position: 1; color: "transparent" }
            }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                // Android has the Back key; the X is for desktop only
                IconButton { iconName: "close"; iconColor: "white"; visible: !Device.isAndroid; onClicked: root.close() }
                Item { Layout.fillWidth: true }
                IconButton {
                    iconName: "zoom-out"; iconColor: "white"
                    visible: Device.isDesktop
                    enabled: img.scale > 1.001
                    opacity: enabled ? 1 : 0.4
                    onClicked: root.setZoom(img.scale / 1.5)
                }
                IconButton {
                    iconName: "zoom-in"; iconColor: "white"
                    visible: Device.isDesktop
                    enabled: img.scale < root.maxZoom - 0.01
                    opacity: enabled ? 1 : 0.4
                    onClicked: root.setZoom(img.scale * 1.5)
                }
                IconButton { iconName: "download"; iconColor: "white"; onClicked: root.save() }
            }
        }

        // short confirmation after saving
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 28 + root.insetBottom
            visible: status.text !== ""
            radius: 18
            color: "#CC202020"
            width: status.implicitWidth + 32
            height: 36
            Label { id: status; anchors.centerIn: parent; color: "white"; font.pixelSize: 13 }
        }
        Timer { id: statusTimer; interval: 2800; onTriggered: status.text = "" }
    }
}
