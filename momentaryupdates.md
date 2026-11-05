# momentaryupdates — vm-android

## 4.11.2026 (Mac session "mac os session")

> Please access my virtual machine in the cloud, Oracle Cloud, and make it dual boot. One boot as it is now and Android phone boot. And Android phone boot needs to have a source code which allows you or cloud session directly, cloud session with giving certain data to log in to be able to test and send my applications to Oracle and then get results. Oracle should emulate Pixel Phone 7, boot it as actual Pixel Phone 7.

Found on chromium-vm (130.61.181.83, Tailscale 100.102.86.30): Ampere A1 arm64, 2 OCPU, 12 GB, 179 GB free;
no /dev/kvm ("HYP mode not available": Oracle gives no nested virtualisation), so no emulator or Cuttlefish with
acceleration; kernel has binder_linux + binderfs as modules, so Android in a container (redroid) can run natively.

## 5.11.2026

(From the vm-chromium session) "Android emulation is very bad ... I cannot click anything ... make the latest Android version there"
**Status:** Android 15 (16 cannot run without ashmem on this kernel), scrcpy 5.0.1 built by GitHub; clicks and swipes tested.

## 5.11.2026

"also, in the virtual machine, develop the ADB over Tail Scale so the local session can test applications locally and in the virtual machine session. The cloud session can test applications inside the virtual machine emulator for Pixel 7, and also on any of my devices connected to the network through Tail Scale. It can be nothing but a phone and Pixel 7 for now, but it can be more phones if my phone is available on Tail Scale. Then the local cloud code can access it and run the programs on my remote phone through ADB. If ADB is enabled, Tail Scale is installed, and we are on the same account, it can access my phone and test the application on my phone, working in a virtual machine of Oracle all the time and accessing my phone as it needs to access it"
**Status:** saved first.
