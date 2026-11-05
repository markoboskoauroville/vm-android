# vm-android: a Pixel 7 in Oracle Cloud

Android 15 (arm64; Android 14 until 5.11.2026) runs on **chromium-vm** (Oracle A1, 130.61.181.83, Tailscale 100.102.86.30) beside Ubuntu, in a
redroid container dressed as a **Google Pixel 7**: model "Pixel 7", device panther, board gs201, the Pixel 7 Android 14
fingerprint, 1080x2400, 420 dpi. Chrome and everything else on the machine keeps running. Marko's words:
[momentaryupdates.md](momentaryupdates.md).

**Why not a dual boot** (asked 4.11.2026): the VM has no /dev/kvm ("HYP mode not available", Oracle gives no nested
virtualisation), a real Pixel 7 system only boots on Tensor G2 hardware, and a reboot into Android would cut Tailscale,
Chrome and the workers. Marko chose "Android beside Ubuntu". The kernel's binder_linux/binderfs modules let Android run
natively (no emulation): it boots in about 12 s and uses ~0.7 GB of RAM (capped at 3 GB).

## Three ways in

| Who | Door | How |
|---|---|---|
| The Mac (and anything on the tailnet) | **pixel7d**, http://100.102.86.30:8897, header `X-Mantra-Token` | `pixel7 health / test <apk> / shot / tap / log / shell / reset` (mac/pixel7 → ~/.local/bin/pixel7; token ~/Downloads/API/pixel7-vm.txt) |
| A cloud session (not on the tailnet) | the private repo **markoboskoauroville/vm-android-jobs** | push `jobs/<id>/job.json` + `app.apk`, pull `results/<id>/` (result.json, screen.png, shots, logcat.txt); ~30 s round trip. Its README is the manual. The session needs that repo in its GitHub access, nothing else. |
| Eyes and hands | adb on 100.102.86.30:5555 (Tailscale only) | `pixel7 screen` = scrcpy live on the Mac; `adb connect 100.102.86.30:5555` for Android Studio |

A cloud session that has a Tailscale auth key (see vm-chromium README) can use pixel7d directly too.

## The parts

- `pixel7/build.sh`: builds `mantra/pixel7:14` from `redroid/redroid:14.0.0-latest`, rewriting every identity line of
  the five build.prop files (and pinning ro.build.fingerprint, which init would otherwise assemble from redroid parts).
- `pixel7/run.sh`: (re)starts the container `pixel7` (restart unless-stopped, /data in /srv/pixel7/data);
  `PIXEL7_FRESH=1` = factory reset. binder_linux loads at boot (/etc/modules-load.d/redroid.conf).
- `api/pixel7d.py`: the door (systemd `pixel7d`), routes listed in its docstring and at GET /api.
  `/test` = install (-r -g) + clear log + launch (resolved launcher activity) + wait + crash/ANR check + screenshot + log.
- `api/jobrunner.py`: the mailbox runner (systemd `pixel7-jobs`, clone ~/vm-android-jobs, deploy key ~/.ssh/pixel7_jobs
  with write access, host alias github-pixel7-jobs). Steps: tap, swipe, wait, text, key, shell, shot.
- One phone, one user: a file lock (~/.cache/pixel7/device.lock) serialises the door and the runner.

## Install on a fresh machine

    tar czf - pixel7 api | ssh chromium-vm 'mkdir -p ~/vm-android && tar xzf - -C ~/vm-android'
    ssh chromium-vm 'sudo apt-get install -y docker.io adb aapt && cd ~/vm-android && sudo ./pixel7/build.sh && sudo ./pixel7/run.sh && sudo ./api/install.sh'
    ssh chromium-vm 'cat ~/.config/pixel7/token' > ~/Downloads/API/pixel7-vm.txt

## Limits

No Google Play services (no Google sign-in, FCM, Maps SDK); no real camera, microphone, GPS, Bluetooth, NFC or sensors;
software rendering (no GPU), so games and video are slow; arm64-v8a/armeabi-v7a apps only. A real thumb on a real phone
still decides how a gesture feels.

## 5.11.2026: Android 15, scrcpy 5, the fourth VM screen

Marko: "Running some old emulation from Android, and I cannot click anything ... make the latest Android version there".
- Android 16 (redroid 16.0.0) does not run on this kernel: its gralloc and SystemServer need ashmem, kernel 7.0 has none,
  and `androidboot.use_memfd=true` does not cover gralloc ("output buffer not gpu writeable", surfaceflinger restarts).
  Android 15 (redroid 15.0.0-latest, `A=15 pixel7/build.sh`, image mantra/pixel7:15) boots in about 18 s. The Android 14
  data is kept in /srv/pixel7/data-android14-5.11.2026.
- Ubuntu's scrcpy 1.25 crashed its input thread on Android 14+ (no clicks). `.github/workflows/scrcpy-arm64.yml` builds
  the newest scrcpy (5.0.1, SDL3 static) on GitHub's arm64 runner; release scrcpy-v5.0.1, unpacked into /opt/scrcpy.
- The VM shows it as its fourth screen (vm-chromium desktop: chromium-vm-pixel7 + chromium-vm-pixel7-bar with
  PORTRAIT | LANDSCAPE and REBOOT).

## 5.11.2026: your own phones over Tailscale

Marko: "test applications inside the virtual machine emulator for Pixel 7, and also on any of my devices connected to
the network through Tail Scale ... working in a virtual machine of Oracle all the time and accessing my phone".
- pixel7d takes `?device=NAME` (or header `X-Device`) on every route: `pixel7` (default, the container) or an Android
  device on the tailnet by its short Tailscale name (`nothing-phone-2a`, `pixel-7`) or ip:port. One lock per device.
  `GET /devices` lists them: online, adb reachable, model, Android. `/reset` is refused for real phones.
- A cloud job's job.json takes `"device": "nothing-phone-2a"` the same way (pixel7-jobs).
- Mac: `pixel7 --on <phone> <command>` and `phones enable | list | connect` (mac/phones → ~/.local/bin/phones).
- **How a phone answers adb over Tailscale with no Wi-Fi:** Android's "Wireless debugging" switch needs Wi-Fi and
  turns itself off without it (Android's rule, not Tailscale's). The classic `adb tcpip 5555` does not: once over USB
  (`phones enable`), adbd listens on every network the phone has, Tailscale over mobile data included, until the phone
  reboots. The first connection from the VM asks "Allow USB debugging?" on the phone: "Always allow from this
  computer" (adb's own key check stays on, so the open port is not open to anyone). Without a cable: join any Wi-Fi
  for a minute (another phone's hotspot does), turn on Wireless debugging, pair, run `adb tcpip 5555` through it, then
  leave the Wi-Fi. Kept across reboots only with root (`persist.adb.tcp.port`).
