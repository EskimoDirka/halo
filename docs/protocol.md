# GHS Wireless InfinitePlay protocol notes

What halo knows about how the receiver is controlled, and how sure each part
is. Worked out from Glorious CORE 2.1's own logs (it logs every command it
sends and every reply as hex) and from read-only queries on Linux.

## The device

| | |
|---|---|
| Receiver | USB `3794:d005` (CORE's device list says `3794:d003`), "Realtek GH Wireless InfinitePlay" |
| Interfaces | 5 × USB Audio (playback, mic) and 1 × HID |
| Firmware tested | 3.1.3, as CORE reports it |

The HID interface carries several report ids. Settings go through report
`0x15` (usage page `0x0062`), 63 bytes in each direction after the report id.
The receiver's other vendor reports (`0x24`, `0x27`, `0x34`, `0x37`, `0x3a`–
`0x3d`, `0x10`, `0x2a`–`0x2d`, `0x80`/`0x81`) have not been seen in use;
their shape (32-bit fields, 60-byte buffers) suggests firmware update and
debug channels. halo never writes to them.

## Framing

```
request  15 <cmd> 01 00                 read
         15 <cmd> 02 <len> <data…>      write
reply    15 <cmd> 01 <len> <data…>      (zero-padded to 64 bytes)
```

The headset also sends a `25` status reply on its own whenever its state
changes (the volume wheel, powering on).

## Commands

| cmd | direction | meaning | confidence |
|---|---|---|---|
| `01` | write | volume, raw 0–50 (percent = raw × 2) | confirmed: CORE writes it, halo tested it |
| `03` | write | Voice Prompts for the active profile, `00` off / `01` on | `00` confirmed from CORE; `01` by symmetry with the status field |
| `05` | read | EQ bands (below) | layout confirmed, gains inferred |
| `13` | read | serial number, ASCII | confirmed |
| `17` | read | battery: headset %, spare pack %, one flag byte | confirmed against the status block |
| `25` | read / pushed | status block (below) | partly decoded |
| `26` `27` `28` | read | polled by CORE several times a second; replies `03 03 01 03`, `03 03 00 02`, `01 [00/01]` | unknown, possibly the base's charge LEDs |
| `30` | read / write | CORE sends `30 02 01 00` once at connect | unknown |

## Status block (`25`)

Payload, 51 bytes:

| offset | example | meaning |
|---|---|---|
| 0 | `22` | volume, raw 0–50 |
| 1 | `01` | flag that toggles independently, unknown (mic mute?) |
| 2–4 | `00 01 01` | constant so far |
| 5 | `32` | headset battery % |
| 6 | `5c` | spare pack (charging in the base) % |
| 7 | `01` | active profile, 1–5 |
| 8 + 8 × (n − 1) | `01 0a VP n 03 01 ?? 02` | profile n: VP is Voice Prompts (`00` off, `01` on) |

The headset powers on with volume `19` (50%) and Voice Prompts on, whatever
was set before. Glorious says the 50% start-up volume is a deliberate safety
feature; Voice Prompts resetting is a bug they are looking into. halo spots
the reset in the status block and writes the user's settings back.

## EQ (`05`)

Payload, 50 bytes: two identical 25-byte banks.

| offset | example | meaning |
|---|---|---|
| 0–9 | `002d 00fa 03e8 0fa0 2710` | five band centre frequencies, big-endian Hz: 45, 250, 1k, 4k, 10k |
| 10–14 | `00 × 5` | probably the band gains (all zero on a flat EQ); unconfirmed |
| 15–19 | `14 × 5` | Q per band in tenths (2.0) |
| 20–24 | `00 × 5` | unknown |

## Still to capture

EQ writes, sidetone, mic level, the sleep timer, and the base lights. CORE
never changed these in the logs halo was built from. [capturing.md](capturing.md)
explains how to record them.
