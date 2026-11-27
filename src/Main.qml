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
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: win.close() }
    Shortcut { sequences: [StandardKey.Save]; context: Qt.ApplicationShortcut; onActivated: headset.saveProfile() }

    // Unsaved EQ edits play until the headset turns off; ask before leaving.
    property bool confirmedQuit: false
    onClosing: (close) => {
        if (headset.eqDirty && headset.online && !win.confirmedQuit) {
            close.accepted = false
            quitDialog.open()
        }
    }

    Dialog {
        id: quitDialog
        anchors.centerIn: parent
        // A fixed width: one tied to the window is negative while the window
        // is still being created, and Material's Dialog can't settle on it.
        // The window is never narrower than 420.
        width: 360
        modal: true
        title: qsTr("Save the custom EQ?")
        standardButtons: Dialog.Save | Dialog.Discard | Dialog.Cancel
        Label {
            id: quitText
            width: quitDialog.availableWidth
            wrapMode: Text.WordWrap
            text: qsTr("The edited curve plays now, but the headset forgets it when it turns off unless it's saved.")
        }
        onAccepted: quitAfter(true)
        onDiscarded: quitAfter(false)
        function quitAfter(save) {
            if (save)
                headset.saveProfile()
            win.confirmedQuit = true
            close()
            // Give the save a moment to leave before the window goes.
            quitTimer.start()
        }
    }
    Timer { id: quitTimer; interval: 300; onTriggered: Qt.quit() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; onActivated: headset.toggleVoicePrompts() }
    Shortcut { sequences: ["Up", "+", "="]; context: Qt.ApplicationShortcut; onActivated: headset.stepVolume(2) }
    Shortcut { sequences: ["Down", "-"]; context: Qt.ApplicationShortcut; onActivated: headset.stepVolume(-2) }
    Shortcut { sequence: "K"; context: Qt.ApplicationShortcut; onActivated: headset.keepSettings = !headset.keepSettings }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; onActivated: headset.refresh() }
    Shortcut { sequence: "1"; context: Qt.ApplicationShortcut; onActivated: headset.setActiveProfile(1) }
    Shortcut { sequence: "2"; context: Qt.ApplicationShortcut; onActivated: headset.setActiveProfile(2) }
    Shortcut { sequence: "3"; context: Qt.ApplicationShortcut; onActivated: headset.setActiveProfile(3) }
    Shortcut { sequence: "4"; context: Qt.ApplicationShortcut; onActivated: headset.setActiveProfile(4) }
    Shortcut { sequence: "5"; context: Qt.ApplicationShortcut; onActivated: headset.setActiveProfile(5) }
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

            // Stop at the edges instead of overshooting and springing back.
            Binding { target: scroll.contentItem; property: "boundsBehavior"; value: Flickable.StopAtBounds }

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
                        spacing: 8
                        Label { text: qsTr("Profile"); font.pixelSize: 14 }
                        SegmentedChoice {
                            Layout.fillWidth: true
                            options: [1, 2, 3, 4, 5].map(n => n.toLocaleString(Qt.locale(), 'f', 0))
                            current: headset.activeProfile - 1
                            onChosen: (index) => headset.setActiveProfile(index + 1)
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                color: "#9a9aa2"
                                font.pixelSize: 12
                                text: qsTr("Everything below belongs to the active profile, except volume and Bluetooth. Save to headset makes the headset keep it after it turns off.")
                            }
                            Button {
                                text: qsTr("Save to headset")
                                flat: true
                                focusPolicy: Qt.NoFocus
                                Material.roundedScale: theme.radius
                                onClicked: {
                                    headset.saveProfile()
                                    toast.show(qsTr("Profile %1 saved on the headset").arg(headset.activeProfile.toLocaleString(Qt.locale(), 'f', 0)))
                                }
                            }
                        }
                    }
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
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: qsTr("Game"); color: "#b4b4bb"; font.pixelSize: 13 }
                            Item { Layout.fillWidth: true }
                            Label {
                                // The balance runs 0 (all game) to 20 (all chat) in 5% steps.
                                text: qsTr("%1% game · %2% chat")
                                      .arg((100 - balanceSlider.value * 5).toLocaleString(Qt.locale(), 'f', 0))
                                      .arg((balanceSlider.value * 5).toLocaleString(Qt.locale(), 'f', 0))
                                color: "#b4b4bb"
                                font.pixelSize: 13
                                font.features: { "tnum": 1 }
                            }
                            Item { Layout.fillWidth: true }
                            Label { text: qsTr("Chat"); color: "#b4b4bb"; font.pixelSize: 13 }
                        }
                        Slider {
                            id: balanceSlider
                            Layout.fillWidth: true
                            from: 0; to: 20; stepSize: 1
                            focusPolicy: Qt.NoFocus
                            onMoved: headset.setBalance(value)
                            Binding on value { value: headset.balance; when: !balanceSlider.pressed }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.topMargin: 4
                            Label { text: qsTr("Equalizer"); Layout.fillWidth: true; font.pixelSize: 14 }
                            ComboBox {
                                Layout.preferredWidth: 180
                                implicitHeight: 40
                                font.pixelSize: 13
                                focusPolicy: Qt.NoFocus
                                Material.roundedScale: theme.radius
                                // Glorious Mode is a brand name, kept as is.
                                model: ["Glorious Mode", qsTr("Music"), qsTr("Immersion"), qsTr("Voice Call"), "FPS",
                                        qsTr("Custom %1").arg(1), qsTr("Custom %1").arg(2)]
                                currentIndex: headset.eqPreset - 1
                                onActivated: (index) => headset.setEqPreset(index + 1)
                            }
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
                        spacing: 8
                        Label { text: qsTr("Microphone"); font.pixelSize: 14 }
                        SegmentedChoice {
                            Layout.fillWidth: true
                            options: [qsTr("Low"), qsTr("Medium"), qsTr("High")]
                            current: headset.micVolume - 1
                            onChosen: (index) => headset.setMicVolume(index + 1)
                        }
                        SettingRow {
                            Layout.fillWidth: true
                            title: qsTr("Noise cancelling")
                            detail: qsTr("Filters background noise from the mic.")
                            checked: headset.noiseCancelling
                            onToggled: (on) => headset.setNoiseCancelling(on)
                        }
                        SettingRow {
                            Layout.fillWidth: true
                            title: qsTr("Sidetone")
                            detail: qsTr("Hear your own voice in the headset.")
                            checked: headset.sidetone
                            onToggled: (on) => headset.setSidetone(on)
                        }
                        SegmentedChoice {
                            Layout.fillWidth: true
                            enabled: headset.sidetone
                            opacity: enabled ? 1 : 0.45
                            options: [qsTr("Low"), qsTr("Medium"), qsTr("High")]
                            current: headset.sidetoneLevel - 1
                            onChosen: (index) => headset.setSidetoneLevel(index + 1)
                        }
                    }
                }

                Panel {
                    visible: win.online
                    Layout.fillWidth: true
                    SettingRow {
                        width: parent.width
                        title: qsTr("Bluetooth")
                        detail: qsTr("Pair the headset with a phone alongside the receiver.")
                        checked: headset.bluetooth
                        onToggled: (on) => headset.setBluetooth(on)
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
                            detail: qsTr("The headset resets settings like Voice Prompts and volume when it turns on. halo puts each profile's settings back.")
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
                        EqEditor { Layout.fillWidth: true }
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
