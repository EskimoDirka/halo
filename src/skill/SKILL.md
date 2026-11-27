---
name: halo
description: Read and change settings on a Glorious GHS Wireless InfinitePlay headset on Linux with the halo CLI. Use for headset battery, volume, or Voice Prompts questions and changes.
---

# halo

`halo` talks to the GHS Wireless InfinitePlay receiver over USB HID. Every
command below needs no display, exits 0 on success and 1 with the error on
stderr, and prints JSON with `--json`.

- `halo status --json`: battery (`battery.headset`, `battery.spare`, percent),
  `volume` (percent), `voicePrompts`, `activeProfile`, and the raw replies.
- `halo set volume <0-100> --json`: sets the volume; succeeds once the headset
  reports the new value.
- `halo set prompts <on|off> --json`: turns Voice Prompts on or off for the
  active profile, and remembers it.
- `halo watch`: keeps running and puts Voice Prompts and the volume back each
  time the headset powers on. The `halo-watch` systemd user service runs it.

`state` is `online`, `off` (receiver present, headset off or away), `missing`
(no receiver), or `permission` (halo's udev rule isn't installed).

Only volume and Voice Prompts can be changed so far. EQ is read-only; sidetone,
sleep timer, and lights aren't decoded yet. Don't write other commands to the
device by hand.
