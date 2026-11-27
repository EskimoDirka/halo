#include "cli.h"

#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>
#include <QTimer>
#include <cmath>
#include <functional>

#include "headset.h"
#include "hidtransport.h"

namespace {

QTextStream out(stdout), err(stderr);

const char *Usage = R"(halo: settings for the Glorious GHS Wireless InfinitePlay headset.

  halo open                       open the window
  halo status [--json]            show battery and every setting
  halo set volume <0-100>         headset volume, in percent
  halo set prompts <on|off>       Voice Prompts
  halo set balance <0-20>         game/chat balance; 0 all game, 10 even, 20 all chat
  halo set eq <preset>            glorious, music, immersion, voice, fps, custom1, custom2 (or 1-7)
  halo set mic <low|mid|high>     microphone level
  halo set enc <on|off>           microphone noise cancelling
  halo set sidetone <off|low|mid|high>
  halo set bluetooth <on|off>
  halo set profile <1-5>          switch the active profile
  halo set autooff <on|off>       power off after 30 idle minutes (not reported back)
  halo set mute <on|off>          mute the microphone (not reported back)
  halo save                       store the active profile on the headset, so it
                                  keeps these settings by itself after power-on
  halo eq [--json]                show the custom EQ curve
  halo eq band <1-5> [hz=<20-20000>] [db=<-12..12>] [q=<0.1-4>]
          [type=peak|lowshelf|highshelf|lowpass|highpass|notch]
                                  change one band; the EQ switches to Custom 1
  halo eq flat                    reset the curve to flat
                                  (eq band and eq flat take --save)
  halo watch                      put settings back after each power-on
  halo skill install              install the agent skill

Per-profile settings apply to the active profile. set and status take --json.
Exit status is 0 on success, 1 on error.
)";

// Runs the event loop until done() holds or the timeout passes.
bool waitFor(Headset &headset, const std::function<bool()> &done, int timeoutMs) {
    if (done())
        return true;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    auto check = [&] { if (done()) loop.quit(); };
    QObject::connect(&headset, &Headset::stateChanged, &loop, check);
    QObject::connect(&headset, &Headset::statusChanged, &loop, check);
    QObject::connect(&headset, &Headset::equalizerChanged, &loop, check);
    QObject::connect(&headset, &Headset::statusReported, &loop, check);
    timeout.start(timeoutMs);
    loop.exec();
    return done();
}

// Waits for the headset, or explains why it isn't there.
bool connect(Headset &headset, const HidTransport &transport, bool json) {
    waitFor(headset, [&] { return headset.online(); }, 2500);
    QString error;
    const QString state = headset.state();
    if (state == "missing")
        error = "No GHS Wireless InfinitePlay receiver found. Plug in the receiver or its base.";
    else if (state == "permission")
        error = QString("Found the receiver at %1 but can't open it. halo's udev rule gives the logged-in "
                        "user access; install the halo package, then unplug and replug the receiver.")
                    .arg(transport.devicePath());
    else if (state == "off")
        error = "The receiver is connected, but the headset is off or out of range.";
    if (error.isEmpty())
        return true;
    if (json)
        out << QJsonDocument(QJsonObject{{"ok", false}, {"state", state}, {"error", error}}).toJson();
    err << error << "\n";
    return false;
}

QJsonObject statusJson(const Headset &headset) {
    const auto &eq = headset.equalizer();
    QJsonArray frequencies, gains, q, types;
    for (int band = 0; band < eq.frequencies.size(); ++band) {
        frequencies << eq.frequencies[band];
        gains << eq.gains[band];
        q << eq.q[band];
        types << eq.types[band];
    }
    return {
        {"ok", true},
        {"state", headset.state()},
        {"volume", headset.volume()},
        {"volumeRaw", headset.volumeRaw()},
        {"bluetooth", headset.bluetooth()},
        {"activeProfile", headset.activeProfile()},
        {"voicePrompts", headset.voicePrompts()},
        {"balance", headset.balance()},
        {"eqPreset", headset.eqPreset()},
        {"eqPresetName", Protocol::eqPresetName(headset.eqPreset())},
        {"micVolume", headset.micVolume()},
        {"noiseCancelling", headset.noiseCancelling()},
        {"sidetone", headset.sidetone()},
        {"sidetoneLevel", headset.sidetoneLevel()},
        {"battery", QJsonObject{{"headset", headset.headsetBattery()}, {"spare", headset.spareBattery()}}},
        {"customEq", QJsonObject{{"frequencies", frequencies}, {"gains", gains}, {"q", q}, {"types", types}}},
        {"keepSettings", headset.keepSettings()},
        {"restoreVolume", headset.restoreVolume()},
        // The raw replies, for working out the fields not yet understood.
        {"raw", QJsonObject{{"status", QString(headset.status().payload.toHex(' '))},
                            {"equalizer", QString(eq.payload.toHex(' '))}}},
    };
}

