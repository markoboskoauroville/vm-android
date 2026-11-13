#!/usr/bin/env bash
# Builds Mantra Bus for macOS from BlackHole (GPL-3, ExistentialAudio/BlackHole):
#   MantraBusSend.driver    device "Mantra Bus Send",   64 channels: Ableton plays into it, Mantra SonoBus reads it
#   MantraBusReturn.driver  device "Mantra Bus Return", 64 channels: Mantra SonoBus plays into it, Ableton records it
# Two drivers, two separate loopbacks, so what you receive never goes back out.
#
#   mac/build-driver.sh <BlackHole checkout> <output dir>
# Runs on a Mac with Xcode (GitHub's macos runner). Ad-hoc signed.
set -euo pipefail

BH="$(cd "$1" && pwd)"
OUT="$(mkdir -p "$2" && cd "$2" && pwd)"

build_one () {
  local kind="$1" uuid="$2"
  local lower; lower="$(echo "$kind" | tr '[:upper:]' '[:lower:]')"
  local bundle="com.mantra.sonobus.bus.$lower"
  local work; work="$(mktemp -d)"
  cp -R "$BH/." "$work/"

  # the names and sizes go in as #defines at the top of the source (no shell-quoting games)
  {
    echo "#define kDriver_Name \"MantraBus$kind\""
    echo "#define kPlugIn_BundleID \"$bundle\""
    echo "#define kHas_Driver_Name_Format false"
    echo "#define kDevice_Name \"Mantra Bus $kind\""
    echo "#define kDevice2_Name \"Mantra Bus $kind Mirror\""
    echo "#define kManufacturer_Name \"Mantra\""
    echo "#define kNumber_Of_Channels 64"
    echo "#define kEnableVolumeControl false"
    echo "#define kCanBeDefaultSystemDevice false"
    cat "$work/BlackHole/BlackHole.c"
  } > "$work/BlackHole/BlackHole.c.new"
  mv "$work/BlackHole/BlackHole.c.new" "$work/BlackHole/BlackHole.c"

  (cd "$work" && xcodebuild -project BlackHole.xcodeproj -configuration Release -target BlackHole \
      CONFIGURATION_BUILD_DIR="$work/build" PRODUCT_BUNDLE_IDENTIFIER="$bundle" \
      ARCHS="arm64 x86_64" ONLY_ACTIVE_ARCH=NO \
      CODE_SIGN_IDENTITY="-" CODE_SIGNING_REQUIRED=NO DEVELOPMENT_TEAM="" > "$work/xcodebuild.log" 2>&1) \
    || { tail -40 "$work/xcodebuild.log"; exit 1; }

  local drv="$OUT/MantraBus$kind.driver"
  rm -rf "$drv"
  mv "$work/build/BlackHole.driver" "$drv"
  # each driver needs its own plug-in factory id
  sed -i '' "s/e395c745-4eea-4d94-bb92-46224221047c/$uuid/g" "$drv/Contents/Info.plist"
  codesign --force --deep --sign - "$drv"
  echo "built $drv"
  rm -rf "$work"
}

build_one Send   cc28daa1-fb56-479b-8ec5-4920b49d1a5e
build_one Return 05ef59bb-10e6-4f07-86de-1294cb46318f
