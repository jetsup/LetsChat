pragma Singleton
import QtQuick

QtObject {
    id: theme
    
    property bool isDarkMode: themeManager ? themeManager.isDarkMode : false
    
    property color background: themeManager ? themeManager.backgroundColor : "#ffffff"
    property color surface: themeManager ? themeManager.surfaceColor : "#f5f5f5"
    property color text: themeManager ? themeManager.textColor : "#1e1e1e"
    property color textSecondary: themeManager ? themeManager.textSecondaryColor : "#666666"
    property color accent: themeManager ? themeManager.accentColor : "#007AFF"
    property color border: themeManager ? themeManager.borderColor : "#e0e0e0"
    property color messageBubbleOwn: themeManager ? themeManager.messageBubbleOwn : "#007AFF"
    property color messageBubbleOther: themeManager ? themeManager.messageBubbleOther : "#e5e5ea"
    property color success: themeManager ? themeManager.successColor : "#34c759"
    property color warning: themeManager ? themeManager.warningColor : "#ff9500"
    property color error: themeManager ? themeManager.errorColor : "#ff3b30"
    
    property int borderRadius: 8
    property int spacing: 8
    property int padding: 12
}
