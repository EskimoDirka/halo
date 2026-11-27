#include "settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

int Settings::voicePromptsFor(int profile) const {
    return profile >= 1 && profile <= Profiles ? voicePrompts[size_t(profile - 1)] : -1;
}

void Settings::setVoicePromptsFor(int profile, bool on) {
    if (profile >= 1 && profile <= Profiles)
        voicePrompts[size_t(profile - 1)] = on ? 1 : 0;
}

QString Settings::defaultPath() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/halo/settings.json";
}

Settings Settings::load(const QString &path) {
    Settings settings;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return settings;
    const QJsonObject o = QJsonDocument::fromJson(file.readAll()).object();
    settings.keepSettings = o.value("keepSettings").toBool(true);
    settings.restoreVolume = o.value("restoreVolume").toBool(true);
    settings.volumeRaw = qBound(-1, o.value("volumeRaw").toInt(-1), 50);
    const QJsonArray profiles = o.value("profiles").toArray();
    for (int i = 0; i < Profiles && i < profiles.size(); ++i)
        settings.voicePrompts[size_t(i)] = qBound(-1, profiles[i].toObject().value("voicePrompts").toInt(-1), 1);
    return settings;
}

bool Settings::save(const QString &path) const {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    QJsonArray profiles;
    for (int value : voicePrompts)
        profiles.append(QJsonObject{{"voicePrompts", value}});
    const QJsonObject o{{"keepSettings", keepSettings},
                        {"restoreVolume", restoreVolume},
                        {"volumeRaw", volumeRaw},
                        {"profiles", profiles}};
    file.write(QJsonDocument(o).toJson());
    return file.commit();
}
