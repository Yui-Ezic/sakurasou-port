# Third-party software

This project is licensed under the GNU General Public License v3.0 or later (see `LICENSE`). It includes or links the following.

| Component | Used for | Licence |
|---|---|---|
| [clHCA](port/third_party/clhca) (from vgmstream) | Decoding the game's HCA audio | ISC-style permissive licence, text in `port/third_party/clhca/COPYING` |
| [SDL2](https://libsdl.org) | Window, input, audio output | zlib |
| [FFmpeg](https://ffmpeg.org) (libavcodec, libavformat, libavutil, libswscale, libswresample) | H.264 video and ATRAC3+ audio decoding for movies | LGPL 2.1 or later. Release builds are minimal static builds; the FFmpeg source is at https://ffmpeg.org/releases/ and the build options are in `tools/build_linux.sh`. On Windows the FFmpeg and SDL2 DLLs are shipped next to the executable and can be replaced |
| GCC runtime (libgcc, libstdc++), MSYS2 UCRT runtime | Windows/Linux runtime | GPL with the GCC Runtime Library Exception |

## The game
"Sakurasou no Pet na Kanojo" and all of its art, music, voices, scripts and text belong to their respective owners. None of it is included in this repository or in the releases; the program reads it from the player's own PSP disc image.

The Russian translation (`translation/`) is original text and is covered by this project's licence. It contains no Japanese source text.