QString levelName(int level) {
    return level == 1 ? "low" : level == 2 ? "mid" : level == 3 ? "high" : "unknown";
}

void printStatus(const Headset &headset) {
    auto battery = [](int level) { return level < 0 ? QString("unknown") : QString("%1%").arg(level); };
    auto onOff = [](bool on) { return on ? "on" : "off"; };
    const int balance = headset.balance();
    out << "Headset battery   " << battery(headset.headsetBattery()) << "\n"
        << "Spare battery     " << battery(headset.spareBattery()) << "\n"
        << "Profile           " << headset.activeProfile() << "\n"
        << "Volume            " << headset.volume() << "%\n"
        << "Balance           game " << 100 - balance * 5 << "% / chat " << balance * 5 << "%\n"
        << "EQ                " << Protocol::eqPresetName(headset.eqPreset()) << "\n"
        << "Voice Prompts     " << onOff(headset.voicePrompts()) << "\n"
        << "Mic level         " << levelName(headset.micVolume()) << "\n"
        << "Noise cancelling  " << onOff(headset.noiseCancelling()) << "\n"
        << "Sidetone          " << (headset.sidetone() ? levelName(headset.sidetoneLevel()) : QString("off")) << "\n"
        << "Bluetooth         " << onOff(headset.bluetooth()) << "\n"
        << "Keep settings     " << onOff(headset.keepSettings()) << "\n";
}

int status(const QStringList &args) {
    const bool json = args.contains("--json");
    HidTransport transport;
    Headset headset(&transport);
    if (!connect(headset, transport, json))
        return 1;
    // The battery and EQ replies follow the status within a few milliseconds.
    waitFor(headset, [&] { return headset.equalizer().valid && headset.headsetBattery() >= 0; }, 1000);
    if (json)
        out << QJsonDocument(statusJson(headset)).toJson();
    else
        printStatus(headset);
    return 0;
}

// One `halo set` target: what to send, and how to tell the headset took it.
struct Target {
    std::function<void(Headset &)> apply;
    std::function<bool(const Headset &)> matches; // empty when the headset never reports it
};

