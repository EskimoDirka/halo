import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// One battery: the pack in the headset, or the spare charging in the base.
Panel {
    id: gauge
    property string label
    property int level: -1
    readonly property bool known: level >= 0 && level <= 100
    readonly property bool low: known && level <= 15

    ColumnLayout {
        width: parent.width
        spacing: 8
        Label { text: gauge.label; color: "#9a9aa2"; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true }
        Label {
            text: gauge.known ? gauge.level.toLocaleString(Qt.locale(), 'f', 0) + "%" : "–"
            font.pixelSize: 26
            font.weight: Font.DemiBold
            font.features: { "tnum": 1 }
            color: gauge.low ? "#ff6b6b" : "#ececf0"
        }
        Rectangle {
            Layout.fillWidth: true
            height: 6
            radius: Math.min(4, theme.radius)
            color: "#26262c"
            Rectangle {
                height: parent.height
                radius: parent.radius
                width: parent.width * (gauge.known ? gauge.level / 100 : 0)
                color: gauge.low ? "#ff6b6b" : theme.accent
            }
        }
    }
}
