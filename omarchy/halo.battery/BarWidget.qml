import QtQuick
import Quickshell
import Quickshell.Io
import qs.Commons
import qs.Commons as Commons
import qs.Ui

// The GHS Wireless InfinitePlay's battery on the bar. halo watch (the
// halo-watch service) publishes the headset's state to a small file as it
// changes, and the widget follows that file; without the service it asks
// halo once a minute instead. Hidden while the receiver is unplugged or the
// headset is off, so it only takes room when there's something to show.
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

  // When halo watch last wrote its file, in seconds; 0 when it hasn't.
  property real published: 0
  readonly property string statePath: Quickshell.env("XDG_RUNTIME_DIR") + "/halo/status.json"

  function fresh() { return published > 0 && Date.now() / 1000 - published < 90 }

  function refresh() {
    if (!statusProc.running) statusProc.running = true
  }

  function readState(raw) {
    raw = String(raw || "").trim()
    if (!raw || raw.length > 4096) {
      published = 0
      return
    }
    try {
      var state = JSON.parse(raw)
      var updated = Number(state.updated)
      published = isFinite(updated) ? updated : 0
      online = state.state === "online"
      headset = percent(state.battery && state.battery.headset)
      spare = percent(state.battery && state.battery.spare)
    } catch (e) {
      published = 0
    }
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

  FileView {
    id: stateFile
    path: root.statePath
    watchChanges: true
    printErrors: false
    onFileChanged: reload()
    onLoaded: root.readState(text())
    onLoadFailed: root.published = 0
  }

  // The file is replaced on each write, which a watch can miss; reading it
  // again every few seconds is cheap and catches up.
  Timer {
    interval: 5000
    running: true
    repeat: true
    onTriggered: stateFile.reload()
  }

  // Without a fresh file (no halo-watch service), ask halo directly.
  Timer {
    interval: 60000
    running: true
    repeat: true
    triggeredOnStart: true
    onTriggered: if (!root.fresh()) root.refresh()
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
