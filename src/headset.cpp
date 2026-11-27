#include "headset.h"

#include <QStringList>

using namespace Protocol;

Headset::Headset(Transport *transport, const QString &settingsPath, QObject *parent)
    : QObject(parent), m_transport(transport), m_settingsPath(settingsPath),
      m_settings(Settings::load(settingsPath)) {
    connect(m_transport, &Transport::received, this, &Headset::onReceived);
    connect(m_transport, &Transport::stateChanged, this, [this] {
        if (m_transport->state() == Transport::Open)
            refresh();
        else
            setOnline(false);
        emit stateChanged();
    });

    // CORE polls several times a second; battery levels move slowly, and the
    // headset pushes every other change itself, so a slow poll is enough.
    m_poll.setInterval(15000);
    connect(&m_poll, &QTimer::timeout, this, &Headset::refresh);
    m_poll.start();

    // No status answer means the receiver is there but the headset is off or
    // out of range.
    m_answerTimeout.setSingleShot(true);
    m_answerTimeout.setInterval(1500);
    connect(&m_answerTimeout, &QTimer::timeout, this, [this] { setOnline(false); });

    // A dragged slider sends at most one write per setting every 60 ms; the
    // last value always goes out.
    m_throttle.setSingleShot(true);
    m_throttle.setInterval(60);
    connect(&m_throttle, &QTimer::timeout, this, [this] {
        const auto queued = m_queued;
        m_queued.clear();
        for (auto it = queued.begin(); it != queued.end(); ++it)
            send(it.key(), it.value());
        if (!queued.isEmpty())
            m_throttle.start();
    });

    // Read the state back shortly after a write, so the window shows what the
    // headset actually took.
    m_confirm.setSingleShot(true);
    m_confirm.setInterval(300);
    connect(&m_confirm, &QTimer::timeout, this, [this] {
        if (m_transport->state() == Transport::Open)
            m_transport->write(readRequest(Command::Status));
    });

    if (m_transport->state() == Transport::Open)
        refresh();
}

QString Headset::state() const {
    switch (m_transport->state()) {
    case Transport::Missing: return "missing";
    case Transport::NoPermission: return "permission";
    case Transport::Open: break;
    }
    return m_online ? "online" : "off";
}

QVariantList Headset::eqFrequencies() const {
    QVariantList list;
    for (int f : m_equalizer.frequencies)
        list << f;
    return list;
}

QVariantList Headset::eqGains() const {
    QVariantList list;
    for (double g : m_equalizer.gains)
        list << g;
    return list;
}

void Headset::refresh() {
    if (m_transport->state() != Transport::Open)
        return;
    m_transport->write(readRequest(Command::Status));
    m_transport->write(readRequest(Command::Battery));
    m_transport->write(readRequest(Command::Equalizer));
    if (!m_answerTimeout.isActive())
        m_answerTimeout.start();
}

void Headset::onReceived(const QByteArray &report) {
    switch (replyCommand(report)) {
    case Command::Status: {
        const Status status = parseStatus(report);
        if (!status.valid) {
            setOnline(false);
            return;
        }
        m_answerTimeout.stop();
        setOnline(true);
        onStatus(status);
        emit statusReported();
        break;
    }
    case Command::Battery: {
        const Battery battery = parseBattery(report);
        if (!battery.valid)
            return;
        m_battery = battery;
        m_status.headsetBattery = battery.headset;
        m_status.spareBattery = battery.spare;
        emit statusChanged();
        break;
    }
    case Command::Equalizer: {
        const Equalizer eq = parseEqualizer(report);
        if (eq.valid && eq.payload != m_equalizer.payload) {
            m_equalizer = eq;
            emit equalizerChanged();
        }
        break;
    }
    default:
        break;
    }
}

