#!/usr/bin/env bash
# Builds the self-contained Linux release: dist/Sakurasou-linux-x86_64.tar.gz
#
# SDL2 and a minimal FFmpeg (the H.264 and ATRAC3+ decoders the game needs) are built from source and linked
# statically, so the player needs nothing installed. Build on an older distro (CI uses Ubuntu 22.04) so the result runs
# on newer ones. Packages needed on Debian/Ubuntu:
#   build-essential cmake ninja-build pkg-config curl xz-utils libx11-dev libxext-dev libxcursor-dev libxi-dev
#   libxrandr-dev libxss-dev libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev libasound2-dev
#   libpulse-dev libpipewire-0.3-dev libdrm-dev libgbm-dev libgl1-mesa-dev libegl1-mesa-dev libudev-dev libdbus-1-dev
set -euo pipefail
SDL_VER=${SDL_VER:-2.32.8}
FFMPEG_VER=${FFMPEG_VER:-7.1.1}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
DEPS=$ROOT/port/build-linux-deps          # downloaded sources + installed static libs (cached between runs)
PREFIX=$DEPS/prefix
OUT=$ROOT/dist/Sakurasou-linux-x86_64
JOBS=$(nproc)
mkdir -p "$DEPS" "$PREFIX"

if [ ! -f "$PREFIX/lib/libSDL2.a" ]; then
  [ -d "$DEPS/SDL2-$SDL_VER" ] || { curl -fL "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VER/SDL2-$SDL_VER.tar.gz" | tar xz -C "$DEPS"; }
  cmake -S "$DEPS/SDL2-$SDL_VER" -B "$DEPS/sdl-build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
        -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF -DSDL_TESTS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  ninja -C "$DEPS/sdl-build" install
fi

if [ ! -f "$PREFIX/lib/libavcodec.a" ]; then
  [ -d "$DEPS/ffmpeg-$FFMPEG_VER" ] || { curl -fL "https://ffmpeg.org/releases/ffmpeg-$FFMPEG_VER.tar.xz" | tar xJ -C "$DEPS"; }
  mkdir -p "$DEPS/ffmpeg-build" && cd "$DEPS/ffmpeg-build"
  "$DEPS/ffmpeg-$FFMPEG_VER/configure" --prefix="$PREFIX" --disable-shared --enable-static --enable-pic \
      --disable-programs --disable-doc --disable-autodetect --disable-x86asm --disable-network \
      --disable-everything --enable-decoder=h264,atrac3p,hevc --enable-parser=h264   # hevc only because h264 SEI code links against its film-grain helper
  make -j"$JOBS" install
  cd "$ROOT"
fi

export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
cmake -S "$ROOT/port" -B "$ROOT/port/build-linux" -G Ninja -DCMAKE_BUILD_TYPE=Release -DSAKURA_STATIC_DEPS=ON \
      -DCMAKE_PREFIX_PATH="$PREFIX"
ninja -C "$ROOT/port/build-linux"

rm -rf "$OUT"; mkdir -p "$OUT/translation"
cp "$ROOT/port/build-linux/sakurasou" "$OUT/sakurasou"
strip "$OUT/sakurasou"
"$ROOT/tools/stage_release.sh" "$OUT" linux
tar czf "$OUT.tar.gz" -C "$ROOT/dist" "$(basename "$OUT")"
echo "Packaged: $OUT.tar.gz ($(du -sh "$OUT.tar.gz" | cut -f1))"
ldd "$OUT/sakurasou" || true
