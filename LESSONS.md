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
