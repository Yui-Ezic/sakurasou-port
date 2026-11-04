#!/usr/bin/env bash
# Builds a portable Windows folder: dist/Sakurasou-win64/ (exe + DLLs + Russian translation).
# Needs MSYS2 with the UCRT64 packages: gcc cmake ninja pkgconf SDL2 ffmpeg ntldd.
# Run from Git Bash or an MSYS2 shell at the repository root:  bash tools/package_win.sh
set -euo pipefail
UCRT=${UCRT:-/c/msys64/ucrt64}
export PATH="$UCRT/bin:$PATH"
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT="$ROOT/dist/Sakurasou-win64"

cmake -S "$ROOT/port" -B "$ROOT/port/build-win" -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C "$ROOT/port/build-win"
cmake -S "$ROOT/port" -B "$ROOT/port/build-con" -G Ninja -DCMAKE_BUILD_TYPE=Release -DSAKURA_CONSOLE=ON
ninja -C "$ROOT/port/build-con"

PYTHONUTF8=1 python "$ROOT/tools/tl_build.py" >/dev/null   # refresh translation/build/ru.pack from translation/ru

rm -rf "$OUT"; mkdir -p "$OUT/translation"
cp "$ROOT/port/build-win/sakurasou.exe" "$OUT/Sakurasou.exe"
cp "$ROOT/port/build-con/sakurasou.exe" "$OUT/Sakurasou-console.exe"
strip "$OUT"/*.exe

# Every DLL the exe pulls from the MSYS2 prefix (system DLLs stay out).
ntldd -R "$OUT/Sakurasou.exe" | tr '\\' '/' | grep -i "msys64/ucrt64/bin" | sed -E 's/.*=> *([^ ]+).*/\1/' | sort -u |
while read -r dll; do cp "$(cygpath -u "$dll")" "$OUT/"; done

# Translation: the pack, redrawn art, and the language choice (Russian by default). Skip stray "name (2).ext" copies.
cp "$ROOT/translation/build/ru.pack" "$OUT/translation/"
printf 'ru\n' > "$OUT/translation/lang.txt"
(cd "$ROOT/translation/build" && find assets -type f ! -name '* (*)*' -print0 | xargs -0 -I{} cp --parents {} "$OUT/translation/")

cp "$ROOT/port/third_party/clhca/COPYING" "$OUT/clhca-COPYING.txt"
cp "$ROOT/port/win/README-windows.txt" "$OUT/README.txt"
echo "Packaged: $OUT ($(du -sh "$OUT" | cut -f1), $(ls "$OUT"/*.dll | wc -l) DLLs)"
