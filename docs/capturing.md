# Capturing new commands

halo only sends commands it has seen Glorious CORE send. To add a setting,
record what CORE sends when you change it.

CORE makes this easy: it logs every command it sends and every reply, as hex,
in `%APPDATA%\Glorious Core\logs\CORELog_<date>.log`:

```
[19:27:41.877]: Audio 5: 15 03 02 01 00
[19:27:41.921]: Received data from headset: 15 03 01 00 00 …
```

So no USB sniffer is needed, just CORE on Windows with the headset attached.

## 1. Get Windows to see the receiver

Either works:

- **A Windows PC.** Plug the receiver (or its base) in there.
- **A virtual machine with USB passthrough.** Omarchy's own
  `omarchy-windows-vm` doesn't pass USB devices through (its devices are
  fixed), so use virt-manager instead:

  ```sh
  omarchy-pkg-add virt-manager qemu-full libvirt dnsmasq
  sudo systemctl enable --now libvirtd
  ```

  Create a Windows 11 VM, then **Add Hardware → USB Host Device → Realtek GH
  Wireless InfinitePlay**. While it's attached, Linux loses the headset's
  audio; remove it from the VM to get it back.

## 2. Record one change at a time

1. Install Glorious CORE and let it find the headset.
2. Note the time, change **one** setting (one EQ band, sidetone level, sleep
   timer, light mode), and wait a few seconds. Repeat for each value, noting
   the time and value for each.
3. Copy the day's `CORELog_*.log`.

## 3. Pull out the commands

The log is large; only a few lines matter:

```sh
grep -E 'Audio 5: 15 .. 02|Received data from headset: 15 25' CORELog_*.log
```

Each `15 <cmd> 02 <len> <data>` line is a write. Add what you find to
[protocol.md](protocol.md), with the time and value you set, and open a pull
request or an issue.

**Don't post raw logs publicly.** They include the headset's serial number
(command `13`) and your Windows user name in paths.
