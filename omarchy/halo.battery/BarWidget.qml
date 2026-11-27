import QtQuick
import Quickshell
import Quickshell.Io
import qs.Commons
import qs.Commons as Commons
import qs.Ui

// The GHS Wireless InfinitePlay's battery on the bar, read from halo once a
// minute. Hidden while the receiver is unplugged or the headset is off, so it
// only takes room when there's something to show.
BarWidget {
  id: root
  moduleName: "halo.battery"

  property bool online: false
  property int headset: -1
  property int spare: -1
  readonly property bool low: online && headset >= 0 && headset <= 15

  Commons.Translations { id: strings; directory: Qt.resolvedUrl("i18n") }

  // halo prints its own numbers; still, keep anything else out of the bar.
  function percent(value) {
    var n = Number(value)
    return isFinite(n) && n >= 0 && n <= 100 ? Math.round(n) : -1
  }

  function refresh() {
    if (!statusProc.running) statusProc.running = true
  }

  visible: online && headset >= 0
  implicitWidth: button.implicitWidth
  implicitHeight: button.implicitHeight

  Process {
    id: statusProc
    command: ["halo", "status", "--json"]
    stdout: StdioCollector {
      waitForEnd: true
      onStreamFinished: {
        var raw = String(text || "").trim()
        // A status is a few hundred bytes; anything far bigger isn't one.
        if (!raw || raw.length > 16384) {
          root.online = false
          return
        }
        try {
          var status = JSON.parse(raw)
          root.online = status.ok === true && status.state === "online"
          root.headset = root.percent(status.battery && status.battery.headset)
          root.spare = root.percent(status.battery && status.battery.spare)
        } catch (e) {
          root.online = false
        }
      }
    }
    // halo exits 1 when the headset is off or the receiver is missing, with
    // a JSON body saying so; a missing halo leaves no output at all.
    onExited: function(exitCode) {
      if (exitCode !== 0) root.online = false
    }
  }

  Timer {
    interval: 60000
    running: true
    repeat: true
    triggeredOnStart: true
    onTriggered: root.refresh()
  }

  TextMetrics {
    id: labelMetrics
    font.family: button.fontFamily
    font.pixelSize: Style.bar.iconFont
    text: button.labelText
  }

  BarIconButton {
    id: button
    anchors.fill: parent
    bar: root.bar
    text: "\u{f02ce}"
    // Room for the percentage beside the icon, with the space either side
    // the bar's other status icons get, and a little more so the number
    // doesn't crowd the icon before it.
    slotSize: labelText !== ""
      ? Style.bar.iconCanvas + Style.space(4) + labelMetrics.advanceWidth
        + (Style.bar.statusSlot - Style.bar.iconSize) + Style.space(8)
      : Style.bar.statusSlot
    labelText: !root.vertical && root.headset >= 0 ? Commons.I18n.formatNumber(root.headset) + "%" : ""
    opacity: root.low ? 1 : 0.9
    tooltipText: root.spare >= 0
      ? strings.t("tooltip").arg(Commons.I18n.formatNumber(root.headset)).arg(Commons.I18n.formatNumber(root.spare))
      : strings.t("tooltipHeadsetOnly").arg(Commons.I18n.formatNumber(root.headset))
    onPressed: if (root.bar) root.bar.run("halo open")
  }
}
