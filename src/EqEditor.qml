import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Edits the custom EQ curve: a response graph with a handle per band, and
// exact controls for the selected band. Changes play on Custom 1 as they're
// made; Save stores them on the headset.
ColumnLayout {
    id: editor
    property int band: 0
    readonly property real minLog: Math.log10(20)
    readonly property real maxLog: Math.log10(20000)
    readonly property real range: 15 // dB from the centre line to the edge
    readonly property var typeNames: [qsTr("Peak"), qsTr("Low shelf"), qsTr("High shelf"),
                                      qsTr("Low pass"), qsTr("High pass"), qsTr("Notch")]
    spacing: 10

    function value(list, i, fallback) { return list[i] !== undefined ? list[i] : fallback }
    function frequency(i) { return value(headset.eqFrequencies, i, 1000) }
    function gain(i) { return value(headset.eqGains, i, 0) }
    function q(i) { return value(headset.eqQ, i, 1) }
    function type(i) { return value(headset.eqTypes, i, 0) }
    // Pass and notch filters have no gain; their handles sit on the centre line.
    function hasGain(t) { return t <= 2 }
    function setBand(i, f, g, qv, t) {
        headset.setEqBand(i, Math.round(Math.max(20, Math.min(20000, f))),
                          Math.round(Math.max(-12, Math.min(12, g)) * 10) / 10,
                          Math.round(Math.max(0.1, Math.min(4, qv)) * 10) / 10, t)
    }
    function hzText(f) {
        return f >= 1000 ? (f / 1000).toLocaleString(Qt.locale(), 'f', f >= 10000 ? 1 : 2) + " kHz"
                         : Math.round(f).toLocaleString(Qt.locale(), 'f', 0) + " Hz"
    }
    function dbText(g) { return (g > 0 ? "+" : "") + g.toLocaleString(Qt.locale(), 'f', 1) + " dB" }

    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTr("Custom EQ"); font.pixelSize: 14; Layout.fillWidth: true }
        Label {
            visible: headset.eqDirty
            text: qsTr("Not saved")
            color: theme.accent
            font.pixelSize: 12
        }
    }

    // The response graph.
    Item {
        id: graph
        Layout.fillWidth: true
        Layout.preferredHeight: 170
        function xOf(f) { return (Math.log10(f) - editor.minLog) / (editor.maxLog - editor.minLog) * width }
        function fOf(x) { return Math.pow(10, editor.minLog + Math.max(0, Math.min(width, x)) / width * (editor.maxLog - editor.minLog)) }
        function yOf(db) { return height / 2 - db / editor.range * height / 2 }
        function dbOf(y) { return (height / 2 - y) / (height / 2) * editor.range }

        Rectangle { anchors.fill: parent; radius: Math.min(4, theme.radius); color: "#121216"; border.color: "#232329" }

        Canvas {
            id: canvas
            anchors.fill: parent
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                // Grid: 0 and ±6, ±12 dB; 100 Hz, 1 kHz, 10 kHz.
                ctx.lineWidth = 1
                for (const db of [-12, -6, 0, 6, 12]) {
                    ctx.strokeStyle = db === 0 ? "#3a3a42" : "#222228"
                    ctx.beginPath(); ctx.moveTo(0, graph.yOf(db)); ctx.lineTo(width, graph.yOf(db)); ctx.stroke()
                }
                ctx.strokeStyle = "#222228"
                for (const f of [100, 1000, 10000]) {
                    ctx.beginPath(); ctx.moveTo(graph.xOf(f), 0); ctx.lineTo(graph.xOf(f), height); ctx.stroke()
                }
                const points = headset.eqResponse
                if (points.length < 2)
                    return
                ctx.strokeStyle = theme.accent
                ctx.lineWidth = 2
                ctx.beginPath()
                for (let i = 0; i < points.length; ++i) {
                    const x = i / (points.length - 1) * width
                    const y = graph.yOf(Math.max(-editor.range, Math.min(editor.range, points[i])))
                    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
                }
                ctx.stroke()
            }
            Connections {
                target: headset
                function onEqualizerChanged() { canvas.requestPaint() }
            }
            Connections {
                target: theme
                function onChanged() { canvas.requestPaint() }
            }
        }

        Repeater {
            model: ["100", "1k", "10k"]
            Label {
                required property int index
                required property string modelData
                x: graph.xOf([100, 1000, 10000][index]) + 3
                y: graph.height - height - 2
                text: modelData
                color: "#5c5c64"
                font.pixelSize: 10
            }
        }

        // A handle per band: drag it to move the band, scroll to change Q.
        Repeater {
            model: 5
            Rectangle {
                id: handle
                required property int index
                objectName: "eqHandle" + index
                readonly property bool selected: index === editor.band
                width: 22; height: 22; radius: 11
                x: graph.xOf(editor.frequency(index)) - width / 2
                y: graph.yOf(editor.hasGain(editor.type(index)) ? editor.gain(index) : 0) - height / 2
                color: selected ? theme.accent : "#2a2a31"
                border.color: theme.accent
                border.width: 2
                Label {
                    anchors.centerIn: parent
                    text: (handle.index + 1).toLocaleString(Qt.locale(), 'f', 0)
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    color: handle.selected ? theme.accentForeground : "#ececf0"
                }
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -6
                    // Keeps the drag inside the scrolling window from turning
                    // into a scroll after a few pixels.
                    preventStealing: true
                    cursorShape: Qt.SizeAllCursor
                    onPressed: editor.band = handle.index
                    onPositionChanged: (mouse) => {
                        if (!pressed)
                            return
                        const p = mapToItem(graph, mouse.x, mouse.y)
                        const t = editor.type(handle.index)
                        editor.setBand(handle.index, graph.fOf(p.x),
                                       editor.hasGain(t) ? graph.dbOf(p.y) : editor.gain(handle.index),
                                       editor.q(handle.index), t)
                    }
                    onWheel: (wheel) => {
                        editor.band = handle.index
                        editor.setBand(handle.index, editor.frequency(handle.index), editor.gain(handle.index),
                                       editor.q(handle.index) + (wheel.angleDelta.y > 0 ? 0.1 : -0.1),
                                       editor.type(handle.index))
                    }
                }
            }
        }
    }

    SegmentedChoice {
        Layout.fillWidth: true
        options: [1, 2, 3, 4, 5].map(n => n.toLocaleString(Qt.locale(), 'f', 0))
        current: editor.band
        onChosen: (index) => editor.band = index
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 3
        columnSpacing: 12
        rowSpacing: 0

        Label { text: qsTr("Filter"); color: "#b4b4bb"; font.pixelSize: 13 }
        ComboBox {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            implicitHeight: 40
            font.pixelSize: 13
            focusPolicy: Qt.NoFocus
            Material.roundedScale: theme.radius
            model: editor.typeNames
            currentIndex: editor.type(editor.band)
            onActivated: (index) => editor.setBand(editor.band, editor.frequency(editor.band), editor.gain(editor.band),
                                                   editor.q(editor.band), index)
        }

        Label { text: qsTr("Frequency"); color: "#b4b4bb"; font.pixelSize: 13 }
        Slider {
            id: frequencySlider
            Layout.fillWidth: true
            from: editor.minLog; to: editor.maxLog
            focusPolicy: Qt.NoFocus
            onMoved: editor.setBand(editor.band, Math.pow(10, value), editor.gain(editor.band), editor.q(editor.band),
                                    editor.type(editor.band))
            Binding on value { value: Math.log10(editor.frequency(editor.band)); when: !frequencySlider.pressed }
        }
        Label {
            text: editor.hzText(editor.frequency(editor.band))
            Layout.preferredWidth: 70
            horizontalAlignment: Text.AlignRight
            font.features: { "tnum": 1 }
        }

        Label { text: qsTr("Gain"); color: "#b4b4bb"; font.pixelSize: 13; opacity: gainSlider.enabled ? 1 : 0.45 }
        Slider {
            id: gainSlider
            Layout.fillWidth: true
            enabled: editor.hasGain(editor.type(editor.band))
            from: -12; to: 12; stepSize: 0.5
            focusPolicy: Qt.NoFocus
            onMoved: editor.setBand(editor.band, editor.frequency(editor.band), value, editor.q(editor.band),
                                    editor.type(editor.band))
            Binding on value { value: editor.gain(editor.band); when: !gainSlider.pressed }
        }
        Label {
            text: editor.dbText(editor.gain(editor.band))
            opacity: gainSlider.enabled ? 1 : 0.45
            Layout.preferredWidth: 70
            horizontalAlignment: Text.AlignRight
            font.features: { "tnum": 1 }
        }

        Label { text: qsTr("Width (Q)"); color: "#b4b4bb"; font.pixelSize: 13 }
        Slider {
            id: qSlider
            Layout.fillWidth: true
            from: 0.1; to: 4; stepSize: 0.1
            focusPolicy: Qt.NoFocus
            onMoved: editor.setBand(editor.band, editor.frequency(editor.band), editor.gain(editor.band), value,
                                    editor.type(editor.band))
            Binding on value { value: editor.q(editor.band); when: !qSlider.pressed }
        }
        Label {
            text: editor.q(editor.band).toLocaleString(Qt.locale(), 'f', 1)
            Layout.preferredWidth: 70
            horizontalAlignment: Text.AlignRight
            font.features: { "tnum": 1 }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        Button {
            text: qsTr("Flat")
            flat: true
            focusPolicy: Qt.NoFocus
            Material.roundedScale: theme.radius
            onClicked: headset.flattenEq()
        }
        Item { Layout.fillWidth: true }
        Button {
            text: qsTr("Save to headset")
            enabled: headset.eqDirty
            highlighted: true
            focusPolicy: Qt.NoFocus
            Material.roundedScale: theme.radius
            Material.foreground: theme.accentForeground
            onClicked: headset.saveProfile()
        }
    }

    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: "#9a9aa2"
        font.pixelSize: 12
        text: qsTr("Edits play on Custom 1 as you make them. Save keeps them on the headset, with the rest of this profile. The graph is approximate.")
    }
}
