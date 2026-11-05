# TAKEOVER: vm-android

**State 5.11.2026 (session closed):** the Pixel 7 container runs **Android 15** (image mantra/pixel7:15, `A=15
pixel7/build.sh`; 16 does not boot on this kernel, no ashmem; Android 14 data kept in /srv/pixel7/data-android14-5.11.2026).
scrcpy 5.0.1 for arm64 is built by `.github/workflows/scrcpy-arm64.yml` (release scrcpy-v5.0.1). adb over Tailscale:
pixel7d `?device=` / `X-Device` + `GET /devices`, jobs `"device"`, Mac `phones enable|list|connect` and
`pixel7 --on <phone>`. Marko's Pixel 7 (pixel-7, 100.70.23.53) has adb on 5555 since 5.11.2026 ~10:07, waiting for his
"Always allow" of the VM's key (fingerprint F1:8A:D5:…:02:81, checked). Not installed (the auto-mode check refused,
waiting for Marko): the Mac login agent phones-keeper (re-enables adb on a cabled phone after a reboot) and the VM timer
phones-connect.


**State 4.11.2026 (v1):** the Pixel 7 runs on chromium-vm (container `pixel7`, image `mantra/pixel7:14`), services
`pixel7d` (:8897 Tailscale) and `pixel7-jobs` (mailbox vm-android-jobs) active. Verified: identity props, 1080x2400/420,
`pixel7 test` with Mantra Remote VNC v22 (installed, launched, screenshot), and a mailbox job with a tap step
(28 s round trip, Settings opened in the shot). Not yet verified: a full VM reboot (binder module + docker restart policy +
services are in place but the machine was not rebooted, so Chrome and the workers were not interrupted).

**Next:** reboot test at a quiet moment; maybe Google Play services (MindTheGapps for redroid) if an app needs them;
Android 15/16 image when redroid publishes one that boots on this kernel.
