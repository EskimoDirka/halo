#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QTimer>
#include <QVariantList>

#include "protocol.h"
#include "settings.h"
#include "transport.h"

// The headset's state and the one thing CORE never did: put Voice Prompts
// and the volume back after every power-on. Exposed to QML as `headset`.
class Headset : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool online READ online NOTIFY stateChanged)
    Q_PROPERTY(int volume READ volume NOTIFY statusChanged)
    Q_PROPERTY(bool voicePrompts READ voicePrompts NOTIFY statusChanged)
    Q_PROPERTY(int activeProfile READ activeProfile NOTIFY statusChanged)
    Q_PROPERTY(int headsetBattery READ headsetBattery NOTIFY statusChanged)
    Q_PROPERTY(int spareBattery READ spareBattery NOTIFY statusChanged)
    Q_PROPERTY(QVariantList eqFrequencies READ eqFrequencies NOTIFY equalizerChanged)
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
    bool voicePrompts() const { return m_status.voicePrompts; }
    int activeProfile() const { return m_status.activeProfile; }
    int headsetBattery() const { return m_status.headsetBattery; }
    int spareBattery() const { return m_status.spareBattery; }
    QVariantList eqFrequencies() const;
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
    Q_INVOKABLE void setVoicePrompts(bool on);
    Q_INVOKABLE void toggleVoicePrompts() { setVoicePrompts(!m_status.voicePrompts); }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

signals:
    void stateChanged();
    void statusChanged();
    // A status report arrived from the headset itself.
    void statusReported();
    void equalizerChanged();
    void settingsChanged();
    void historyChanged();
    // Settings were put back after the headset reset itself.
    void restored(const QString &what);

private:
    struct Change {
        enum Field { Volume, Prompts } field;
        int before, after;
        qint64 at;
    };

    void onReceived(const QByteArray &report);
    void onStatus(const Protocol::Status &status);
    void sendVolume(int raw);
    void sendPrompts(bool on);
    void record(Change::Field field, int before, int after);
    void apply(Change::Field field, int value);
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
    int m_queuedVolumeRaw = -1;  // a slider value waiting out the throttle
    QDateTime m_lastRestored;
    qint64 m_lastRestoreAt = 0;
    QList<Change> m_undo, m_redo;
    QTimer m_poll, m_answerTimeout, m_volumeThrottle, m_confirm;
};
