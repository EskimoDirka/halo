import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// A raised section of the window, with corners that follow Hyprland's.
Pane {
    padding: 16
    Material.roundedScale: theme.radius
    background: Rectangle {
        color: "#17171b"
        radius: theme.radius
        border.color: "#232329"
    }
}
