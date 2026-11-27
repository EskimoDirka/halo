import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Every shortcut, shown with ?.
Rectangle {
    id: help
    signal closed()
    color: "#d90e0e10"

    MouseArea { anchors.fill: parent; onClicked: help.closed() }

    Panel {
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 380)

        GridLayout {
            width: parent.width
            columns: 2
            columnSpacing: 18
            rowSpacing: 8

            Label { text: qsTr("Shortcuts"); font.pixelSize: 15; font.weight: Font.DemiBold; Layout.columnSpan: 2 }

            Repeater {
                model: [
                    ["Space", qsTr("Voice Prompts on or off")],
                    ["↑  ↓", qsTr("Volume up or down")],
                    ["K", qsTr("Keep my settings on or off")],
                    ["1 – 5", qsTr("Switch profile")],
                    ["R", qsTr("Read the headset again")],
                    ["Ctrl+Z", qsTr("Undo")],
                    ["Ctrl+Shift+Z", qsTr("Redo")],
                    ["?", qsTr("Show or hide shortcuts")],
                    ["Q", qsTr("Quit")]
                ]
                delegate: RowLayout {
                    required property var modelData
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    Label { text: modelData[0]; color: theme.accent; font.family: "monospace"; Layout.preferredWidth: 110 }
                    Label { text: modelData[1]; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                }
            }
        }
    }
}
