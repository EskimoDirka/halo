#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "headset.h"
#include "hidtransport.h"
#include "protocol.h"
#include "settings.h"
#include "theme.h"
#include "translator.h"

using namespace Protocol;

// Stands in for the receiver: records what halo writes and lets a test push
// reports as if the headset had sent them.
class FakeTransport : public Transport {
public:
    State current = Open;
    QList<QByteArray> writes;

    State state() const override { return current; }
    bool write(const QByteArray &report) override { writes << report; return true; }
    void push(const QByteArray &report) { emit received(report); }

    // Writes that change something (op 02), as "command value" pairs.
    QList<QPair<int, int>> changes() const {
        QList<QPair<int, int>> list;
        for (const auto &w : writes)
            if (quint8(w[2]) == 0x02)
                list << qMakePair(int(quint8(w[1])), int(quint8(w[4])));
        return list;
    }
};

// One profile's settings in status order: balance, prompts, EQ preset, mic
// level, ENC, sidetone, sidetone level.
static ProfileRecord profileRecord(bool prompts, int balance = 0x0A, int eq = 1) {
    return {balance, prompts ? 1 : 0, eq, 3, 1, 0, 2};
}

// A status report laid out like the headset's (synthetic values).
static QByteArray statusReport(int volumeRaw, bool prompts, int profile = 1, int headset = 50, int spare = 92,
                               int balance = 0x0A) {
    std::array<ProfileRecord, Profiles> profiles;
    for (int n = 1; n <= Profiles; ++n)
        profiles[size_t(n - 1)] = profileRecord(n == profile ? prompts : true, n == profile ? balance : 0x0A, n);
    QByteArray p;
    p += char(volumeRaw);
    p += QByteArray::fromHex("01000101");
    p += char(headset);
    p += char(spare);
    p += char(0x01);
    for (const auto &record : profiles) {
        p += char(0x01);
        for (int value : record)
            p += char(value);
    }
    p += char(profile);
    p += QByteArray::fromHex("0000");
    QByteArray report(ReportSize, '\0');
    report[0] = char(ReportId);
    report[1] = char(Command::Status);
    report[2] = 0x01;
    report[3] = char(p.size());
    report.replace(4, p.size(), p);
    return report;
}

static QByteArray reply(quint8 command, const QByteArray &payloadHex) {
    const QByteArray p = QByteArray::fromHex(payloadHex);
    QByteArray report(ReportSize, '\0');
    report[0] = char(ReportId);
    report[1] = char(command);
    report[2] = 0x01;
    report[3] = char(p.size());
    report.replace(4, p.size(), p);
    return report;
}

class HaloTests : public QObject {
    Q_OBJECT

private slots:
    void requestsAreFramed()
    {
        const QByteArray read = readRequest(Command::Status);
        QCOMPARE(read.size(), 64);
        QCOMPARE(read.left(4), QByteArray::fromHex("15250100"));
        QCOMPARE(writeRequest(Command::VoicePrompts, 0).left(5), QByteArray::fromHex("1503020100"));
        QCOMPARE(writeRequest(Command::Volume, 0x22).left(5), QByteArray::fromHex("1501020122"));
    }

    void volumeConverts()
    {
        QCOMPARE(volumeRawToPercent(0x19), 50);
        QCOMPARE(volumeRawToPercent(0x25), 74);
        QCOMPARE(volumePercentToRaw(68), 0x22);
        QCOMPARE(volumePercentToRaw(100), 50);
        QCOMPARE(volumePercentToRaw(-5), 0);
    }

