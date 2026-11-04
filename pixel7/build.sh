#!/bin/bash
# Builds the image mantra/pixel7:14 on chromium-vm: redroid 14 (arm64) with every identity property of a Pixel 7
# (brand google, Google, "Pixel 7", panther, the Pixel 7 Android 14 fingerprint). Run as root on the VM.
set -euo pipefail
BASE=${BASE:-redroid/redroid:14.0.0-latest}
OUT=${OUT:-mantra/pixel7:14}
W=$(mktemp -d); trap 'rm -rf "$W"; docker rm -f pixel7-props >/dev/null 2>&1 || true' EXIT
FP='google/panther/panther:14/UQ1A.240205.004/11269751:user/release-keys'
DESC='panther-user 14 UQ1A.240205.004 11269751 release-keys'
docker create --name pixel7-props "$BASE" >/dev/null
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
    -e 's/^ro\.build\.display\.id=.*/ro.build.display.id=UQ1A.240205.004/' \
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
