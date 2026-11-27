import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The custom EQ curve's five bands: each dot sits at its gain on a ±12 dB
// track, labelled with its frequency and gain. Read-only for now.
RowLayout {
    id: bands
    property var frequencies: []
    property var gains: []
    readonly property real range: 12
    spacing: 8

    Repeater {
        model: bands.frequencies
        ColumnLayout {
            required property int index
            required property int modelData
            readonly property real gain: bands.gains[index] !== undefined ? bands.gains[index] : 0
            Layout.fillWidth: true
            spacing: 4
            Label {
                Layout.alignment: Qt.AlignHCenter
                color: "#b4b4bb"
                font.pixelSize: 11
                font.features: { "tnum": 1 }
                // Units stay as they are in every language.
                text: (gain > 0 ? "+" : "") + gain.toLocaleString(Qt.locale(), 'f', 1) + " dB"
            }
            Item {
                id: track
                Layout.fillWidth: true
                Layout.preferredHeight: 64
                Rectangle { anchors.centerIn: parent; width: 4; height: parent.height; radius: 2; color: "#26262c" }
                Rectangle { anchors.centerIn: parent; width: 12; height: 1; color: "#3a3a42" }
                Rectangle {
                    width: 14; height: 14; radius: 7
                    color: theme.accent
                    x: (track.width - width) / 2
                    y: (track.height - height) / 2 - (Math.max(-bands.range, Math.min(bands.range, gain)) / bands.range)
                       * (track.height - height) / 2
                }
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                color: "#9a9aa2"
                font.pixelSize: 11
                text: modelData >= 1000
                      ? (modelData / 1000).toLocaleString(Qt.locale(), 'f', modelData % 1000 ? 1 : 0) + " kHz"
                      : modelData.toLocaleString(Qt.locale(), 'f', 0) + " Hz"
            }
        }
    }
}
