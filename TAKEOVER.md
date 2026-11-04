# TAKEOVER: vm-android

**State 4.11.2026 (v1):** the Pixel 7 runs on chromium-vm (container `pixel7`, image `mantra/pixel7:14`), services
`pixel7d` (:8897 Tailscale) and `pixel7-jobs` (mailbox vm-android-jobs) active. Verified: identity props, 1080x2400/420,
`pixel7 test` with Mantra Remote VNC v22 (installed, launched, screenshot), and a mailbox job with a tap step
(28 s round trip, Settings opened in the shot). Not yet verified: a full VM reboot (binder module + docker restart policy +
services are in place but the machine was not rebooted, so Chrome and the workers were not interrupted).

**Next:** reboot test at a quiet moment; maybe Google Play services (MindTheGapps for redroid) if an app needs them;
Android 15/16 image when redroid publishes one that boots on this kernel.
