# Mantra SonoBus

Audio between your own machines over Tailscale: send, receive, listen and record, in any combination,
2 to 32 channels (or any custom count up to 64). Built on [SonoBus](https://github.com/sonosaurus/sonobus) (its audio engine, unchanged),
with one screen: a strip of VU meters.

- **The strip.** Left half is **SEND** (what leaves this machine, one bar per channel), right half is
  **HEAR** (what arrives). The captions name the machines: `SEND 16 > chromium-vm`, `HEAR 2 < macbook-pro`.
  On a computer the strip sits at the bottom of the screen, always on top; drag it to move it.
  On Android it is the bottom of the app.
- **Click the strip** for the panel:
  - **CHANNELS** 2 (stereo), 4, 8, 16, 24, 32, like the CQ mixer's stereo or multitrack USB mode, or type any
    count from 1 to 64 in the custom field.
  - **QUALITY** LIGHT / AUTO / HIGH / LOSSLESS. The bitrate per channel drops as the channel count
    rises (AUTO: 128 kbps/ch at 2 ch, 64 at 16, 48 at 32, so 32 channels are about 1.5 Mbit/s per link).
    The panel shows the total.
  - **SEND FROM** / **HEAR / RECORD TO**: the input and output devices (Mantra Bus, a mic, an interface).
  - **MACHINES**: every Mantra SonoBus on the tailnet, each with **SEND >** and **< RECEIVE**.
    RECEIVE works from this side alone: it asks the other machine to send. Links are remembered.
  - **REC TO FILES** records the mix, this machine, and each machine as its own multichannel file.

## Mantra Bus: the virtual driver

Two virtual devices with 64 channels each, like BlackHole:

| Device | Who plays into it | Who reads it |
|---|---|---|
| **Mantra Bus Send** | Ableton (or any app) | Mantra SonoBus → the other machines |
| **Mantra Bus Return** | Mantra SonoBus ← the other machines | Ableton records it |

Two devices so that what you receive never goes back out. Use as many of the 64 channels as the
CHANNELS setting says; channel N on one machine arrives as channel N on the other.

Example, multitrack from Ableton on the Mac to a recorder on the VM: on the Mac, Ableton output =
Mantra Bus Send (tracks on 1..16), Mantra SonoBus SEND FROM = Mantra Bus Send, CHANNELS 16, machine
chromium-vm SEND on. On the VM, Mantra SonoBus HEAR / RECORD TO = Mantra Bus Return, and the recorder
records the 16 channels of Mantra Bus Return (or press REC TO FILES there).
To listen and record at the same time on a Mac, make a Multi-Output Device (Audio MIDI Setup) of your
speakers + Mantra Bus Return and pick that as the output.

## Install

macOS (app + Mantra Bus drivers, asks for the password once):

    curl -fsSL https://raw.githubusercontent.com/markoboskoauroville/MANTRA_SONOBUS/main/mac/install-mac.sh | bash

Linux (x86_64 or arm64; app in ~/.local, Mantra Bus as PipeWire or PulseAudio devices):

    curl -fsSL https://raw.githubusercontent.com/markoboskoauroville/MANTRA_SONOBUS/main/linux/install-linux.sh | bash

Android: the `.apk` of the [latest release](https://github.com/markoboskoauroville/MANTRA_SONOBUS/releases/latest).
Android has no virtual drivers: it sends its mic or USB interface and plays to its speaker or interface.

## How machines find each other

Every Mantra SonoBus listens on UDP **11000** on its Tailscale address. They meet in two ways:
- **Meeting point** (default `100.102.86.30`, chromium-vm): the machine whose address it is runs the
  SonoBus server inside the app (UDP/TCP 10999); all the others join the group `mantra-sonobus` there and
  are linked to each other automatically. Change it in the panel; empty = direct links only.
- **Direct**: on Mac and Linux the panel lists the online Tailscale machines (`tailscale status`);
  LINK calls one directly. Any machine can also be added by name or 100.x address.
A link starts silent; nothing is sent until you switch SEND or RECEIVE on.

## Build

GitHub Actions builds everything (`.github/workflows/release.yml`) on every push to main and publishes
the release `v<VERSION>`. By hand, on Linux:

    git clone https://github.com/sonosaurus/sonobus && cd sonobus && git checkout 35f1062
    python3 ../MANTRA_SONOBUS/scripts/prepare.py .
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C build SonoBus_Standalone

`src/MantraEditor.*` is the whole interface; `scripts/prepare.py` swaps it in for SonoBus' editor
and renames the app (bundle id `com.mantra.sonobus`, so it lives beside a real SonoBus).

License: GPL-3.0 (SonoBus and BlackHole are GPL-3.0).
