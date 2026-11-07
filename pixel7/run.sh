#!/bin/bash
# (Re)starts a Pixel 7 container on chromium-vm. adb answers on 127.0.0.1:$PORT and on the Tailscale address only.
# PIXEL7_FRESH=1 wipes its /data first (a factory reset).
# 7.11.2026 (Marko: "It needs to have two emulators for Android ... make two copies"): NAME and PORT pick the phone.
#   Pixel 1 = NAME=pixel7  PORT=5555 (the default; pixel7d and the jobs use it), data /srv/pixel7/data
#   Pixel 2 = NAME=pixel7b PORT=5556, data /srv/pixel7b/data (its own, fresh: a copy of Pixel 1's data would give
#             Google two phones with the same device ids)
set -euo pipefail
IMG=${IMG:-mantra/pixel7:15-gapps}   # 5.11.2026: + MindTheGapps 15 (Play Store), image built from gapps-dl/ctx; Android 15 since 5.11.2026 (16 needs ashmem, this kernel has none); :14 the old one
NAME=${NAME:-pixel7}
PORT=${PORT:-5555}
TS=$(tailscale ip -4 2>/dev/null | head -1 || true)
modprobe binder_linux devices=binder,hwbinder,vndbinder 2>/dev/null || true
docker rm -f "$NAME" >/dev/null 2>&1 || true
[ "${PIXEL7_FRESH:-0}" = 1 ] && rm -rf "/srv/$NAME/data"
mkdir -p "/srv/$NAME/data"
PORTS=(-p "127.0.0.1:$PORT:5555"); [ -n "$TS" ] && PORTS+=(-p "$TS:$PORT:5555")
docker run -d --name "$NAME" --hostname "$NAME" --privileged --restart unless-stopped --memory 3g \
  -v "/srv/$NAME/data:/data" "${PORTS[@]}" "$IMG" \
  androidboot.redroid_width=1080 androidboot.redroid_height=2400 androidboot.redroid_dpi=420 \
  androidboot.redroid_fps=60 androidboot.redroid_gpu_mode=guest \
  androidboot.use_memfd=true   # Android 16 (5.11.2026): this kernel has no ashmem, memfd instead >/dev/null
for i in $(seq 1 90); do
  adb connect "127.0.0.1:$PORT" >/dev/null 2>&1 || true
  [ "$(adb -s "127.0.0.1:$PORT" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = 1 ] && { echo "$NAME booted in ${i}x2 s"; exit 0; }
  sleep 2
done
echo "$NAME did not finish booting in 180 s" >&2; exit 1
