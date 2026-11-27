#pragma once

#include <QString>
#include <array>

#include "protocol.h"

// What the user wants the headset to keep, saved in
// ~/.config/halo/settings.json. The headset resets settings when it powers
// on (Voice Prompts and volume at least), so halo remembers each profile's
// settings and puts them back.
struct Settings {
    static constexpr int Profiles = Protocol::Profiles;

    bool keepSettings = true;
    bool restoreVolume = true;
    int volumeRaw = -1; // -1 until known
    // Per profile and field: -1 until known.
    std::array<Protocol::ProfileRecord, Profiles> profiles = blank();

    int value(int profile, int field) const;
    void setValue(int profile, int field, int value);

    // Kept for the Voice Prompts call sites and older tests.
    int voicePromptsFor(int profile) const { return value(profile, Protocol::VoicePrompts); }
    void setVoicePromptsFor(int profile, bool on) { setValue(profile, Protocol::VoicePrompts, on ? 1 : 0); }

    static QString defaultPath();
    static Settings load(const QString &path);
    // Written beside the target and renamed into place.
    bool save(const QString &path) const;

private:
    static std::array<Protocol::ProfileRecord, Profiles> blank();
};
