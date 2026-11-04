#!/usr/bin/env bash
# Shared by the Windows and Linux packaging: copies the translation, licences and the player README into a release folder.
# Usage: stage_release.sh <folder> <linux|windows>
set -euo pipefail
OUT=$1; PLATFORM=$2
ROOT=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$OUT/translation"
cp "$ROOT/translation/release/ru.pack" "$OUT/translation/"
printf 'ru\n' > "$OUT/translation/lang.txt"
# Redrawn menu art is optional; it only exists in a checkout that has generated or tracked it.
for A in "$ROOT/translation/release/assets" "$ROOT/translation/build/assets"; do
  if [ -d "$A" ]; then
    mkdir -p "$OUT/translation/assets"; cp -r "$A/." "$OUT/translation/assets/"
    find "$OUT/translation/assets" -name '* (*)*' -delete     # stray "name (2).ext" copies
    break
  fi
done
cp "$ROOT/LICENSE" "$OUT/LICENSE.txt"
cp "$ROOT/port/third_party/clhca/COPYING" "$OUT/clhca-COPYING.txt"
cp "$ROOT/THIRD_PARTY.md" "$OUT/THIRD_PARTY.txt"
if [ "$PLATFORM" = linux ]; then
  cp "$ROOT/port/linux/README-linux.txt" "$OUT/README.txt"
  cp "$ROOT/port/linux/Sakurasou.sh" "$OUT/Sakurasou.sh"; chmod +x "$OUT/Sakurasou.sh" "$OUT/sakurasou"
else
  cp "$ROOT/port/win/README-windows.txt" "$OUT/README.txt"
fi