    void statusParses()
    {
        const Status status = parseStatus(statusReport(0x22, false, 2, 80, 95, 0x13));
        QVERIFY(status.valid);
        QCOMPARE(status.volumeRaw, 0x22);
        QCOMPARE(status.activeProfile, 2);
        QCOMPARE(status.value(VoicePrompts), 0);
        QCOMPARE(status.value(Balance), 0x13);
        QCOMPARE(status.value(EqPreset), 2);
        QCOMPARE(status.value(MicVolume), 3);
        QCOMPARE(status.headsetBattery, 80);
        QCOMPARE(status.spareBattery, 95);
        QVERIFY(status.bluetooth);
        QVERIFY(!parseStatus(reply(Command::Status, "")).valid);
        QVERIFY(!parseStatus(reply(Command::Battery, "325C01")).valid);
    }

    // The real status block CORE logged, profile 1 active, Voice Prompts off.
    void parsesTheCapturedStatus()
    {
        const Status status = parseStatus(reply(Command::Status,
            "1101000101325C01" "010A000103010002" "010A010203010002" "010A010303010002"
            "010A010403010102" "010A010503010002" "010000"));
        QVERIFY(status.valid);
        QCOMPARE(status.volumeRaw, 0x11);
        QCOMPARE(status.activeProfile, 1);
        QCOMPARE(status.value(VoicePrompts), 0);
        QCOMPARE(status.profiles[3][Sidetone], 1);
        QCOMPARE(status.profiles[1][VoicePrompts], 1);
    }

    void batteryAndEqualizerParse()
    {
        const Battery battery = parseBattery(reply(Command::Battery, "325C01"));
        QVERIFY(battery.valid);
        QCOMPARE(battery.headset, 50);
        QCOMPARE(battery.spare, 92);

        // The Valorant curve CORE wrote into Custom slot 1.
        const Equalizer eq = parseEqualizer(reply(Command::Equalizer,
            "0043010B0CDD17DF1F3C" "886B784340" "280C21281E" "0100000200"));
        QVERIFY(eq.valid);
        QCOMPARE(eq.frequencies, (QList<int>{67, 267, 3293, 6111, 7996}));
        QCOMPARE(eq.gains, (QList<double>{-12.0, 10.7, 12.0, 6.7, 6.4}));
        QCOMPARE(eq.q, (QList<double>{4.0, 1.2, 3.3, 4.0, 3.0}));
        QCOMPARE(eq.types, (QList<int>{1, 0, 0, 2, 0}));

        QCOMPARE(parseSerial(reply(Command::Serial, QByteArray("TEST0001").toHex())), QString("TEST0001"));
    }

    void settingsRoundTrip()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("halo/settings.json");
        Settings settings;
        settings.restoreVolume = false;
        settings.volumeRaw = 30;
        settings.setVoicePromptsFor(3, false);
        QVERIFY(settings.save(path));

