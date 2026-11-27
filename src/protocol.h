#pragma once

// The GHS Wireless InfinitePlay receiver's settings protocol, as far as it is
// known. Everything goes through HID report 0x15 (63 bytes after the report
// id) on the receiver's vendor interface:
//
//   request   15 <command> 01 00                 read
//             15 <command> 02 <length> <data…>   write
//   reply     15 <command> 01 <length> <data…>
//
// The headset also sends a Status reply on its own whenever its state
// changes, such as the volume wheel turning or the headset powering on.
// docs/protocol.md records how each field was worked out.

#include <QByteArray>
#include <QList>
#include <QString>

namespace Protocol {

constexpr quint16 VendorId = 0x3794;
// CORE lists the receiver as d003; the one tested here enumerates as d005.
constexpr quint16 ProductIds[] = {0xd003, 0xd005};
constexpr quint8 ReportId = 0x15;
constexpr int ReportSize = 64; // report id plus 63 bytes

namespace Command {
enum : quint8 {
    Volume = 0x01,       // write: raw volume 0..50
    VoicePrompts = 0x03, // write: 0 off, 1 on
    Equalizer = 0x05,    // read: band layout
    Serial = 0x13,       // read: serial number as ASCII
    Battery = 0x17,      // read: headset and spare pack charge
    Status = 0x25,       // read, and pushed on every change
};
} // namespace Command

// The headset restores this raw volume (50%) every time it powers on.
constexpr int PowerOnVolumeRaw = 0x19;
constexpr int MaxVolumeRaw = 50;

QByteArray readRequest(quint8 command);
QByteArray writeRequest(quint8 command, quint8 value);

// The command a reply answers, or -1 when it isn't a settings reply.
int replyCommand(const QByteArray &report);

int volumeRawToPercent(int raw);
int volumePercentToRaw(int percent);

struct Status {
    bool valid = false;
    int volumeRaw = 0;
    int activeProfile = 1;
    bool voicePrompts = true;
    int headsetBattery = -1; // percent
    int spareBattery = -1;   // percent, the pack charging in the base
    QByteArray payload;
};
Status parseStatus(const QByteArray &report);

struct Battery {
    bool valid = false;
    int headset = -1;
    int spare = -1;
    int flags = 0; // not yet understood; kept for --json
};
Battery parseBattery(const QByteArray &report);

struct Equalizer {
    bool valid = false;
    QList<int> frequencies; // Hz
    QList<double> q;
    QByteArray payload;
};
Equalizer parseEqualizer(const QByteArray &report);

QString parseSerial(const QByteArray &report);

} // namespace Protocol
