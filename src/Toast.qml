import QtQuick
import QtQuick.Controls

// A short note that fades out by itself.
Rectangle {
    id: toast
    property alias text: label.text
    width: label.implicitWidth + 32
    height: label.implicitHeight + 18
    radius: Math.min(8, theme.radius)
    color: "#26262c"
    border.color: theme.accent
    opacity: 0
    visible: opacity > 0

    function show(message) {
        label.text = message
        opacity = 1
        hide.restart()
    }

    Label { id: label; anchors.centerIn: parent; font.pixelSize: 13 }
    Timer { id: hide; interval: 4000; onTriggered: toast.opacity = 0 }
    Behavior on opacity { NumberAnimation { duration: 200 } }
}
