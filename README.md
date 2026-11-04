# Sakurasou no Pet na Kanojo — PC port

A native Windows and Linux version of the PSP visual novel **さくら荘のペットな彼女** (*Sakurasou no Pet na Kanojo*), with an optional **Russian translation**. It is a from-scratch reimplementation of the game engine, not an emulator: it runs the game's own scripts, reading everything from **your own copy of the game**.

> **You need to own the game.** No game data is included or distributed here. You supply the PSP disc image (`.iso`) of *Sakurasou no Pet na Kanojo*.

## Play (no technical knowledge needed)

1. **Download** the latest release from the [Releases page](../../releases/latest):
   - Windows: `Sakurasou-win64.zip`
   - Linux: `Sakurasou-linux-x86_64.tar.gz`
2. **Unpack** it anywhere.
3. **Start** `Sakurasou.exe` (Windows) or `Sakurasou.sh` (Linux).
4. The first time, the game asks for your game file: **drag your `.iso` onto the window that opens** (or put the `.iso` in the same folder as the game, or drag it onto `Sakurasou.exe`). It is remembered after that.

That's it. The game starts in Russian; **Settings → Language** switches to Japanese and back.

### Controls

| PSP | Keyboard | Mouse | Gamepad |
|---|---|---|---|
| ○ confirm / advance | Z, Enter, Space | Left click | A |
| × cancel | X, Esc, Backspace | Right click | B |
| □ | A | | X |
| △ menu (in scenes) | S, Esc | | Y |
| L (hold: hide the text box) | Q, PageUp | | LB |
| R (hold: skip) | W, PageDown, Left Ctrl | | RB |
| SELECT: backlog | Tab | | Back |
| START: auto mode | F | | Start |
| D-pad | Arrows | | D-pad, left stick |

In a scene, once the text has finished and the game is waiting for you, Esc opens the menu (like S) instead of advancing; everywhere else Esc is cancel. F11 or Alt+Enter toggles fullscreen; the picture scales to any window size.

### Saves

Windows: `%APPDATA%\Sakurasou\SakurasouPort\` · Linux: `~/.local/share/Sakurasou/SakurasouPort/`

### Problems?

- *"Game image needed" keeps appearing*: the file must be the PSP image of this game (about 1.5 GB). A `.cso` or another game will not work. Convert a CSO to ISO with PPSSPP or `maxcso`.
- *Windows warns about an unknown publisher*: the program is not code-signed. Choose **More info → Run anyway**.
- *Something looks wrong or crashes*: on Windows start `Sakurasou-console.exe` (the same game with a log window); on Linux start it from a terminal. Then open an [issue](../../issues) with the log.
- *A translated line is wrong*: press **F8** while it is on screen, then send `translation/feedback.tsv` with an issue.

What is tested and what differs from the PSP original: [docs/DIFFERENCES.md](docs/DIFFERENCES.md). Status: playable from the title screen through all six route endings.

## Building from source

Needs CMake, Ninja, a C++20 compiler, SDL2 (2.0.18+) and FFmpeg libraries (libavcodec, libavformat, libavutil, libswscale, libswresample).

    cmake -S port -B port/build -G Ninja
    ninja -C port/build
    port/build/sakurasou --iso path/to/game.iso --tl translation/release

Release packages are built by [GitHub Actions](.github/workflows/release.yml) with `tools/build_linux.sh` (static SDL2 + minimal FFmpeg) and `tools/package_win.sh` (MSYS2). More: [port/README.md](port/README.md) for developer modes, [docs/](docs/) for the reverse-engineering notes and subsystem specifications, [translation/README.md](translation/README.md) for working on the translation.

## Licence

GNU General Public License, version 3 or (at your option) any later version; see [LICENSE](LICENSE). Third-party components: [THIRD_PARTY.md](THIRD_PARTY.md). The game itself and its assets are © their owners and are not part of this project. This is an unofficial fan project, not affiliated with the rights holders.
