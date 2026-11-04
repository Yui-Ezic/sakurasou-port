#!/usr/bin/env bash
# Builds a portable Windows folder: dist/Sakurasou-win64/ (exe + DLLs + Russian translation, from translation/release).
# Needs MSYS2 with the UCRT64 packages: gcc cmake ninja pkgconf SDL2 ffmpeg ntldd.
# Run from Git Bash or an MSYS2 shell at the repository root:  bash tools/package_win.sh
set -euo pipefail
UCRT=${UCRT:-${MSYSTEM_PREFIX:-/c/msys64/ucrt64}}
export PATH="$UCRT/bin:$PATH"
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT="$ROOT/dist/Sakurasou-win64"

cmake -S "$ROOT/port" -B "$ROOT/port/build-win" -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C "$ROOT/port/build-win"
cmake -S "$ROOT/port" -B "$ROOT/port/build-con" -G Ninja -DCMAKE_BUILD_TYPE=Release -DSAKURA_CONSOLE=ON
ninja -C "$ROOT/port/build-con"

rm -rf "$OUT"; mkdir -p "$OUT"
cp "$ROOT/port/build-win/sakurasou.exe" "$OUT/Sakurasou.exe"
cp "$ROOT/port/build-con/sakurasou.exe" "$OUT/Sakurasou-console.exe"
strip "$OUT"/*.exe

# Every DLL the exe pulls from the MSYS2 prefix (system DLLs stay out).
ntldd -R "$OUT/Sakurasou.exe" | tr '\\' '/' | grep -i "ucrt64/bin" | sed -E 's/.*=> *([^ ]+).*/\1/' | sort -u |
while read -r dll; do cp "$(cygpath -u "$dll")" "$OUT/"; done

"$ROOT/tools/stage_release.sh" "$OUT" windows
echo "Packaged: $OUT ($(du -sh "$OUT" | cut -f1), $(ls "$OUT"/*.dll | wc -l) DLLs)"
(cd "$ROOT/dist" && { command -v zip >/dev/null && zip -qr Sakurasou-win64.zip Sakurasou-win64 || powershell -NoProfile -Command "Compress-Archive -Force Sakurasou-win64 Sakurasou-win64.zip"; })
