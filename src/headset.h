#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QObject>
#include <QTimer>
#include <QVariantList>

#include "protocol.h"
#include "settings.h"
#include "transport.h"

// The headset's state, its settings, and the one thing CORE never did: put
// each profile's settings back after the headset resets itself on power-on.
// Exposed to QML as `headset`.
class Headset : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool online READ online NOTIFY stateChanged)
    Q_PROPERTY(int volume READ volume NOTIFY statusChanged)
    Q_PROPERTY(int headsetBattery READ headsetBattery NOTIFY statusChanged)
    Q_PROPERTY(int spareBattery READ spareBattery NOTIFY statusChanged)
    Q_PROPERTY(bool bluetooth READ bluetooth NOTIFY statusChanged)
    Q_PROPERTY(int activeProfile READ activeProfile NOTIFY statusChanged)
    Q_PROPERTY(bool voicePrompts READ voicePrompts NOTIFY statusChanged)
    Q_PROPERTY(int balance READ balance NOTIFY statusChanged)
    Q_PROPERTY(int eqPreset READ eqPreset NOTIFY statusChanged)
    Q_PROPERTY(int micVolume READ micVolume NOTIFY statusChanged)
    Q_PROPERTY(bool noiseCancelling READ noiseCancelling NOTIFY statusChanged)
    Q_PROPERTY(bool sidetone READ sidetone NOTIFY statusChanged)
    Q_PROPERTY(int sidetoneLevel READ sidetoneLevel NOTIFY statusChanged)
    Q_PROPERTY(QVariantList eqFrequencies READ eqFrequencies NOTIFY equalizerChanged)
    Q_PROPERTY(QVariantList eqGains READ eqGains NOTIFY equalizerChanged)
    Q_PROPERTY(QVariantList eqQ READ eqQ NOTIFY equalizerChanged)
    Q_PROPERTY(QVariantList eqTypes READ eqTypes NOTIFY equalizerChanged)
    Q_PROPERTY(QVariantList eqResponse READ eqResponse NOTIFY equalizerChanged)
    Q_PROPERTY(bool eqDirty READ eqDirty NOTIFY equalizerChanged)
    Q_PROPERTY(bool keepSettings READ keepSettings WRITE setKeepSettings NOTIFY settingsChanged)
    Q_PROPERTY(bool restoreVolume READ restoreVolume WRITE setRestoreVolume NOTIFY settingsChanged)
    Q_PROPERTY(QDateTime lastRestored READ lastRestored NOTIFY restored)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)

