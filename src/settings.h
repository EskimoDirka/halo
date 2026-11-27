#pragma once

#include <QString>
#include <array>

// What the user wants the headset to keep, saved in
// ~/.config/halo/settings.json. The headset forgets Voice Prompts and volume
// every time it powers off, so halo remembers them for it, per profile where
// the headset keeps a setting per profile.
struct Settings {
    static constexpr int Profiles = 5;

    bool keepSettings = true;
    bool restoreVolume = true;
    int volumeRaw = -1; // -1 until known
    // Per profile: -1 until the user picks, then 0 or 1.
    std::array<int, Profiles> voicePrompts{-1, -1, -1, -1, -1};

    int voicePromptsFor(int profile) const;
    void setVoicePromptsFor(int profile, bool on);

    static QString defaultPath();
    static Settings load(const QString &path);
    // Written beside the target and renamed into place.
    bool save(const QString &path) const;
};