// The headset powers on with Voice Prompts on and the volume at 50%, whatever
// was set before. That shows up as a status report with those defaults
// arriving out of nowhere: the volume jumping straight to 50% rather than
// stepping there, or Voice Prompts turning on while the user wants them off.
// Any other change came from the user (the headset's own buttons, or another
// app), so halo adopts it rather than fighting it.
void Headset::onStatus(const Status &status) {
    const Status previous = m_status;
    const bool firstSight = !m_haveStatus;
    const bool switched = !firstSight && previous.activeProfile != status.activeProfile;
    m_status = status;
    m_haveStatus = true;

    const int profile = status.activeProfile;
    const int wantVolume = m_settings.volumeRaw;
    bool reset = firstSight || switched;
    bool fixVolume = false;
    bool changed = false;

    if (firstSight) {
        // The headset just came into view: it may have powered on while
        // halo wasn't watching.
        fixVolume = wantVolume >= 0 && status.volumeRaw == PowerOnVolumeRaw && wantVolume != PowerOnVolumeRaw;
    } else if (!switched) {
        const bool volumeReset = status.volumeRaw == PowerOnVolumeRaw
                                 && qAbs(previous.volumeRaw - PowerOnVolumeRaw) > 1
                                 && m_pendingVolumeRaw != PowerOnVolumeRaw;
        const bool promptsReset = m_settings.value(profile, VoicePrompts) == 0
                                  && status.value(VoicePrompts) && !previous.value(VoicePrompts);
        reset = volumeReset || promptsReset;
        if (reset) {
            fixVolume = wantVolume >= 0 && wantVolume != status.volumeRaw;
        } else {
            if (status.volumeRaw != previous.volumeRaw) {
                if (m_pendingVolumeRaw == status.volumeRaw)
                    m_pendingVolumeRaw = -1;
                m_settings.volumeRaw = status.volumeRaw;
                changed = true;
            }
            for (int field = 0; field < FieldCount; ++field)
                if (status.value(field) != previous.value(field)) {
                    m_settings.setValue(profile, field, status.value(field));
                    changed = true;
                }
        }
    }

    // Fields halo has no wish for yet take the headset's value.
    QList<int> fix;
    for (int field = 0; field < FieldCount; ++field) {
        const int want = m_settings.value(profile, field);
        if (want < 0) {
            m_settings.setValue(profile, field, status.value(field));
            changed = true;
        } else if (reset && want != status.value(field)) {
            fix << field;
        }
    }
    if (m_settings.volumeRaw < 0) {
        m_settings.volumeRaw = status.volumeRaw;
        changed = true;
    }
    if (changed)
        saveSettings();

    fixVolume = fixVolume && m_settings.restoreVolume;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    // A short cooldown keeps halo from fighting the headset if it ever
    // refuses a value.
    if (m_settings.keepSettings && (!fix.isEmpty() || fixVolume) && now - m_lastRestoreAt > 3000) {
        QStringList what;
        for (int field : fix) {
            const int want = m_settings.value(profile, field);
            // The headset's next report shows the value once it's taken;
            // until then the state stays as reported.
            send(fieldInfo(field).command, want);
            what << fieldInfo(field).key;
        }
        if (fixVolume) {
            send(Command::Volume, wantVolume);
            what << "volume";
        }
        m_lastRestoreAt = now;
        m_lastRestored = QDateTime::currentDateTime();
        emit restored(what.join(", "));
    }
    emit statusChanged();
}

void Headset::send(quint8 command, int value, bool throttled) {
    if (throttled && m_throttle.isActive()) {
        m_queued.insert(command, value);
        return;
    }
    if (command == Command::Volume)
        m_pendingVolumeRaw = value;
    m_transport->write(writeRequest(command, quint8(value)));
    if (throttled)
        m_throttle.start();
    m_confirm.start();
}

void Headset::setVolume(int percent) {
    const int raw = volumePercentToRaw(percent);
    if (!m_online || raw == m_status.volumeRaw)
        return;
    record(VolumeKind, m_status.volumeRaw, raw);
    apply(m_undo.last(), raw);
}