bool parseTarget(const QString &what, const QString &value, Target &target, QString &error) {
    bool isNumber = false;
    const int number = value.toInt(&isNumber);
    const bool on = value == "on", off = value == "off";
    const QStringList levels{"low", "mid", "high"};
    const int level = levels.indexOf(value) + 1;

    if (what == "volume") {
        if (!isNumber || number < 0 || number > 100)
            return error = "Volume takes a percentage from 0 to 100.", false;
        target = {[=](Headset &h) { h.setVolume(number); },
                  [=](const Headset &h) { return h.volumeRaw() == Protocol::volumePercentToRaw(number); }};
    } else if (what == "balance") {
        if (!isNumber || number < 0 || number > 20)
            return error = "Balance takes 0 (all game) to 20 (all chat); 10 is even.", false;
        target = {[=](Headset &h) { h.setBalance(number); }, [=](const Headset &h) { return h.balance() == number; }};
    } else if (what == "eq") {
        const QStringList names{"glorious", "music", "immersion", "voice", "fps", "custom1", "custom2"};
        const int preset = isNumber ? number : names.indexOf(value.toLower()) + 1;
        if (preset < 1 || preset > Protocol::EqPresets)
            return error = "EQ takes glorious, music, immersion, voice, fps, custom1, custom2, or 1-7.", false;
        target = {[=](Headset &h) { h.setEqPreset(preset); }, [=](const Headset &h) { return h.eqPreset() == preset; }};
    } else if (what == "mic") {
        if (!level)
            return error = "Mic takes low, mid, or high.", false;
        target = {[=](Headset &h) { h.setMicVolume(level); }, [=](const Headset &h) { return h.micVolume() == level; }};
    } else if (what == "sidetone") {
        if (!level && !off)
            return error = "Sidetone takes off, low, mid, or high.", false;
        target = {[=](Headset &h) {
                      h.setSidetone(!off);
                      if (level)
                          h.setSidetoneLevel(level);
                  },
                  [=](const Headset &h) { return off ? !h.sidetone() : h.sidetone() && h.sidetoneLevel() == level; }};
    } else if (what == "profile") {
        if (!isNumber || number < 1 || number > Protocol::Profiles)
            return error = "Profile takes 1 to 5.", false;
        target = {[=](Headset &h) { h.setActiveProfile(number); },
                  [=](const Headset &h) { return h.activeProfile() == number; }};
    } else if (what == "prompts" || what == "enc" || what == "bluetooth" || what == "autooff" || what == "mute") {
        if (!on && !off)
            return error = QString("%1 takes on or off.").arg(what), false;
        if (what == "prompts")
            target = {[=](Headset &h) { h.setVoicePrompts(on); }, [=](const Headset &h) { return h.voicePrompts() == on; }};
        else if (what == "enc")
            target = {[=](Headset &h) { h.setNoiseCancelling(on); },
                      [=](const Headset &h) { return h.noiseCancelling() == on; }};
        else if (what == "bluetooth")
            target = {[=](Headset &h) { h.setBluetooth(on); }, [=](const Headset &h) { return h.bluetooth() == on; }};
        else if (what == "autooff")
            target = {[=](Headset &h) { h.setAutoOff(on); }, {}};
        else
            target = {[=](Headset &h) { h.setMicMute(on); }, {}};
    } else {
        return error = "halo can set volume, prompts, balance, eq, mic, enc, sidetone, bluetooth, profile, autooff, "
                       "or mute.", false;
    }
    return true;
}

int set(const QStringList &args) {
    const bool json = args.contains("--json");
    const QStringList positional = QStringList(args).filter(QRegularExpression("^[^-]"));
    const QString what = positional.value(2), value = positional.value(3);

    auto fail = [&](const QString &error) {
        if (json)
            out << QJsonDocument(QJsonObject{{"ok", false}, {"error", error}}).toJson();
        err << error << "\n";
        return 1;
    };

    Target target;
    QString error;
    if (!parseTarget(what, value, target, error))
        return fail(error);

    HidTransport transport;
    Headset headset(&transport);
    if (!connect(headset, transport, json))
        return 1;

    // Already set: nothing changes on the headset, but applying it still
    // records the choice for the next power-on.
    if (target.matches && target.matches(headset)) {
        target.apply(headset);
        if (json)
            out << QJsonDocument(statusJson(headset)).toJson();
        return 0;
    }

    // Wait for the headset's own report after the write, so success means
    // the headset took the value, not just that halo sent it.
    bool reported = false;
    QObject::connect(&headset, &Headset::statusReported, [&] { reported = true; });
    target.apply(headset);
    reported = false;
    if (target.matches) {
        if (!waitFor(headset, [&] { return reported && target.matches(headset); }, 2000))
            return fail("The headset didn't confirm the change.");
    } else {
        // Nothing to confirm against; give the write a moment to go out.
        waitFor(headset, [] { return false; }, 300);
    }
    if (json)
        out << QJsonDocument(statusJson(headset)).toJson();
    return 0;
}

void printEqualizer(const Protocol::Equalizer &eq) {
    for (int band = 0; band < eq.frequencies.size(); ++band)
        out << QString("Band %1  %2 Hz  %3 dB  Q %4  %5\n")
                   .arg(band + 1)
                   .arg(eq.frequencies[band], 5)
                   .arg(eq.gains[band], 5, 'f', 1)
                   .arg(eq.q[band], 0, 'f', 1)
                   .arg(Protocol::eqTypeName(eq.types[band]));
}

