#include "settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

using namespace Protocol;

std::array<ProfileRecord, Settings::Profiles> Settings::blank() {
    std::array<ProfileRecord, Profiles> profiles;
    for (auto &record : profiles)
        record.fill(-1);
    return profiles;
}

int Settings::value(int profile, int field) const {
    if (profile < 1 || profile > Profiles || field < 0 || field >= FieldCount)
        return -1;
    return profiles[size_t(profile - 1)][size_t(field)];
}

void Settings::setValue(int profile, int field, int value) {
    if (profile < 1 || profile > Profiles || field < 0 || field >= FieldCount)
        return;
    const auto &info = fieldInfo(field);
    profiles[size_t(profile - 1)][size_t(field)] = value < info.min || value > info.max ? -1 : value;
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
    settings.volumeRaw = qBound(-1, o.value("volumeRaw").toInt(-1), MaxVolumeRaw);
    const QJsonArray profiles = o.value("profiles").toArray();
    for (int i = 0; i < Profiles && i < profiles.size(); ++i) {
        const QJsonObject record = profiles[i].toObject();
        for (int field = 0; field < FieldCount; ++field)
            settings.setValue(i + 1, field, record.value(fieldInfo(field).key).toInt(-1));
    }
    return settings;
}

bool Settings::save(const QString &path) const {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    QJsonArray list;
    for (const auto &record : profiles) {
        QJsonObject o;
        for (int field = 0; field < FieldCount; ++field)
            o.insert(fieldInfo(field).key, record[size_t(field)]);
        list.append(o);
    }
    const QJsonObject o{{"keepSettings", keepSettings},
                        {"restoreVolume", restoreVolume},
                        {"volumeRaw", volumeRaw},
                        {"profiles", list}};
    file.write(QJsonDocument(o).toJson());
    return file.commit();
}
