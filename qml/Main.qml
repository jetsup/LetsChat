import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Window
import LetsChat 1.0

ApplicationWindow {
    id: window
    width: 900
    height: 600
    minimumWidth: 400
    minimumHeight: 500
    visible: true
    title: qsTr("LetsChat - LAN Messenger")
    
    property bool isOnboardingComplete: identityManager.isFirstRun === false && identityManager.userId !== ""
    
    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: isOnboardingComplete ? mainView : onboardingView
    }
    
    Component {
        id: onboardingView
        OnboardingView {
            onOnboardingComplete: {
                identityManager.generateIfNeeded()
                stackView.replace(mainView)
            }
        }
    }
    
    Component {
        id: mainView
        RowLayout {
            spacing: 0
            
            ContactsView {
                Layout.preferredWidth: 280
                Layout.fillHeight: true
                Layout.minimumWidth: 200
            }
            
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: themeManager.surfaceColor
                
                ChatView {
                    anchors.fill: parent
                }
            }
        }
    }
    
    Connections {
        target: lanTracker
        function onJoinedNewLan(networkId) {
            peerManager.handleNetworkChange()
            peerManager.startScanning()
        }
    }
    
    Component.onCompleted: {
        identityManager.generateIfNeeded()
        if (identityManager.isFirstRun) {
            // Will show onboarding
        } else {
            peerManager.setLocalIdentity(identityManager.userId, identityManager.alias, deviceInfo.getDeviceName())
            peerManager.startScanning()
            transferService.startServer()
            backgroundService.startBackgroundService()
            lanTracker.monitorNetwork()
        }
    }
}
