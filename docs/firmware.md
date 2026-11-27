# Firmware research

The longer-term goal is firmware that keeps settings (Voice Prompts, volume,
per-profile settings) on the headset itself, so it doesn't need halo watching
from the PC. This is research, not a plan with dates: nothing here has been
tried on a device yet.

## Ground rules

- **No flashing without a verified backup and a tested way back.** A bad
  flash can leave the headset or receiver unusable.
- Anything that writes firmware lives outside halo's normal commands and asks
  for explicit confirmation.
- Nothing from Glorious's own firmware images goes into this repository. Notes
  on how they're structured are fine; the images themselves aren't ours to
  publish.

## What's known

- The receiver identifies as Realtek ("GH Wireless InfinitePlay"), Realtek's
  low-latency 2.4 GHz gaming audio platform. The exact chip in the headset and
  the receiver is not yet identified.
- CORE updates firmware itself and reports version 3.1.3 for this unit. Its
  log notes it checks the version over a wired USB connection to the headset.
- The receiver exposes vendor HID reports that look like a firmware update or
  debug channel (see [protocol.md](protocol.md)).

## Steps, in order

1. **Identify the chips.** Read the markings on the receiver and headset
   boards (photos welcome in an issue). This decides everything after.
2. **Capture an official update.** Run a CORE firmware update with the
   traffic recorded ([capturing.md](capturing.md), plus a USB capture with
   `usbmon` on the Linux host while the VM owns the device). This shows the
   update protocol and how the image is delivered.
3. **Find a read-back path.** Before writing anything, find a way to read the
   current firmware out (an update-protocol read command, or the chip's
   UART/SWD boot mode) and verify it round-trips.
4. **Look for the settings store.** The smallest useful change is likely not
   new firmware but the stored defaults that make Voice Prompts come back on.
5. Only then consider changes, on a spare unit first.