QJsonObject equalizerJson(const Protocol::Equalizer &eq) {
    QJsonArray bands;
    for (int band = 0; band < eq.frequencies.size(); ++band)
        bands << QJsonObject{{"band", band + 1}, {"hz", eq.frequencies[band]}, {"db", eq.gains[band]},
                             {"q", eq.q[band]}, {"type", Protocol::eqTypeName(eq.types[band])}};
    return {{"ok", true}, {"bands", bands}};
}

int eq(const QStringList &args) {
    const bool json = args.contains("--json"), save = args.contains("--save");
    const QStringList positional = QStringList(args).filter(QRegularExpression("^[^-]"));
    const QString action = positional.value(2);

    auto fail = [&](const QString &error) {
        if (json)
            out << QJsonDocument(QJsonObject{{"ok", false}, {"error", error}}).toJson();
        err << error << "\n";
        return 1;
    };

    HidTransport transport;
    Headset headset(&transport);
    if (!connect(headset, transport, json))
        return 1;
    if (!waitFor(headset, [&] { return headset.equalizer().valid; }, 1500))
        return fail("The headset didn't send its EQ curve.");

    Protocol::Equalizer target = headset.equalizer();
    if (action == "band") {
        bool isNumber = false;
        const int band = positional.value(3).toInt(&isNumber) - 1;
        if (!isNumber || band < 0 || band >= Protocol::EqBands)
            return fail("eq band takes a band from 1 to 5.");
        for (const QString &setting : positional.mid(4)) {
            const QString key = setting.section('=', 0, 0), value = setting.section('=', 1);
            bool ok = false;
            const double number = value.toDouble(&ok);
            if (key == "hz" && ok && number >= Protocol::EqMinFrequency && number <= Protocol::EqMaxFrequency)
                target.frequencies[band] = int(number);
            else if (key == "db" && ok && std::abs(number) <= Protocol::EqMaxGain)
                target.gains[band] = number;
            else if (key == "q" && ok && number >= Protocol::EqMinQ && number <= Protocol::EqMaxQ)
                target.q[band] = number;
            else if (key == "type" && QStringList{"peak", "lowshelf", "highshelf", "lowpass", "highpass", "notch"}.contains(value))
                target.types[band] = QStringList{"peak", "lowshelf", "highshelf", "lowpass", "highpass", "notch"}.indexOf(value);
            else
                return fail(QString("Can't use %1. Bands take hz=20-20000, db=-12..12, q=0.1-4, and type=peak, "
                                    "lowshelf, highshelf, lowpass, highpass, or notch.").arg(setting));
        }
    } else if (action == "flat") {
        target = Protocol::flatEqualizer();
    } else if (!action.isEmpty()) {
        return fail("eq takes band or flat, or nothing to show the curve.");
    }

    if (action.isEmpty()) {
        if (json)
            out << QJsonDocument(equalizerJson(target)).toJson();
        else
            printEqualizer(target);
        return 0;
    }

    // Send, and wait for the headset to accept it. A read returns the stored
    // curve, so the read-back only confirms the curve once it's saved.
    const QByteArray wanted = Protocol::encodeEqualizer(target).left(25);
    bool accepted = false, refused = false;
    QObject::connect(&headset, &Headset::acknowledged, [&](int command, bool ok) {
        if (command == Protocol::Command::Equalizer)
            (ok ? accepted : refused) = true;
    });
    headset.setEqualizer(target, true);
    if (!waitFor(headset, [&] { return accepted || refused; }, 1500) || refused)
        return fail("The headset didn't take the curve.");
    Protocol::Equalizer reported = Protocol::parseEqualizerPayload(Protocol::encodeEqualizer(target));
    if (save) {
        Protocol::Equalizer stored;
        QObject::connect(&headset, &Headset::equalizerReported, [&](const Protocol::Equalizer &eq) { stored = eq; });
        headset.saveProfile();
        waitFor(headset, [] { return false; }, 300);
        headset.readEqualizer();
        if (!waitFor(headset, [&] { return stored.payload.left(25) == wanted; }, 1500))
            return fail("The curve plays, but the headset didn't confirm saving it.");
        reported = stored;
    } else if (!json) {
        out << "Playing on Custom 1, not saved; run halo save to keep it.\n";
    }
    if (json)
        out << QJsonDocument(equalizerJson(reported)).toJson();
    else
        printEqualizer(reported);
    return 0;
}