public:
    explicit Headset(Transport *transport, const QString &settingsPath = Settings::defaultPath(),
                     QObject *parent = nullptr);

    // missing: no receiver. permission: receiver found, but not ours to open.
    // off: receiver answers, headset doesn't. online: all good.
    QString state() const;
    bool online() const { return m_online; }
    int volume() const { return Protocol::volumeRawToPercent(m_status.volumeRaw); }
    int volumeRaw() const { return m_status.volumeRaw; }
    int headsetBattery() const { return m_status.headsetBattery; }
    int spareBattery() const { return m_status.spareBattery; }
    bool bluetooth() const { return m_status.bluetooth; }
    int activeProfile() const { return m_status.activeProfile; }
    int value(int field) const { return m_status.value(field); }
    bool voicePrompts() const { return value(Protocol::VoicePrompts) != 0; }
    int balance() const { return value(Protocol::Balance); }
    int eqPreset() const { return value(Protocol::EqPreset); }
    int micVolume() const { return value(Protocol::MicVolume); }
    bool noiseCancelling() const { return value(Protocol::NoiseCancelling) != 0; }
    bool sidetone() const { return value(Protocol::Sidetone) != 0; }
    int sidetoneLevel() const { return value(Protocol::SidetoneLevel); }
    QVariantList eqFrequencies() const;
    QVariantList eqGains() const;
    QVariantList eqQ() const;
    QVariantList eqTypes() const;
    // The curve's approximate response at EqResponsePoints log-spaced
    // frequencies, for drawing it.
    QVariantList eqResponse() const;
    static constexpr int EqResponsePoints = 96;
    bool eqDirty() const { return m_eqDirty; }
    const Protocol::Status &status() const { return m_status; }
    const Protocol::Battery &battery() const { return m_battery; }
    const Protocol::Equalizer &equalizer() const { return m_equalizer; }
    const Settings &settings() const { return m_settings; }

    bool keepSettings() const { return m_settings.keepSettings; }
    bool restoreVolume() const { return m_settings.restoreVolume; }
    void setKeepSettings(bool keep);
    void setRestoreVolume(bool restore);
    QDateTime lastRestored() const { return m_lastRestored; }

    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }

    Q_INVOKABLE void setVolume(int percent);
    Q_INVOKABLE void stepVolume(int deltaPercent);
    // Any per-profile setting (a Protocol::Field), on the active profile.
    Q_INVOKABLE void setField(int field, int value);
    Q_INVOKABLE void setVoicePrompts(bool on) { setField(Protocol::VoicePrompts, on); }
    Q_INVOKABLE void toggleVoicePrompts() { setVoicePrompts(!voicePrompts()); }
    Q_INVOKABLE void setBalance(int value) { setField(Protocol::Balance, value); }
    Q_INVOKABLE void setEqPreset(int preset) { setField(Protocol::EqPreset, preset); }
    Q_INVOKABLE void setMicVolume(int level) { setField(Protocol::MicVolume, level); }
    Q_INVOKABLE void setNoiseCancelling(bool on) { setField(Protocol::NoiseCancelling, on); }
    Q_INVOKABLE void setSidetone(bool on) { setField(Protocol::Sidetone, on); }
    Q_INVOKABLE void setSidetoneLevel(int level) { setField(Protocol::SidetoneLevel, level); }
    Q_INVOKABLE void setBluetooth(bool on);
    Q_INVOKABLE void setActiveProfile(int profile);
    // Not reported back by the headset, so they're sent and not shown.
    Q_INVOKABLE void setAutoOff(bool on);
    Q_INVOKABLE void setMicMute(bool muted);
    // Edits the custom EQ curve live. The EQ switches to Custom 1 so the
    // change can be heard, as CORE's editor does; saveProfile() keeps it.
    Q_INVOKABLE void setEqBand(int band, int frequency, double gain, double q, int type);
    Q_INVOKABLE void flattenEq();
    // Stores the active profile, custom EQ curve included, on the headset
    // (CORE's "Save To Profile").
    Q_INVOKABLE void saveProfile();
    // force sends even when the curve matches halo's copy, for callers that
    // only know the stored curve, not the one playing.
    void setEqualizer(const Protocol::Equalizer &eq, bool force = false);
    Q_INVOKABLE void refresh();
    void readEqualizer();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

signals:
    void stateChanged();
    void statusChanged();
    // A status report arrived from the headset itself.
    void statusReported();
    void equalizerChanged();
    // An EQ reply arrived from the headset (even one ignored mid-edit).
    void equalizerReported(const Protocol::Equalizer &eq);
    // The headset acknowledged a write: accepted, or refused (it refuses a
    // curve unless Custom 1 is the active preset).
    void acknowledged(int command, bool accepted);
    void settingsChanged();
    void historyChanged();
    // Settings were put back after the headset reset itself.
    void restored(const QString &what);

private:
    // What a change touched: the volume, Bluetooth, the active profile, or a
    // per-profile field (kind >= 0 is the Protocol::Field).
    enum Kind { VolumeKind = -1, BluetoothKind = -2, ProfileKind = -3, EqKind = -4 };
    struct Change {
        int kind;
        int profile;
        int before, after;
        qint64 at;
        QByteArray eqBefore, eqAfter; // for EqKind: the curves
    };

    void onReceived(const QByteArray &report);
    void onStatus(const Protocol::Status &status);
    void send(quint8 command, int value, bool throttled = false);
    void record(int kind, int before, int after);
    void apply(const Change &change, int value);
    void applyEq(const QByteArray &curve);
    void setOnline(bool online);
    void saveSettings();

    Transport *m_transport;
    QString m_settingsPath;
    Settings m_settings;
    Protocol::Status m_status;
    Protocol::Battery m_battery;
    Protocol::Equalizer m_equalizer;
    bool m_online = false;
    bool m_haveStatus = false;
    int m_pendingVolumeRaw = -1; // last raw volume halo wrote, until the headset echoes it
    QHash<quint8, int> m_queued; // slider values waiting out the throttle
    QByteArray m_queuedEq;       // a curve waiting out the throttle
    bool m_eqDirty = false;      // edited since the last save
    qint64 m_lastEqEditAt = 0;
    int m_pollTick = 0;
    QDateTime m_lastRestored;
    qint64 m_lastRestoreAt = 0;
    QList<Change> m_undo, m_redo;
    QTimer m_poll, m_answerTimeout, m_throttle, m_confirm;
};
