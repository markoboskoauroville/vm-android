#!/usr/bin/env bash
# Mantra SonoBus for macOS: the app (into /Applications) + Mantra Bus Send / Return (32-channel drivers).
#
#   curl -fsSL https://raw.githubusercontent.com/markoboskoauroville/MANTRA_SONOBUS/main/mac/install-mac.sh | bash
#
# or, from an unzipped release:  bash install-mac.sh
# Asks for the Mac password once (drivers live in /Library/Audio/Plug-Ins/HAL). Run it again to update.
set -euo pipefail

REPO="markoboskoauroville/MANTRA_SONOBUS"
here="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" 2>/dev/null && pwd || pwd)"

if [ ! -d "$here/Mantra SonoBus.app" ]; then
  tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
  url="$(curl -fsSL "https://api.github.com/repos/$REPO/releases/latest" \
        | grep -o 'https://[^"]*macos\.zip' | head -1)"
  [ -n "$url" ] || { echo "No macOS build in the latest release yet."; exit 1; }
  echo "Downloading $url"
  curl -fsSL "$url" -o "$tmp/m.zip"
  ditto -x -k "$tmp/m.zip" "$tmp"
  here="$(dirname "$(find "$tmp" -maxdepth 3 -name 'Mantra SonoBus.app' -type d | head -1)")"
fi

echo "Installing the app"
pkill -x "Mantra SonoBus" 2>/dev/null || true
rm -rf "/Applications/Mantra SonoBus.app"
ditto "$here/Mantra SonoBus.app" "/Applications/Mantra SonoBus.app"
xattr -dr com.apple.quarantine "/Applications/Mantra SonoBus.app" 2>/dev/null || true

echo "Installing Mantra Bus (needs your password)"
sudo mkdir -p /Library/Audio/Plug-Ins/HAL
for d in MantraBusSend MantraBusReturn; do
  sudo rm -rf "/Library/Audio/Plug-Ins/HAL/$d.driver"
  sudo ditto "$here/$d.driver" "/Library/Audio/Plug-Ins/HAL/$d.driver"
  sudo xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/HAL/$d.driver" 2>/dev/null || true
done
sudo killall -9 coreaudiod 2>/dev/null || true   # Core Audio restarts by itself and loads the drivers

echo "Done: /Applications/Mantra SonoBus.app, devices 'Mantra Bus Send' and 'Mantra Bus Return' (32 ch)."
open "/Applications/Mantra SonoBus.app"
