# vm-android: a Pixel 7 in Oracle Cloud

Android 14 (arm64) runs on **chromium-vm** (Oracle A1, 130.61.181.83, Tailscale 100.102.86.30) beside Ubuntu, in a
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
