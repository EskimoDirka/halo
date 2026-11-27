---
name: halo
description: Read and change settings on a Glorious GHS Wireless InfinitePlay headset on Linux with the halo CLI. Use for headset battery, volume, or Voice Prompts questions and changes.
---

# halo

`halo` talks to the GHS Wireless InfinitePlay receiver over USB HID. Every
command below needs no display, exits 0 on success and 1 with the error on
stderr, and prints JSON with `--json`.

- `halo status --json`: battery (`battery.headset`, `battery.spare`, percent),
  `volume` (percent), `activeProfile`, the active profile's `balance` (0-20),
  `eqPreset` and `eqPresetName`, `voicePrompts`, `micVolume` (1-3),
  `noiseCancelling`, `sidetone`, `sidetoneLevel` (1-3), `bluetooth`, the
  `customEq` curve, and the raw replies.
- `halo set <setting> <value> --json`, confirmed by the headset's own report:
  `volume 0-100`, `prompts on|off`, `balance 0-20` (0 all game, 20 all chat),
  `eq glorious|music|immersion|voice|fps|custom1|custom2`, `mic low|mid|high`,
  `enc on|off`, `sidetone off|low|mid|high`, `bluetooth on|off`, `profile 1-5`.
  `autooff on|off` and `mute on|off` are sent but can't be confirmed.
  Per-profile settings apply to the active profile and are remembered.
- `halo watch`: keeps running and puts each profile's settings back each time
  the headset powers on. The `halo-watch` systemd user service runs it.

`state` is `online`, `off` (receiver present, headset off or away), `missing`
(no receiver), or `permission` (halo's udev rule isn't installed).

- `halo eq --json`: the custom EQ curve, five bands of `hz`, `db`, `q`, `type`.
- `halo eq band <1-5> [hz=20-20000] [db=-12..12] [q=0.1-4] [type=peak|lowshelf|highshelf|lowpass|highpass|notch] [--save]`
  and `halo eq flat [--save]`: edit the curve. The EQ switches to Custom 1
  and the edit plays at once; without `--save` the headset forgets it when
  it turns off.
- `halo save`: store the active profile on the headset, so it keeps these
  settings after power-on by itself.

Don't write other commands to the device by hand.
