import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 480
    height: 700
    minimumWidth: 420
    minimumHeight: 560
    visible: true
    title: "halo"
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property bool helpVisible: false
    readonly property bool online: headset.state === "online"

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Esc"; context: Qt.ApplicationShortcut; enabled: win.helpVisible; onActivated: win.helpVisible = false }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; onActivated: headset.toggleVoicePrompts() }
    Shortcut { sequences: ["Up", "+", "="]; context: Qt.ApplicationShortcut; onActivated: headset.stepVolume(2) }
    Shortcut { sequences: ["Down", "-"]; context: Qt.ApplicationShortcut; onActivated: headset.stepVolume(-2) }
    Shortcut { sequence: "K"; context: Qt.ApplicationShortcut; onActivated: headset.keepSettings = !headset.keepSettings }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; onActivated: headset.refresh() }
    Shortcut { sequences: [StandardKey.Undo]; context: Qt.ApplicationShortcut; onActivated: headset.undo() }
    Shortcut { sequences: [StandardKey.Redo, "Ctrl+Shift+Z"]; context: Qt.ApplicationShortcut; onActivated: headset.redo() }

    Connections {
        target: headset
        function onRestored() { toast.show(qsTr("Settings put back after the headset powered on")) }
    }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        // Tiled windows can be shorter than the content, so it scrolls.
        ScrollView {
            id: scroll
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: footer.top
            contentWidth: availableWidth
            contentHeight: content.implicitHeight + 40

            ColumnLayout {
                id: content
                x: 20
                y: 20
                width: scroll.availableWidth - 40
                spacing: 14

                // Header
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label {
                            text: "GHS Wireless InfinitePlay"
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            visible: win.online
                            text: qsTr("Profile %1").arg(headset.activeProfile.toLocaleString(Qt.locale(), 'f', 0))
                            color: "#9a9aa2"
                            font.pixelSize: 13
                        }
                    }
                    Rectangle {
                        width: 10; height: 10; radius: 5
                        color: win.online ? theme.accent : "#55555c"
                    }
                }

                // Shown when there is nothing to control, saying why.
                Panel {
                    visible: !win.online
                    Layout.fillWidth: true
                    ColumnLayout {
                        width: parent.width
                        spacing: 6
                        Label { text: stateTitle(); font.pixelSize: 15; font.weight: Font.DemiBold }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: "#b4b4bb"
                            text: headset.state === "missing"
                                  ? qsTr("halo connects as soon as the receiver or its base is plugged in.")
                                  : headset.state === "permission"
                                    ? qsTr("halo's udev rule lets you open the receiver. Reinstall halo, then unplug and replug the receiver.")
                                    : qsTr("Turn the headset on, or bring it into range.")
                        }
                    }
                }

                RowLayout {
                    visible: win.online
                    Layout.fillWidth: true
                    spacing: 14
                    BatteryGauge { Layout.fillWidth: true; Layout.preferredWidth: 1; label: qsTr("Headset"); level: headset.headsetBattery }
                    BatteryGauge { Layout.fillWidth: true; Layout.preferredWidth: 1; label: qsTr("Spare pack"); level: headset.spareBattery }
                }

                Panel {
                    visible: win.online
                    Layout.fillWidth: true
                    ColumnLayout {
                        width: parent.width
                        spacing: 4
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: qsTr("Volume"); Layout.fillWidth: true; font.pixelSize: 14 }
                            Label {
                                text: volumeSlider.value.toLocaleString(Qt.locale(), 'f', 0) + "%"
                                color: "#b4b4bb"
                                font.features: { "tnum": 1 }
                            }
                        }
                        Slider {
                            id: volumeSlider
                            Layout.fillWidth: true
                            from: 0; to: 100; stepSize: 2
                            focusPolicy: Qt.NoFocus
                            onMoved: headset.setVolume(value)
                            Binding on value { value: headset.volume; when: !volumeSlider.pressed }
                        }
                        SettingRow {
                            Layout.fillWidth: true
                            title: qsTr("Voice Prompts")
                            detail: qsTr("Spoken \"power on\" and battery announcements.")
                            checked: headset.voicePrompts
                            onToggled: (on) => headset.setVoicePrompts(on)
                        }
                    }
                }

                Panel {
                    visible: win.online
                    Layout.fillWidth: true
                    ColumnLayout {
                        width: parent.width
                        spacing: 12
                        SettingRow {
                            Layout.fillWidth: true
                            title: qsTr("Keep my settings")
                            detail: qsTr("The headset forgets Voice Prompts and volume when it turns off. halo puts them back for each profile.")
                            checked: headset.keepSettings
                            onToggled: (on) => headset.keepSettings = on
                        }
                        SettingRow {
                            Layout.fillWidth: true
                            enabled: headset.keepSettings
                            title: qsTr("Restore volume too")
                            detail: qsTr("Off keeps the headset's 50% start-up volume.")
                            checked: headset.restoreVolume
                            onToggled: (on) => headset.restoreVolume = on
                        }
                        Label {
                            visible: !isNaN(headset.lastRestored.getTime())
                            Layout.fillWidth: true
                            color: "#9a9aa2"
                            font.pixelSize: 12
                            // Reads translator.language so the time re-renders on a switch.
                            text: translator.language, qsTr("Last put back at %1")
                                  .arg(headset.lastRestored.toLocaleTimeString(Qt.locale(), Locale.ShortFormat))
                        }
                    }
                }

                Panel {
                    visible: win.online && headset.eqFrequencies.length > 0
                    Layout.fillWidth: true
                    ColumnLayout {
                        width: parent.width
                        spacing: 8
                        Label { text: qsTr("Equalizer"); font.pixelSize: 14 }
                        EqualizerBands { Layout.fillWidth: true; frequencies: headset.eqFrequencies }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: "#9a9aa2"
                            font.pixelSize: 12
                            text: qsTr("Read from the headset. Changing the EQ, sidetone, sleep timer, and lights comes once their commands are decoded.")
                        }
                    }
                }
            }
        }

        Label {
            id: footer
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 12
            topPadding: 8
            text: qsTr("Press ? for shortcuts")
            color: "#6c6c74"
            font.pixelSize: 12
        }

        Toast { id: toast; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 48 }

        ShortcutHelp { anchors.fill: parent; visible: win.helpVisible; onClosed: win.helpVisible = false }
    }

    function stateTitle() {
        switch (headset.state) {
        case "missing": return qsTr("Receiver not found")
        case "permission": return qsTr("Can't open the receiver")
        case "off": return qsTr("Headset is off")
        default: return qsTr("Connected")
        }
    }
}
