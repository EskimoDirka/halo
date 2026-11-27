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
#include <functional>

#include "headset.h"
#include "hidtransport.h"

namespace {

QTextStream out(stdout), err(stderr);

const char *Usage = R"(halo: settings for the Glorious GHS Wireless InfinitePlay headset.

  halo open                    open the window
  halo status [--json]         show battery, volume, and Voice Prompts
  halo set volume <0-100>      set the headset volume
  halo set prompts <on|off>    turn Voice Prompts on or off
  halo watch                   put settings back after each power-on
  halo skill install           install the agent skill

set and status take --json. Exit status is 0 on success, 1 on error.
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
    QJsonArray frequencies, q;
    for (int f : headset.equalizer().frequencies)
        frequencies << f;
    for (double value : headset.equalizer().q)
        q << value;
    return {
        {"ok", true},
        {"state", headset.state()},
        {"volume", headset.volume()},
        {"volumeRaw", headset.volumeRaw()},
        {"voicePrompts", headset.voicePrompts()},
        {"activeProfile", headset.activeProfile()},
        {"battery", QJsonObject{{"headset", headset.headsetBattery()}, {"spare", headset.spareBattery()}}},
        {"equalizer", QJsonObject{{"frequencies", frequencies}, {"q", q}}},
        {"keepSettings", headset.keepSettings()},
        {"restoreVolume", headset.restoreVolume()},
        // The raw replies, for working out the fields not yet understood.
        {"raw", QJsonObject{{"status", QString(headset.status().payload.toHex(' '))},
                            {"equalizer", QString(headset.equalizer().payload.toHex(' '))}}},
    };
}

void printStatus(const Headset &headset) {
    auto battery = [](int level) { return level < 0 ? QString("unknown") : QString("%1%").arg(level); };
    out << "Headset battery   " << battery(headset.headsetBattery()) << "\n"
        << "Spare battery     " << battery(headset.spareBattery()) << "\n"
        << "Volume            " << headset.volume() << "%\n"
        << "Voice Prompts     " << (headset.voicePrompts() ? "on" : "off") << "\n"
        << "Profile           " << headset.activeProfile() << "\n"
        << "Keep settings     " << (headset.keepSettings() ? "on" : "off") << "\n";
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

    bool isNumber = false;
    const int percent = value.toInt(&isNumber);
    if (what == "volume" && (!isNumber || percent < 0 || percent > 100))
        return fail("Volume takes a percentage from 0 to 100.");
    if (what == "prompts" && value != "on" && value != "off")
        return fail("Voice Prompts take on or off.");
    if (what != "volume" && what != "prompts")
        return fail("halo can set volume or prompts.");

    HidTransport transport;
    Headset headset(&transport);
    if (!connect(headset, transport, json))
        return 1;

    auto matches = [&] {
        return what == "volume" ? headset.volumeRaw() == Protocol::volumePercentToRaw(percent)
                                : headset.voicePrompts() == (value == "on");
    };
    // Nothing to send when the headset already has the value, but the choice
    // is still remembered for the next power-on.
    if (matches()) {
        if (what == "prompts")
            headset.setVoicePrompts(value == "on");
        if (json)
            out << QJsonDocument(statusJson(headset)).toJson();
        return 0;
    }

    // Wait for the headset's own report after the write, so success means
    // the headset took the value, not just that halo sent it.
    bool reported = false;
    QObject::connect(&headset, &Headset::statusReported, [&] { reported = true; });
    if (what == "volume")
        headset.setVolume(percent);
    else
        headset.setVoicePrompts(value == "on");
    reported = false;
    const bool took = waitFor(headset, [&] { return reported && matches(); }, 2000);
    if (!took)
        return fail("The headset didn't confirm the change.");
    if (json)
        out << QJsonDocument(statusJson(headset)).toJson();
    return 0;
}

int watch() {
    HidTransport transport;
    Headset headset(&transport);
    auto stamp = [] { return QDateTime::currentDateTime().toString(Qt::ISODate); };
    QObject::connect(&headset, &Headset::stateChanged, [&] {
        out << stamp() << " headset " << headset.state() << Qt::endl;
    });
    QObject::connect(&headset, &Headset::restored, [&](const QString &what) {
        out << stamp() << " restored " << what << Qt::endl;
    });
    out << stamp() << " watching; headset " << headset.state() << Qt::endl;
    return QEventLoop().exec();
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
