#include "protocol.h"

#include <QtGlobal>
#include <cmath>

namespace Protocol {

static const FieldInfo Fields[FieldCount] = {
    {"balance", Command::Balance, 1, 0, 20},
    {"voicePrompts", Command::VoicePrompts, 2, 0, 1},
    {"eqPreset", Command::EqPreset, 3, 1, EqPresets},
    {"micVolume", Command::MicVolume, 4, 1, 3},
    {"noiseCancelling", Command::NoiseCancelling, 5, 0, 1},
    {"sidetone", Command::Sidetone, 6, 0, 1},
    {"sidetoneLevel", Command::SidetoneLevel, 7, 1, 3},
};

const FieldInfo &fieldInfo(int field) { return Fields[qBound(0, field, FieldCount - 1)]; }

int fieldByKey(const QString &key) {
    for (int f = 0; f < FieldCount; ++f)
        if (key == QLatin1String(Fields[f].key))
            return f;
    return -1;
}

QString eqPresetName(int preset) {
    switch (preset) {
    case 1: return "Glorious Mode";
    case 2: return "Music";
    case 3: return "Immersion";
    case 4: return "Voice Call";
    case 5: return "FPS";
    case 6: return "Custom 1";
    case 7: return "Custom 2";
    }
    return {};
}

static QByteArray frame(quint8 command, quint8 operation, const QByteArray &data) {
    QByteArray report(ReportSize, '\0');
    report[0] = char(ReportId);
    report[1] = char(command);
    report[2] = char(operation);
    report[3] = char(data.size());
    for (int i = 0; i < data.size() && 4 + i < ReportSize; ++i)
        report[4 + i] = data[i];
    return report;
}

QByteArray readRequest(quint8 command) { return frame(command, 0x01, {}); }

QByteArray writeRequest(quint8 command, quint8 value) { return frame(command, 0x02, QByteArray(1, char(value))); }

QByteArray writeRequest(quint8 command, const QByteArray &data) { return frame(command, 0x02, data); }

int replyCommand(const QByteArray &report) {
    if (report.size() < 4 || quint8(report[0]) != ReportId)
        return -1;
    return quint8(report[1]);
}

// The payload of a reply, bounded by the length byte and the report itself.
static QByteArray payload(const QByteArray &report, quint8 command) {
    if (replyCommand(report) != command)
        return {};
    const int length = quint8(report[3]);
    return report.mid(4, qMin(length, int(report.size()) - 4));
}

int volumeRawToPercent(int raw) { return qBound(0, raw, MaxVolumeRaw) * 100 / MaxVolumeRaw; }

int volumePercentToRaw(int percent) {
    return qBound(0, int(std::lround(qBound(0, percent, 100) * MaxVolumeRaw / 100.0)), MaxVolumeRaw);
}

Status parseStatus(const QByteArray &report) {
    Status status;
    const QByteArray p = payload(report, Command::Status);
    // An 8-byte header (volume, three bytes not yet understood, Bluetooth,
    // the two battery levels, one more unknown), five 8-byte profile records,
    // then the active profile. Anything shorter carries no headset state,
    // which is treated as the headset being off or away.
    constexpr int Header = 8, Record = 8, Active = Header + Profiles * Record;
    if (p.size() <= Active)
        return status;
    status.volumeRaw = quint8(p[0]);
    status.bluetooth = quint8(p[4]) != 0;
    status.headsetBattery = quint8(p[5]);
    status.spareBattery = quint8(p[6]);
    status.activeProfile = quint8(p[Active]);
    for (int profile = 0; profile < Profiles; ++profile)
        for (int field = 0; field < FieldCount; ++field)
            status.profiles[size_t(profile)][size_t(field)] =
                quint8(p[Header + profile * Record + Fields[field].offset]);
    status.payload = p;
    status.valid = status.volumeRaw <= MaxVolumeRaw && status.activeProfile >= 1 && status.activeProfile <= Profiles;
    return status;
}

Battery parseBattery(const QByteArray &report) {
    Battery battery;
    const QByteArray p = payload(report, Command::Battery);
    if (p.size() < 2)
        return battery;
    battery.headset = quint8(p[0]);
    battery.spare = quint8(p[1]);
    battery.flags = p.size() > 2 ? quint8(p[2]) : 0;
    battery.valid = battery.headset <= 100 && battery.spare <= 100;
    return battery;
}

Equalizer parseEqualizer(const QByteArray &report) {
    Equalizer eq;
    const QByteArray p = payload(report, Command::Equalizer);
    // Two 25-byte blocks. The first is the custom curve: five big-endian
    // frequencies, five signed gains in tenths of a dB, five Q values in
    // tenths, five filter types. CORE always writes the second block as a
    // flat default; what it controls is unknown.
    if (p.size() < 25)
        return eq;
    for (int band = 0; band < 5; ++band) {
        eq.frequencies << (quint8(p[band * 2]) << 8 | quint8(p[band * 2 + 1]));
        eq.gains << qint8(p[10 + band]) / 10.0;
        eq.q << quint8(p[15 + band]) / 10.0;
        eq.types << quint8(p[20 + band]);
    }
    eq.payload = p;
    eq.valid = eq.frequencies.first() > 0;
    return eq;
}

QString parseSerial(const QByteArray &report) {
    return QString::fromLatin1(payload(report, Command::Serial)).trimmed().remove(QChar('\0'));
}

} // namespace Protocol
