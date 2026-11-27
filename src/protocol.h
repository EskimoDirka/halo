#pragma once

// The GHS Wireless InfinitePlay receiver's settings protocol. Everything goes
// through HID report 0x15 (63 bytes after the report id) on the receiver's
// vendor interface:
//
//   request   15 <command> 01 00                 read
//             15 <command> 02 <length> <data…>   write
//   reply     15 <command> 01 <length> <data…>
//
// The headset also sends a Status reply on its own when its state changes
// and every 30 seconds or so. docs/protocol.md records how each field was
// worked out, from Glorious CORE's own logs.

#include <QByteArray>
#include <QList>
#include <QString>
#include <array>

namespace Protocol {

constexpr quint16 VendorId = 0x3794;
// CORE's device list says d003; the receiver enumerates as d005 (CORE's own
// hot-plug log agrees). Both are accepted.
constexpr quint16 ProductIds[] = {0xd003, 0xd005};
constexpr quint8 ReportId = 0x15;
constexpr int ReportSize = 64; // report id plus 63 bytes
constexpr int Profiles = 5;

namespace Command {
enum : quint8 {
    Volume = 0x01,          // raw 0..50
    Balance = 0x02,         // game/chat, 0..20 (10 is even)
    VoicePrompts = 0x03,    // 0 off, 1 on
    EqPreset = 0x04,        // 1..7, see eqPresetName()
    Equalizer = 0x05,       // read, or write a whole custom curve
    MicMute = 0x08,         // 0 live, 1 muted
    MicVolume = 0x09,       // 1 low, 2 mid, 3 high
    NoiseCancelling = 0x10, // ENC: 0 off, 1 on
    Sidetone = 0x11,        // 0 off, 1 on
    SidetoneLevel = 0x12,   // 1 low, 2 mid, 3 high
    Serial = 0x13,          // read: ASCII
    AutoOff = 0x14,         // power off after 30 minutes idle: 0 off, 1 on
    Bluetooth = 0x15,       // 0 off, 1 on
    Battery = 0x17,         // read: headset and spare pack charge
    SaveProfile = 0x18,     // write with no data: store the profile in flash
    ActiveProfile = 0x20,   // 1..5
    Status = 0x25,          // read, and pushed on changes
};
} // namespace Command

// Settings the headset keeps per profile, in the order of the status block's
// 8-byte profile record (after its first, not yet understood, byte).
enum Field { Balance, VoicePrompts, EqPreset, MicVolume, NoiseCancelling, Sidetone, SidetoneLevel, FieldCount };

struct FieldInfo {
    const char *key; // settings.json and CLI name
    quint8 command;
    int offset;      // within the profile record
    int min, max;
};
const FieldInfo &fieldInfo(int field);
int fieldByKey(const QString &key); // -1 when unknown

QString eqPresetName(int preset);
constexpr int EqPresets = 7;

// The headset restores this raw volume (50%) every time it powers on.
constexpr int PowerOnVolumeRaw = 0x19;
constexpr int MaxVolumeRaw = 50;

QByteArray readRequest(quint8 command);
QByteArray writeRequest(quint8 command, quint8 value);
QByteArray writeRequest(quint8 command, const QByteArray &data);

// The command a reply answers, or -1 when it isn't a settings reply.
int replyCommand(const QByteArray &report);

int volumeRawToPercent(int raw);
int volumePercentToRaw(int percent);

using ProfileRecord = std::array<int, FieldCount>;

struct Status {
    bool valid = false;
    int volumeRaw = 0;
    bool bluetooth = true;
    int headsetBattery = -1; // percent
    int spareBattery = -1;   // percent, the pack charging in the base
    int activeProfile = 1;
    std::array<ProfileRecord, Profiles> profiles{};
    QByteArray payload;

    const ProfileRecord &active() const { return profiles[size_t(activeProfile - 1)]; }
    int value(int field) const { return active()[size_t(field)]; }
};
Status parseStatus(const QByteArray &report);

struct Battery {
    bool valid = false;
    int headset = -1;
    int spare = -1;
    int flags = 0; // not yet understood; kept for --json
};
Battery parseBattery(const QByteArray &report);

// The custom EQ curve: five bands, each a filter.
struct Equalizer {
    bool valid = false;
    QList<int> frequencies; // Hz
    QList<double> gains;    // dB
    QList<double> q;
    QList<int> types;       // 0 peak, 1 low shelf, 2 high shelf, 3 low pass, 4 high pass, 5 notch(?)
    QByteArray payload;
};
Equalizer parseEqualizer(const QByteArray &report);

QString parseSerial(const QByteArray &report);

} // namespace Protocol
