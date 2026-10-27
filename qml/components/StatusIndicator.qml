import QtQuick
import "../themes"

Rectangle {
    id: root
    width: 8
    height: 8
    radius: 4
    
    property bool online: false
    
    color: online ? Theme.success : Theme.textSecondary
}
