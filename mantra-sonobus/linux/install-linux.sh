#!/usr/bin/env bash
# Mantra SonoBus for Linux: the app + Mantra Bus (two 64-channel virtual devices).
#
#   curl -fsSL https://raw.githubusercontent.com/markoboskoauroville/MANTRA_SONOBUS/main/linux/install-linux.sh | bash
#
# or, from an unpacked release tarball:  ./install-linux.sh
# Installs into ~/.local (no root needed except for missing packages). Run it again to update.
set -euo pipefail

REPO="markoboskoauroville/MANTRA_SONOBUS"
BIN="$HOME/.local/bin"
SHARE="$HOME/.local/share/mantra-sonobus"
here="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" 2>/dev/null && pwd || pwd)"

case "$(uname -m)" in
  x86_64|amd64)  arch=x86_64 ;;
  aarch64|arm64) arch=arm64 ;;
  *) echo "Unsupported CPU $(uname -m)"; exit 1 ;;
esac

# 1. the files: next to this script, or the latest release
if [ ! -x "$here/mantra-sonobus" ]; then
  tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
  url="$(curl -fsSL "https://api.github.com/repos/$REPO/releases/latest" \
        | grep -o "https://[^\"]*linux-$arch\.tar\.gz" | head -1)"
  [ -n "$url" ] || { echo "No Linux $arch build in the latest release yet."; exit 1; }
  echo "Downloading $url"
  curl -fsSL "$url" | tar xz -C "$tmp"
  here="$(dirname "$(find "$tmp" -name mantra-sonobus -type f | head -1)")"
fi

# 2. libraries the app needs (Ubuntu/Debian names; skipped if apt is missing)
if command -v apt-get >/dev/null; then
  asound=libasound2t64; apt-cache show libasound2t64 >/dev/null 2>&1 || asound=libasound2
  need=()
  for p in libopus0 "$asound" libfreetype6 libx11-6 libxext6 libxinerama1 libxrandr2 libxcursor1 pipewire-jack; do
    dpkg -s "$p" >/dev/null 2>&1 || need+=("$p")
  done
  if [ "${#need[@]}" -gt 0 ]; then
    echo "Installing: ${need[*]}"
    sudo apt-get install -y "${need[@]}" || echo "(could not install ${need[*]}; the app may still run)"
  fi
fi

# 3. the app and a launcher that runs it through PipeWire's JACK, so it sees Mantra Bus by name
mkdir -p "$BIN" "$SHARE" "$HOME/.local/share/applications"
install -m 755 "$here/mantra-sonobus" "$SHARE/mantra-sonobus"
[ -f "$here/mantra-sonobus.png" ] && install -m 644 "$here/mantra-sonobus.png" "$SHARE/mantra-sonobus.png"
cat > "$BIN/mantra-sonobus" <<EOF
#!/usr/bin/env bash
if command -v pw-jack >/dev/null && pgrep -x pipewire >/dev/null; then
  exec pw-jack "$SHARE/mantra-sonobus" "\$@"
fi
exec "$SHARE/mantra-sonobus" "\$@"
EOF
chmod 755 "$BIN/mantra-sonobus"
cat > "$HOME/.local/share/applications/mantra-sonobus.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Mantra SonoBus
Comment=Audio between your machines over Tailscale
Exec=$BIN/mantra-sonobus
Icon=$SHARE/mantra-sonobus.png
Categories=AudioVideo;Audio;
Terminal=false
EOF

# 4. Mantra Bus
if pgrep -x pipewire >/dev/null; then
  mkdir -p "$HOME/.config/pipewire/pipewire.conf.d"
  install -m 644 "$here/mantra-bus.conf" "$HOME/.config/pipewire/pipewire.conf.d/mantra-bus.conf"
  systemctl --user restart pipewire pipewire-pulse 2>/dev/null || true
  echo "Mantra Bus Send / Mantra Bus Return: PipeWire devices, 64 channels each."
elif command -v pactl >/dev/null; then
  map=$(seq -s, -f 'aux%g' 0 63)
  for n in send return; do
    pactl list short sinks | grep -q "mantra_bus_$n" || \
      pactl load-module module-null-sink sink_name="mantra_bus_$n" channels=64 channel_map="$map" \
        sink_properties=device.description="Mantra\ Bus\ ${n^}" >/dev/null
  done
  mkdir -p "$HOME/.config/pulse"
  grep -q mantra_bus "$HOME/.config/pulse/default.pa" 2>/dev/null || {
    [ -f "$HOME/.config/pulse/default.pa" ] || echo ".include /etc/pulse/default.pa" > "$HOME/.config/pulse/default.pa"
    for n in send return; do
      echo "load-module module-null-sink sink_name=mantra_bus_$n channels=64 channel_map=$map sink_properties=device.description=Mantra_Bus_${n^}" >> "$HOME/.config/pulse/default.pa"
    done
  }
  echo "Mantra Bus Send / Mantra Bus Return: PulseAudio null sinks, 64 channels each."
else
  echo "No PipeWire or PulseAudio running: Mantra Bus not created (the app still works with real devices)."
fi

case ":$PATH:" in *":$BIN:"*) ;; *) echo "Add $BIN to PATH (or start it from the app menu)." ;; esac
echo "Installed. Start: mantra-sonobus"
