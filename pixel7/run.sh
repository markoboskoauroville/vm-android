#!/bin/bash
# (Re)starts the Pixel 7 container on chromium-vm. adb answers on 127.0.0.1:5555 and on the Tailscale address only.
# PIXEL7_FRESH=1 wipes /data first (a factory reset).
set -euo pipefail
IMG=${IMG:-mantra/pixel7:15}   # Android 15 since 5.11.2026 (16 needs ashmem, this kernel has none); :14 the old one
TS=$(tailscale ip -4 2>/dev/null | head -1 || true)
modprobe binder_linux devices=binder,hwbinder,vndbinder 2>/dev/null || true
docker rm -f pixel7 >/dev/null 2>&1 || true
[ "${PIXEL7_FRESH:-0}" = 1 ] && rm -rf /srv/pixel7/data
mkdir -p /srv/pixel7/data
PORTS=(-p 127.0.0.1:5555:5555); [ -n "$TS" ] && PORTS+=(-p "$TS:5555:5555")
docker run -d --name pixel7 --hostname pixel7 --privileged --restart unless-stopped --memory 3g \
  -v /srv/pixel7/data:/data "${PORTS[@]}" "$IMG" \
  androidboot.redroid_width=1080 androidboot.redroid_height=2400 androidboot.redroid_dpi=420 \
  androidboot.redroid_fps=60 androidboot.redroid_gpu_mode=guest \
  androidboot.use_memfd=true   # Android 16 (5.11.2026): this kernel has no ashmem, memfd instead >/dev/null
for i in $(seq 1 90); do
  adb connect 127.0.0.1:5555 >/dev/null 2>&1 || true
  [ "$(adb -s 127.0.0.1:5555 shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = 1 ] && { echo "pixel7 booted in ${i}x2 s"; exit 0; }
  sleep 2
done
echo "pixel7 did not finish booting in 180 s" >&2; exit 1