int save() {
    HidTransport transport;
    Headset headset(&transport);
    if (!connect(headset, transport, false))
        return 1;
    headset.saveProfile();
    // The write needs a moment to leave before the process exits.
    waitFor(headset, [] { return false; }, 300);
    out << "Saved profile " << headset.activeProfile() << " to the headset.\n";
    return 0;
}

// Where halo watch publishes the headset's state for the Omarchy bar widget:
// in the per-user runtime directory, so it's private and gone at logout.
QString statePath() {
    const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    return (runtime.isEmpty() ? QDir::tempPath() + "/halo-" + qEnvironmentVariable("USER") : runtime) + "/halo/status.json";
}

int watch() {
    HidTransport transport;
    Headset headset(&transport);
    auto stamp = [] { return QDateTime::currentDateTime().toString(Qt::ISODate); };

    // Written beside the target and renamed into place, on every change and
    // every 30 seconds, so a reader can tell a live file from a stale one.
    const QString path = statePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    auto publish = [&] {
        const bool online = headset.online();
        const QJsonObject state{
            {"state", headset.state()},
            {"battery", QJsonObject{{"headset", online ? headset.headsetBattery() : -1},
                                    {"spare", online ? headset.spareBattery() : -1}}},
            {"updated", QDateTime::currentSecsSinceEpoch()},
        };
        QSaveFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(state).toJson(QJsonDocument::Compact));
            file.commit();
        }
    };
    QTimer debounce, heartbeat;
    debounce.setSingleShot(true);
    debounce.setInterval(150);
    QObject::connect(&debounce, &QTimer::timeout, publish);
    QObject::connect(&headset, &Headset::stateChanged, &debounce, qOverload<>(&QTimer::start));
    QObject::connect(&headset, &Headset::statusChanged, &debounce, qOverload<>(&QTimer::start));
    heartbeat.setInterval(30000);
    QObject::connect(&heartbeat, &QTimer::timeout, publish);
    heartbeat.start();
    publish();

    QObject::connect(&headset, &Headset::stateChanged, [&] {
        out << stamp() << " headset " << headset.state() << Qt::endl;
    });
    QObject::connect(&headset, &Headset::restored, [&](const QString &what) {
        out << stamp() << " restored " << what << Qt::endl;
    });
    out << stamp() << " watching; headset " << headset.state() << Qt::endl;
    const int code = QEventLoop().exec();
    QFile::remove(path);
    return code;
}

int skillInstall() {
    QFile source(":/skill/SKILL.md");
    if (!source.open(QIODevice::ReadOnly)) {
        err << "The bundled skill is missing from this build.\n";
        return 1;
    }
    const QString path = QDir::homePath() + "/.claude/skills/halo/SKILL.md";
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile target(path);
    if (!target.open(QIODevice::WriteOnly) || target.write(source.readAll()) < 0 || !target.commit()) {
        err << "Couldn't write " << path << "\n";
        return 1;
    }
    out << "Installed " << path << "\n";
    return 0;
}

} // namespace

namespace Cli {

bool handles(const QStringList &args) {
    const QString command = args.value(1);
    return command != "open";
}

int run(const QStringList &args) {
    const QString command = args.value(1);
    if (command == "status")
        return status(args);
    if (command == "set")
        return set(args);
    if (command == "watch")
        return watch();
    if (command == "eq")
        return eq(args);
    if (command == "save")
        return save();
    if (command == "skill" && args.value(2) == "install")
        return skillInstall();
    if (command.isEmpty() || command == "help" || command == "--help" || command == "-h") {
        out << Usage;
        return 0;
    }
    err << "Unknown command: " << command << "\n\n" << Usage;
    return 1;
}

} // namespace Cli
