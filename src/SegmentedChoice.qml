import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// A row of buttons, one of them current: profiles, mic and sidetone levels.
RowLayout {
    id: choice
    property var options: []
    property int current: -1
    signal chosen(int index)
    spacing: 6

    Repeater {
        model: choice.options
        Button {
            id: button
            required property int index
            required property var modelData
            readonly property bool selected: index === choice.current
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            text: modelData
            focusPolicy: Qt.NoFocus
            onClicked: choice.chosen(index)

            // Drawn here rather than through Material, which only tints the
            // text of a flat button: a filled accent for the current choice.
            background: Rectangle {
                implicitHeight: 36
                radius: Math.min(4, theme.radius)
                color: button.selected ? theme.accent : button.hovered ? "#2a2a31" : "#202026"
            }
            contentItem: Label {
                text: button.text
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                font.pixelSize: 13
                font.weight: button.selected ? Font.DemiBold : Font.Normal
                color: button.selected ? theme.accentForeground : "#ececf0"
            }
        }
    }
}