void Headset::stepVolume(int deltaPercent) { setVolume(volume() + deltaPercent); }

void Headset::setField(int field, int value) {
    if (!m_online || field < 0 || field >= FieldCount)
        return;
    const auto &info = fieldInfo(field);
    if (value < info.min || value > info.max)
        return;
    if (value == m_status.value(field)) {
        // Nothing to send, but remember the choice for the next power-on.
        m_settings.setValue(m_status.activeProfile, field, value);
        saveSettings();
        return;
    }
    record(field, m_status.value(field), value);
    apply(m_undo.last(), value);
}

void Headset::setBluetooth(bool on) {
    if (!m_online || on == m_status.bluetooth)
        return;
    record(BluetoothKind, m_status.bluetooth, on);
    apply(m_undo.last(), on);
}

void Headset::setActiveProfile(int profile) {
    if (!m_online || profile < 1 || profile > Profiles || profile == m_status.activeProfile)
        return;
    record(ProfileKind, m_status.activeProfile, profile);
    apply(m_undo.last(), profile);
}

void Headset::setAutoOff(bool on) {
    if (m_online)
        send(Command::AutoOff, on ? 1 : 0);
}

void Headset::setMicMute(bool muted) {
    if (m_online)
        send(Command::MicMute, muted ? 1 : 0);
}

void Headset::apply(const Change &change, int value) {
    switch (change.kind) {
    case VolumeKind:
        m_settings.volumeRaw = value;
        m_status.volumeRaw = value;
        send(Command::Volume, value, true);
        break;
    case BluetoothKind:
        m_status.bluetooth = value;
        send(Command::Bluetooth, value);
        break;
    case ProfileKind:
        // The headset reports the new profile's settings right away.
        m_status.activeProfile = value;
        send(Command::ActiveProfile, value);
        break;
    default: {
        // A profile's setting only applies while that profile is active.
        if (change.profile != m_status.activeProfile)
            return;
        m_settings.setValue(change.profile, change.kind, value);
        m_status.profiles[size_t(change.profile - 1)][size_t(change.kind)] = value;
        send(fieldInfo(change.kind).command, value, change.kind == Balance);
        break;
    }
    }
    saveSettings();
    emit statusChanged();
}

// One drag of a slider is one step to undo.
void Headset::record(int kind, int before, int after) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool slider = kind == VolumeKind || kind == Balance;
    if (slider && !m_undo.isEmpty() && m_undo.last().kind == kind && now - m_undo.last().at < 1000) {
        m_undo.last().after = after;
        m_undo.last().at = now;
    } else {
        m_undo.append({kind, m_status.activeProfile, before, after, now});
    }
    m_redo.clear();
    emit historyChanged();
}

void Headset::undo() {
    if (m_undo.isEmpty() || !m_online)
        return;
    Change change = m_undo.takeLast();
    change.at = 0;
    apply(change, change.before);
    m_redo.append(change);
    emit historyChanged();
}

void Headset::redo() {
    if (m_redo.isEmpty() || !m_online)
        return;
    const Change change = m_redo.takeLast();
    apply(change, change.after);
    m_undo.append(change);
    emit historyChanged();
}

void Headset::setKeepSettings(bool keep) {
    if (keep == m_settings.keepSettings)
        return;
    m_settings.keepSettings = keep;
    saveSettings();
}

void Headset::setRestoreVolume(bool restore) {
    if (restore == m_settings.restoreVolume)
        return;
    m_settings.restoreVolume = restore;
    saveSettings();
}

void Headset::setOnline(bool online) {
    if (!online)
        m_haveStatus = false;
    if (online == m_online)
        return;
    m_online = online;
    emit stateChanged();
}

void Headset::saveSettings() {
    m_settings.save(m_settingsPath);
    emit settingsChanged();
}
