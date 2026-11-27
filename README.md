# halo

Settings for the Glorious **GHS Wireless InfinitePlay** headset on Linux.

![halo showing battery, profiles, sound, microphone, and custom EQ settings](docs/screenshot.png)

The headset works on Linux as a standard USB headset, but its settings live
in Glorious CORE, which only runs on Windows. halo reads and changes them
directly over USB, and fixes one long-standing annoyance: the headset turns
Voice Prompts back on and resets its volume to 50% every time it powers on.
halo notices and puts each profile's settings back.

halo is an unofficial, open-source project. It isn't made by, endorsed by, or
affiliated with Glorious.

## What it does

- Headset and spare battery levels
- Volume, game/chat balance, and EQ preset (Glorious Mode, Music, Immersion, Voice Call, FPS, Custom)
- Voice Prompts
- Microphone level, noise cancelling, and sidetone
- Bluetooth on or off, and the five profiles
- Keeps every profile's settings across power-ons
- Shows the custom EQ curve stored on the headset (editing it comes next)
- A command line for scripts and agents

## Install

On Omarchy or Arch:

```sh
git clone https://github.com/EskimoDirka/halo
cd halo
bin/install
```

This installs the app, its launcher entry, a udev rule that lets the
logged-in user talk to the receiver, and a systemd user service. Unplug and
replug the receiver once after installing.

To keep settings across power-ons even when the window is closed:

```sh
systemctl --user enable --now halo-watch
```

## Shortcuts

| Key | Action |
|---|---|
| Space | Voice Prompts on or off |
| ↑ / ↓ | Volume up or down |
| K | Keep my settings on or off |
| 1 – 5 | Switch profile |
| R | Read the headset again |
| Ctrl+Z / Ctrl+Shift+Z | Undo / redo |
| ? | Show or hide shortcuts |
| Q | Quit |

## Command line

```sh
halo status [--json]
halo set volume <0-100>
halo set prompts <on|off>
halo set balance <0-20>            # 0 all game, 10 even, 20 all chat
halo set eq <glorious|music|immersion|voice|fps|custom1|custom2>
halo set mic <low|mid|high>
halo set enc <on|off>              # microphone noise cancelling
halo set sidetone <off|low|mid|high>
halo set bluetooth <on|off>
halo set profile <1-5>
halo set autooff <on|off>          # power off after 30 idle minutes
halo set mute <on|off>
halo watch                         # what the halo-watch service runs
halo skill install                 # agent skill for Claude Code
```

Each takes `--json`, and exits 0 on success or 1 with the error on stderr. A
change only counts once the headset reports it back (auto off and mute aren't
reported, so those are sent and not confirmed).

## How it works

The receiver takes settings commands on HID report `0x15`.
[docs/protocol.md](docs/protocol.md) has everything known so far, how each part
was worked out, and how sure it is. halo only sends commands seen from
Glorious CORE itself.

Firmware research, aiming for a headset that keeps its own settings, is in
[docs/firmware.md](docs/firmware.md).

## Development

```sh
bin/build    # build/halo
bin/test     # tests run offscreen, no headset needed
```

C++17 and Qt Quick, built with qmake. Translations live in `src/i18n/`
(Danish, German, and Spanish so far); halo follows Omarchy's language setting.

## License

MIT. Theme and language handling follow Omacut (MIT, David Heinemeier Hansson).
