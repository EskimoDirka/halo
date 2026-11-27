# GHS Wireless InfinitePlay protocol notes

What halo knows about how the receiver is controlled, and how sure each part
is. Worked out from Glorious CORE 2.1.23's own logs (it logs every command it
sends and every reply as hex) during a session that changed each setting in
CORE one at a time, and from read-only queries on Linux. Headset firmware
3.1.3.

## The device

| | |
|---|---|
| Receiver | USB `3794:d005`, "Realtek GH Wireless InfinitePlay". CORE's device list says `d003`; its own hot-plug log shows `d005`. |
| Interfaces | 5 × USB Audio (playback, mic) and 1 × HID |

The HID interface carries several report ids. Settings go through report
`0x15` (usage page `0x0062`), 63 bytes in each direction after the report id.
The receiver's other vendor reports (`0x24`, `0x27`, `0x34`, `0x37`, `0x3a`–
`0x3d`, `0x10`, `0x2a`–`0x2d`, `0x80`/`0x81`) were never used by CORE in any
capture; their shape (32-bit fields, 60-byte buffers) suggests firmware update
and debug channels. halo never writes to them.

## Framing

```
request  15 <cmd> 01 00                 read
         15 <cmd> 02 <len> <data…>      write
reply    15 <cmd> 01 <len> <data…>      (zero-padded to 64 bytes)
```

Every write is acknowledged with `15 <cmd> 01 00` when the headset takes it,
or `15 <cmd> 00 00` when it refuses it. The headset sends a `25`
status reply on its own about every 30 seconds, and at once when its state
changes (the volume wheel, a profile switch, powering on).

## Commands

All confirmed by watching CORE send them as each setting changed, unless the
last column says otherwise.

| cmd | setting | write data | notes |
|---|---|---|---|
| `01` | volume | raw 0–50 | percent = raw × 2 |
| `02` | game/chat balance | 0–20 | `0A` even; `01` shows as 95/5 game, `13` as 5/95 chat |
| `03` | Voice Prompts | `00` off, `01` on | per profile |
| `04` | EQ preset | 1 Glorious Mode, 2 Music, 3 Immersion, 4 Voice Call, 5 FPS, 6 Custom 1 | 7 is presumably Custom 2 (not seen) |
| `05` | custom EQ curve | 50 bytes, below | also readable |
| `08` | mic mute | `01` muted, `00` live | not in the status block |
| `09` | mic level | 1 low, 2 mid, 3 high | |
| `10` | mic noise cancelling (ENC) | `00` off, `01` on | |
| `11` | sidetone | `00` off, `01` on | |
| `12` | sidetone level | 1 low, 2 mid, 3 high | |
| `13` | serial number | read only, ASCII | |
| `14` | auto off after 30 idle minutes | `00` off, `01` on | CORE offers no other times; not yet seen in the status block |
| `15` | Bluetooth | `00` off, `01` on | |
| `17` | battery | read only | headset %, spare pack %, one flag byte |
| `18` | save profile | no data (`15 18 02 00`) | stores the active profile on the headset, custom EQ curve included; see below |
| `20` | active profile | 1–5 | the headset pushes a status at once |
| `25` | status | read, and pushed | below |
| `26` `27` `28` | unknown | read only | CORE polls every 2.5 s; replies never changed during any setting change |
| `30` | unknown | `00` | CORE writes it once at startup; all-zero reply |

CORE offers no base-light, sleep-timer choice, or other noise-suppression
setting for this headset. Its "Volume Boost" was not captured (it warns that
it raises the volume).

## Status block (`25`)

Payload, 51 bytes:

| offset | example | meaning |
|---|---|---|
| 0 | `11` | volume, raw 0–50 |
| 1 | `01` | unknown; possibly auto off |
| 2–3 | `00 01` | unknown |
| 4 | `01` | Bluetooth on |
| 5 | `32` | headset battery % |
| 6 | `5c` | spare pack (charging in the base) %; it wanders slightly |
| 7 | `01` | unknown |
| 8 + 8 × (n − 1) | 8 bytes | profile n's record (below) |
| 48 | `01` | active profile, 1–5 |

Profile record: `?? balance prompts eq mic enc sidetone sidetone-level`, for
example `01 0A 00 01 03 01 00 02`. The first byte was `01` in every profile.

The headset powers on with volume `19` (50%) and Voice Prompts on, whatever
was set before. Glorious says the 50% start-up volume is a deliberate safety
feature; Voice Prompts resetting is a bug they are looking into. halo spots
the reset in the status block and writes the user's settings back.

## Custom EQ (`05`)

Payload, 50 bytes: two 25-byte blocks. CORE logs its own mapping, so the
fields are confirmed:

| offset | meaning |
|---|---|
| 0–9 | five band frequencies, big-endian Hz |
| 10–14 | five gains, signed, tenths of a dB (`3C` +6 dB, `C4` −6 dB) |
| 15–19 | five Q values, tenths (CORE clamps to 4.0) |
| 20–24 | five filter types: 0 peak, 1 low shelf, 2 high shelf, 3 low pass, 4 high pass, 5 probably notch |

CORE writes only the first block; the second is always the flat default
(45/250/1k/4k/10k Hz, 0 dB, Q 2.0, peak). What it controls is unknown.

Tested on Linux:

- The headset **refuses a curve** (`15 05 00 00`) unless Custom 1 is the
  active preset. CORE's editor switches to it (`04` → 6) as it opens; halo
  does the same before its first write.
- A **read returns the stored curve**, not the one playing. A written curve
  plays at once but only shows up in a read after a `18` save.

## Saving a profile (`18`)

CORE sends `18` from its custom EQ editor's Save To Profile button. It looks
like it stores the whole active profile, not just the curve: after a CORE
session that saved while Voice Prompts were off and Custom 1 was playing, the
headset powered on with Voice Prompts off (they had reset to on at every
power-on before) and with Custom 1 selected. A second power cycle after a
save with other settings would confirm it.

## Still to work out

- Header bytes 1–3 and 7, the first byte of each profile record, and `26`–`30`.
- Whether auto off or mic mute appear anywhere in the status.
- What the EQ's second block does, and how Custom 2 stores its curve.
- Exactly what `18` stores.