        const Settings loaded = Settings::load(path);
        QCOMPARE(loaded.restoreVolume, false);
        QCOMPARE(loaded.volumeRaw, 30);
        QCOMPARE(loaded.voicePromptsFor(3), 0);
        QCOMPARE(loaded.voicePromptsFor(1), -1);
    }

    // The headset came on while halo wasn't looking: defaults on first sight.
    void restoresOnFirstSight()
    {
        QTemporaryDir dir;
        FakeTransport transport;
        Headset headset(&transport, wanting(dir, 0x22, false));
        QSignalSpy restored(&headset, &Headset::restored);
        transport.push(statusReport(PowerOnVolumeRaw, true));
        QCOMPARE(restored.size(), 1);
        QCOMPARE(transport.changes(), (QList<QPair<int, int>>{{Command::VoicePrompts, 0}, {Command::Volume, 0x22}}));
    }

    // A power cycle mid-session: volume jumps to 50% and prompts come back on.
    void restoresAfterPowerCycle()
    {
        QTemporaryDir dir;
        FakeTransport transport;
        Headset headset(&transport, wanting(dir, 0x22, false));
        transport.push(statusReport(0x22, false));
        QVERIFY(transport.changes().isEmpty());
        transport.push(statusReport(PowerOnVolumeRaw, true));
        QCOMPARE(transport.changes(), (QList<QPair<int, int>>{{Command::VoicePrompts, 0}, {Command::Volume, 0x22}}));
    }

    // Turning the wheel one step at a time through 50% is not a reset.
    void wheelStepsAreKept()
    {
        QTemporaryDir dir;
        FakeTransport transport;
        Headset headset(&transport, wanting(dir, 0x1A, false));
        transport.push(statusReport(0x1A, false));
        transport.push(statusReport(0x19, false));
        transport.push(statusReport(0x18, false));
        QVERIFY(transport.changes().isEmpty());
        QCOMPARE(Settings::load(dir.filePath("settings.json")).volumeRaw, 0x18);
    }

    // Turning the balance used to make the status look invalid.
    void balanceChangesKeepTheHeadsetOnline()
    {
        QTemporaryDir dir;
        FakeTransport transport;
        Headset headset(&transport, dir.filePath("settings.json"));
        transport.push(statusReport(0x22, false, 1, 50, 92, 0x13));
        QCOMPARE(headset.state(), QString("online"));
        QCOMPARE(headset.balance(), 0x13);
    }

    // A change made on the headset or in another app is adopted, not undone.
    void headsetChangesAreAdopted()
    {
        QTemporaryDir dir;
        FakeTransport transport;
        Headset headset(&transport, wanting(dir, 0x22, false));
        transport.push(statusReport(0x22, false));
        transport.push(statusReport(0x22, false, 1, 50, 92, 0x05));
        QVERIFY(transport.changes().isEmpty());
        QCOMPARE(Settings::load(dir.filePath("settings.json")).value(1, Balance), 0x05);
    }

    // After a power cycle, every setting halo knows for the profile comes back.
    void restoresEveryFieldAfterPowerCycle()
    {
        QTemporaryDir dir;
        FakeTransport transport;
        Headset headset(&transport, wanting(dir, 0x22, false));
        transport.push(statusReport(0x22, false, 1, 50, 92, 0x05));
        transport.push(statusReport(PowerOnVolumeRaw, true));
        QCOMPARE(transport.changes(), (QList<QPair<int, int>>{
            {Command::Balance, 0x05}, {Command::VoicePrompts, 0}, {Command::Volume, 0x22}}));
    }

    // Switching profile puts back that profile's remembered settings.
    void profileSwitchRestoresThatProfile()
    {
        QTemporaryDir dir;
        FakeTransport transport;
        Headset headset(&transport, wanting(dir, 0x22, false));
        transport.push(statusReport(0x22, false, 1));
        QTest::qWait(3100); // past the restore cooldown
        transport.push(statusReport(0x22, true, 2));
        QCOMPARE(transport.changes(), (QList<QPair<int, int>>{{Command::VoicePrompts, 0}}));
    }

    void settingsAreSent()
    {
        QTemporaryDir dir;
        FakeTransport transport;
        Headset headset(&transport, dir.filePath("settings.json"));
        transport.push(statusReport(0x22, false));
        headset.setSidetone(true);
        headset.setSidetoneLevel(3);
        headset.setMicVolume(1);
        headset.setEqPreset(5);
        headset.setActiveProfile(3);
        QCOMPARE(transport.changes(), (QList<QPair<int, int>>{
            {Command::Sidetone, 1}, {Command::SidetoneLevel, 3}, {Command::MicVolume, 1},
            {Command::EqPreset, 5}, {Command::ActiveProfile, 3}}));
        QCOMPARE(Settings::load(dir.filePath("settings.json")).value(1, SidetoneLevel), 3);
    }

    void keepSettingsOffLeavesTheHeadsetAlone()
    {
        QTemporaryDir dir;
        FakeTransport transport;
        Headset headset(&transport, wanting(dir, 0x22, false));
        headset.setKeepSettings(false);
        transport.push(statusReport(PowerOnVolumeRaw, true));
        QVERIFY(transport.changes().isEmpty());
    }

    void restoreVolumeOffOnlyFixesPrompts()
    {
        QTemporaryDir dir;
        FakeTransport transport;
        Headset headset(&transport, wanting(dir, 0x22, false));
        headset.setRestoreVolume(false);
        transport.push(statusReport(PowerOnVolumeRaw, true));
        QCOMPARE(transport.changes(), (QList<QPair<int, int>>{{Command::VoicePrompts, 0}}));
    }

    // Voice Prompts are remembered per profile.
    void promptsFollowTheActiveProfile()
    {
        QTemporaryDir dir;
        FakeTransport transport;
        Headset headset(&transport, dir.filePath("settings.json"));
        transport.push(statusReport(0x22, true, 2));
        headset.setVoicePrompts(false);
        const Settings saved = Settings::load(dir.filePath("settings.json"));
        QCOMPARE(saved.voicePromptsFor(2), 0);
        QCOMPARE(saved.voicePromptsFor(1), -1);
    }

    void undoPutsTheVolumeBack()
    {
        QTemporaryDir dir;
        FakeTransport transport;
        Headset headset(&transport, dir.filePath("settings.json"));
        transport.push(statusReport(0x22, false));
        headset.setVolume(80);
        QCOMPARE(headset.volume(), 80);
        QVERIFY(headset.canUndo());
        QTest::qWait(80); // let the slider throttle pass
        headset.undo();
        QCOMPARE(headset.volumeRaw(), 0x22);
        QVERIFY(headset.canRedo());
    }

    void noHeadsetMeansOff()
    {
        QTemporaryDir dir;
        FakeTransport transport;
        Headset headset(&transport, dir.filePath("settings.json"));
        QCOMPARE(headset.state(), QString("off"));
        transport.push(statusReport(0x22, false));
        QCOMPARE(headset.state(), QString("online"));
        transport.current = Transport::Missing;
        emit transport.stateChanged();
        QCOMPARE(headset.state(), QString("missing"));
    }

    void findsTheReceiverInSysfs()
    {
        QTemporaryDir dir;
        auto node = [&](const QString &name, const QByteArray &id) {
            QDir(dir.path()).mkpath(name + "/device");
            write(dir.filePath(name + "/device/uevent"), "DRIVER=hid-generic\nHID_ID=" + id + "\nHID_NAME=x\n");
        };
        node("hidraw0", "0003:00001038:0000183A");
        QVERIFY(HidTransport::findReceiver(dir.path()).isEmpty());
        node("hidraw5", "0003:00003794:0000D005");
        QCOMPARE(HidTransport::findReceiver(dir.path()), QString("hidraw5"));
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        QFile colors(dir.path() + "/theme/colors.toml");
        QVERIFY(colors.open(QIODevice::WriteOnly));
        colors.write("accent = \"#112233\"\n");
        colors.close();

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        auto write = [](const QString &path, const QByteArray &data) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
        };
        QVERIFY(write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n"));
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(write(response, "{\"int\":0}"));

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        QVERIFY(write(response, "{\"int\":6}"));
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorPicksPluralForms()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "ru\n");
        write(dir.path() + "/en.json", R"({"%n clips": {"one": "%n clip", "other": "%n clips"}})");
        write(dir.path() + "/ru.json", R"({"%n clips": {"one": "%n клип", "few": "%n клипа", "many": "%n клипов", "other": "%n клипа"}})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 21), QString("%n клип"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 22), QString("%n клипа"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 25), QString("%n клипов"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
    }

private:
    static QString wanting(QTemporaryDir &dir, int volumeRaw, bool prompts)
    {
        Settings settings;
        settings.volumeRaw = volumeRaw;
        for (int profile = 1; profile <= Settings::Profiles; ++profile)
            settings.setVoicePromptsFor(profile, prompts);
        const QString path = dir.filePath("settings.json");
        settings.save(path);
        return path;
    }
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(HaloTests)
#include "halo_tests.moc"
