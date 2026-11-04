# さくら荘のペットな彼女: native PC port

A native reimplementation of the PSP game's engine in C++20 with SDL2. There is no emulation: the game's own scripts run on a reimplemented VM, and all assets load directly from your unmodified ISO at runtime. Nothing from the game is included here.

## Build
Needs CMake, Ninja, a C++20 compiler, SDL2 2.0.18+ and FFmpeg libraries (libavcodec, libavformat, libavutil, libswscale, libswresample). FFmpeg decodes the opening and ending movies.

    cmake -S port -B port/build -G Ninja
    ninja -C port/build

### Windows
Install [MSYS2](https://www.msys2.org/) (`winget install MSYS2.MSYS2`), then in it:

    pacman -S --needed mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,pkgconf,SDL2,ffmpeg,ntldd}

From Git Bash or MSYS2 at the repository root, `bash tools/package_win.sh` builds `dist/Sakurasou-win64/`: `Sakurasou.exe` (no console),
`Sakurasou-console.exe` (with console, for logs and developer modes), the DLLs, and the Russian translation. The folder is portable;
the player drops their ISO next to the exe. A `translation/ru.pack` next to the exe loads automatically (`--no-tl` turns that off).
Add `-DSAKURA_CONSOLE=ON` to the cmake call for a console build by hand.

## Run
    port/build/sakurasou --iso path/to/sakuraso-no-pet-na-kanojo-psp.iso

Add `--tl translation/build` to play with the Russian translation; see `translation/README.md`.

`--iso` can be omitted when the ISO is in the current directory or a parent. Saves go to the SDL per-user folder (Linux: `~/.local/share/Sakurasou/SakurasouPort/`, Windows: `%APPDATA%\Sakurasou\SakurasouPort\`), or to `SAKURA_SAVE_DIR` if set.

## Controls
| PSP | Keyboard | Mouse | Gamepad |
|---|---|---|---|
| ○ confirm / advance | Z, Enter, Space | Left click | A |
| × cancel | X, Esc, Backspace | Right click | B |
| □ | A | | X |
| △ menu (in scenes) | S | | Y |
| L (held: hide UI) | Q, PageUp | | LB |
| R (held: skip) | W, PageDown, Left Ctrl | | RB |
| SELECT backlog | Tab | | Back |
| START auto mode | F | | Start |
| D-pad | Arrows | | D-pad, left stick |

F11 or Alt+Enter toggles fullscreen. The picture scales to any window size at the original aspect ratio.

## Developer modes
`--play SCN_x.ssb`, `--scan [filter]` (runs every scenario through the real ADV at skip speed), `--headless --frames N --shot-every N DIR --keys "frame:mask,..."`, `--test-audio`, `--test-vm`, `--test-image`, `--test-movie`. Use `SDL_VIDEODRIVER=dummy` for headless runs.

## Third party
`third_party/clhca` is the HCA decoder from vgmstream; its licence is in `third_party/clhca/COPYING`.
