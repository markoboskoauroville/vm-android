# LESSONS: vm-android

- **Oracle A1 VMs have no KVM** (dmesg "kvm: HYP mode not available"): no Android emulator, no Cuttlefish, no nested VM.
  Android in a container (redroid) needs only binder_linux + binderfs, which the Ubuntu oracle kernel ships as modules.
- **redroid splits boot arguments on spaces**: `"ro.product.model=Pixel 7"` became "Pixel". Identity goes into the
  build.prop files of a derived image instead (pixel7/build.sh), not into docker run arguments.
- **ro.build.fingerprint is assembled by init** from brand/name/device/id/incremental/type/tags when no file sets it;
  pin it explicitly in /system/build.prop or it keeps redroid's build id.
- **`monkey -p pkg -c LAUNCHER 1` can exit -5 right after an install**; resolve the activity with
  `cmd package resolve-activity --brief -a MAIN -c LAUNCHER pkg` and `am start -W -n` it.
- **Two processes, one phone**: a threading.Lock does not cover the mailbox runner; use flock on a shared file.

## 5.11.2026
- Ubuntu's scrcpy 1.25 shows Android 14+ but its control thread dies (NoSuchMethod/NullPointer in the server): no
  clicks. Use a current scrcpy; there is no arm64 Linux build upstream, so GitHub's ubuntu-24.04-arm runner builds it
  (scrcpy 4+ needs SDL3: build SDL3 static first).
- redroid 16 needs ashmem for gralloc and SystemServer; `androidboot.use_memfd=true` is not enough on kernel 7.0. 15 runs.
- Android's Wireless debugging switch needs Wi-Fi and turns itself off without it; `adb tcpip 5555` over USB listens on
  every interface (Tailscale over mobile data too) until a reboot. Only root keeps it across reboots.
- `pkill -f <pattern>` inside `ssh host '...'` matches the ssh command itself and kills it (exit 255).
- Tailscale's HostName is the display name ("Nothing Phone (2a)"); the short name is the first label of DNSName.
