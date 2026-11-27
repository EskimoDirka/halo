import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// A switch with a title and a line on what it does.
RowLayout {
    id: row
    property string title
    property string detail
    property bool checked
    signal toggled(bool on)
    spacing: 12

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2
        opacity: row.enabled ? 1 : 0.45
        Label { text: row.title; font.pixelSize: 14; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        Label {
            visible: row.detail.length > 0
            text: row.detail
            color: "#9a9aa2"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
    Switch {
        focusPolicy: Qt.NoFocus
        checked: row.checked
        onToggled: row.toggled(checked)
        Material.roundedScale: theme.radius
    }
}
