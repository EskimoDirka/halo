#include "protocol.h"

#include <QtGlobal>
#include <cmath>

namespace Protocol {

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
    // Volume, a flag, two constants, then the two battery levels, the active
    // profile, and one 8-byte block per profile. Anything shorter carries no
    // headset state, which is treated as the headset being off or away.
    if (p.size() < 16)
        return status;
    status.volumeRaw = quint8(p[0]);
    status.headsetBattery = quint8(p[5]);
    status.spareBattery = quint8(p[6]);
    status.activeProfile = qBound(1, int(quint8(p[7])), 5);
    status.payload = p;

    // Each profile block reads 01 0A <voice prompts> <profile> 03 01 <?> 02.
    const int block = 8 + (status.activeProfile - 1) * 8;
    if (block + 2 >= p.size() || quint8(p[block]) != 0x01 || quint8(p[block + 1]) != 0x0A)
        return status;
    status.voicePrompts = quint8(p[block + 2]) != 0;
    status.valid = status.volumeRaw <= MaxVolumeRaw;
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
    // A 25-byte bank: five big-endian band frequencies, five bytes that are
    // all zero on a flat EQ (likely the gains, not yet confirmed), five Q
    // values in tenths, and five more zero bytes. The payload holds two banks.
    if (p.size() < 20)
        return eq;
    for (int band = 0; band < 5; ++band) {
        eq.frequencies << (quint8(p[band * 2]) << 8 | quint8(p[band * 2 + 1]));
        eq.q << quint8(p[15 + band]) / 10.0;
    }
    eq.payload = p;
    eq.valid = eq.frequencies.first() > 0;
    return eq;
}

QString parseSerial(const QByteArray &report) {
    return QString::fromLatin1(payload(report, Command::Serial)).trimmed().remove(QChar('\0'));
}

} // namespace Protocol
