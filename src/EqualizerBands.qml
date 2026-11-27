import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The headset's EQ bands, read-only until the gain bytes are decoded. Each
// band sits on the flat line, labelled with its frequency.
RowLayout {
    id: bands
    property var frequencies: []
    spacing: 8

    Repeater {
        model: bands.frequencies
        ColumnLayout {
            required property int modelData
            Layout.fillWidth: true
            spacing: 6
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                Rectangle { anchors.centerIn: parent; width: 4; height: parent.height; radius: 2; color: "#26262c" }
                Rectangle { anchors.centerIn: parent; width: 14; height: 14; radius: 7; color: theme.accent }
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                color: "#9a9aa2"
                font.pixelSize: 11
                // Units stay as they are in every language.
                text: modelData >= 1000
                      ? (modelData / 1000).toLocaleString(Qt.locale(), 'f', modelData % 1000 ? 1 : 0) + " kHz"
                      : modelData.toLocaleString(Qt.locale(), 'f', 0) + " Hz"
            }
        }
    }
}
