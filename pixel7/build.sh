#!/bin/bash
# Builds the image mantra/pixel7:<A> on chromium-vm: redroid <A> (arm64) with every identity property of a Pixel 7
# (brand google, Google, "Pixel 7", panther, a Pixel 7 fingerprint). Run as root on the VM.
# 5.11.2026 (Marko: "make the latest Android version there"): Android 16 by default (redroid 16.0.0-latest). The build ID
# BP2A.250605.031.A2 is the Pixel 7's Android 16 (June 2025) release; its incremental number was not found, so the
# fingerprint carries the redroid image's own incremental. A=14 still builds the old image.
set -euo pipefail
A=${A:-16}
BASE=${BASE:-redroid/redroid:$A.0.0-latest}
OUT=${OUT:-mantra/pixel7:$A}
W=$(mktemp -d); trap 'rm -rf "$W"; docker rm -f pixel7-props >/dev/null 2>&1 || true' EXIT
docker pull -q "$BASE" >/dev/null
docker create --name pixel7-props "$BASE" >/dev/null
if [ "$A" = 14 ]; then BID=UQ1A.240205.004; INC=11269751
else
  # 16: the Pixel 7's own Android 16 build ID; any other version: the redroid image's own build ID
  if [ "$A" = 16 ]; then BID=BP2A.250605.031.A2
  else BID=$(docker cp -L pixel7-props:/system/build.prop - | tar xO | sed -n 's/^ro\.build\.id=//p' | head -1); fi
  INC=$(docker cp -L pixel7-props:/system/build.prop - | tar xO | sed -n 's/^ro\.build\.version\.incremental=//p' | head -1)
fi
FP="google/panther/panther:$A/$BID/$INC:user/release-keys"
DESC="panther-user $A $BID $INC release-keys"
FILES="/system/build.prop /vendor/build.prop /system/product/etc/build.prop /system/system_ext/etc/build.prop /vendor/odm/etc/build.prop"
echo "FROM $BASE" > "$W/Dockerfile"
for f in $FILES; do
  n=$(echo "$f" | tr / _)
  docker cp -L "pixel7-props:$f" "$W/$n"
  sed -i -E \
    -e 's/^(ro\.product\.[a-z_]*\.?brand)=.*/\1=google/' \
    -e 's/^(ro\.product\.[a-z_]*\.?manufacturer)=.*/\1=Google/' \
    -e 's/^(ro\.product\.[a-z_]*\.?model)=.*/\1=Pixel 7/' \
    -e 's/^(ro\.product\.[a-z_]*\.?device)=.*/\1=panther/' \
    -e 's/^(ro\.product\.[a-z_]*\.?name)=.*/\1=panther/' \
    -e "s#^(ro\.([a-z_]+\.)?build\.fingerprint)=.*#\1=$FP#" \
    -e "s#^ro\.build\.description=.*#ro.build.description=$DESC#" \
    -e 's/^ro\.build\.product=.*/ro.build.product=panther/' \
    -e 's/^ro\.build\.flavor=.*/ro.build.flavor=panther-user/' \
    -e "s/^ro\.build\.display\.id=.*/ro.build.display.id=$BID/" \
    "$W/$n"
  echo "COPY $n $f" >> "$W/Dockerfile"
done
# extra Pixel 7 facts that are not in the image (init would otherwise compute the fingerprint from redroid parts)
echo "ro.build.fingerprint=$FP" >> "$W/_system_build.prop"
cat >> "$W/_vendor_build.prop" <<P
ro.product.board=gs201
ro.soc.manufacturer=Google
ro.soc.model=GS201
ro.boot.hardware.sku=GVU6C
P
docker build -q -t "$OUT" "$W"
echo "built $OUT"
