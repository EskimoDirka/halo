#include "headset.h"

#include <QStringList>

using namespace Protocol;

Headset::Headset(Transport *transport, const QString &settingsPath, QObject *parent)
    : QObject(parent), m_transport(transport), m_settingsPath(settingsPath),
      m_settings(Settings::load(settingsPath)) {
    connect(m_transport, &Transport::received, this, &Headset::onReceived);
    connect(m_transport, &Transport::stateChanged, this, [this] {
        if (m_transport->state() == Transport::Open) {
            refresh();
        } else {
            setOnline(false);
        }
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

    // A dragged slider sends at most one volume write every 60 ms.
    m_volumeThrottle.setSingleShot(true);
    m_volumeThrottle.setInterval(60);
    connect(&m_volumeThrottle, &QTimer::timeout, this, [this] {
        if (m_queuedVolumeRaw >= 0) {
            sendVolume(m_queuedVolumeRaw);
            m_queuedVolumeRaw = -1;
            m_volumeThrottle.start();
        }
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

void Headset::refresh() {
    if (m_transport->state() != Transport::Open)
        return;
    m_transport->write(readRequest(Command::Status));
    m_transport->write(readRequest(Command::Battery));
    if (!m_equalizer.valid)
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
        if (eq.valid) {
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
void Headset::onStatus(const Status &status) {
    const Status previous = m_status;
    const bool firstSight = !m_haveStatus;
    m_status = status;
    m_haveStatus = true;

    const int wantVolume = m_settings.volumeRaw;
    const int wantPrompts = m_settings.voicePromptsFor(status.activeProfile);
    bool fixPrompts = false, fixVolume = false;

    if (firstSight) {
        // The headset just came into view: it may have powered on while
        // halo wasn't watching.
        fixPrompts = wantPrompts >= 0 && status.voicePrompts != bool(wantPrompts);
        fixVolume = wantVolume >= 0 && status.volumeRaw == PowerOnVolumeRaw && wantVolume != PowerOnVolumeRaw;
        if (wantVolume < 0) {
            m_settings.volumeRaw = status.volumeRaw;
            saveSettings();
        }
    } else {
        const bool volumeReset = status.volumeRaw == PowerOnVolumeRaw
                                 && qAbs(previous.volumeRaw - PowerOnVolumeRaw) > 1
                                 && m_pendingVolumeRaw != PowerOnVolumeRaw;
        const bool promptsReset = wantPrompts == 0 && status.voicePrompts && !previous.voicePrompts
                                  && status.activeProfile == previous.activeProfile;
        if (volumeReset || promptsReset) {
            fixPrompts = wantPrompts >= 0 && status.voicePrompts != bool(wantPrompts);
            fixVolume = wantVolume >= 0 && wantVolume != status.volumeRaw;
        } else if (status.volumeRaw != previous.volumeRaw) {
            // The wheel on the headset, or halo's own write echoing back:
            // either way it's the volume to keep.
            if (m_pendingVolumeRaw == status.volumeRaw)
                m_pendingVolumeRaw = -1;
            m_settings.volumeRaw = status.volumeRaw;
            saveSettings();
        }
    }

    fixVolume = fixVolume && m_settings.restoreVolume;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    // A short cooldown keeps halo from fighting the headset if it ever
    // refuses a value.
    if (m_settings.keepSettings && (fixPrompts || fixVolume) && now - m_lastRestoreAt > 3000) {
        QStringList what;
        if (fixPrompts) {
            sendPrompts(bool(wantPrompts));
            what << "voice prompts";
        }
        if (fixVolume) {
            sendVolume(wantVolume);
            what << "volume";
        }
        m_lastRestoreAt = now;
        m_lastRestored = QDateTime::currentDateTime();
        emit restored(what.join(", "));
    }
    emit statusChanged();
}

void Headset::sendVolume(int raw) {
    m_pendingVolumeRaw = raw;
    m_transport->write(writeRequest(Command::Volume, quint8(raw)));
    m_confirm.start();
}

void Headset::sendPrompts(bool on) {
    m_transport->write(writeRequest(Command::VoicePrompts, on ? 1 : 0));
    m_confirm.start();
}

void Headset::setVolume(int percent) {
    const int raw = volumePercentToRaw(percent);
    if (!m_online || raw == m_status.volumeRaw)
        return;
    record(Change::Volume, m_status.volumeRaw, raw);
    apply(Change::Volume, raw);
}

void Headset::stepVolume(int deltaPercent) { setVolume(volume() + deltaPercent); }

void Headset::setVoicePrompts(bool on) {
    if (!m_online)
        return;
    if (on != m_status.voicePrompts)
        record(Change::Prompts, m_status.voicePrompts, on);
    apply(Change::Prompts, on);
}

void Headset::apply(Change::Field field, int value) {
    if (field == Change::Volume) {
        m_settings.volumeRaw = value;
        m_status.volumeRaw = value;
        if (m_volumeThrottle.isActive()) {
            m_queuedVolumeRaw = value;
        } else {
            sendVolume(value);
            m_volumeThrottle.start();
        }
    } else {
        // The headset applies Voice Prompts to its active profile.
        m_settings.setVoicePromptsFor(m_status.activeProfile, value);
        m_status.voicePrompts = value;
        sendPrompts(value);
    }
    saveSettings();
    emit statusChanged();
}

// One drag of the slider is one step to undo.
void Headset::record(Change::Field field, int before, int after) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!m_undo.isEmpty() && field == Change::Volume && m_undo.last().field == Change::Volume
        && now - m_undo.last().at < 1000) {
        m_undo.last().after = after;
        m_undo.last().at = now;
    } else {
        m_undo.append({field, before, after, now});
    }
    m_redo.clear();
    emit historyChanged();
}

void Headset::undo() {
    if (m_undo.isEmpty() || !m_online)
        return;
    Change change = m_undo.takeLast();
    change.at = 0;
    apply(change.field, change.before);
    m_redo.append(change);
    emit historyChanged();
}

void Headset::redo() {
    if (m_redo.isEmpty() || !m_online)
        return;
    const Change change = m_redo.takeLast();
    apply(change.field, change.after);
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
