# Sakurasou PSP -> native PC port

Reverse engineering "Sakurasou no Pet na Kanojo" (PSP) to build a native PC port.

## Files
- `sakuraso-no-pet-na-kanojo-psp.iso` - game image (do not modify)
- `extracted/EBOOT.BIN` - original encrypted (`~PSP`) executable, `PARAM.SFO`
- `extracted/EBOOT.BIN.dec` - decrypted ELF, imported into Ghidra (decrypted with `pspdecrypt`)
- ISO assets: `PSP_GAME/USRDIR/DATA.CPK` (~100MB), `STRM.CPK` (~590MB) - CRI CPK archives (not yet analyzed)
- `ghidra/` - Ghidra project (if present)

## Tooling (user-space installs, no sudo)
- Ghidra 12.1.4 at `~/tools/ghidra/ghidra_12.1.4_PUBLIC`, JDK 21 at `~/tools/jdk`. Launch with `ghidra` (wrapper in `~/.local/bin`; needs XWayland, sets `_JAVA_AWT_WM_NONREPARENTING=1`).
- Extensions: ghidra-allegrex (PSP CPU), GhidraMCP 2.0 (plugin listens on :8080 while a program is open in CodeBrowser).
- `pspdecrypt` at `~/.local/bin` (built from `~/tools/src/pspdecrypt`).
- MCP servers (user scope): `ghidra` (-> http://127.0.0.1:8080, requires CodeBrowser open with EBOOT.BIN.dec), `ppsspp` (mcp-ppsspp, PPSSPP_PORT=36203).
- Permissions pre-approved in `.claude/settings.json`.

## PPSSPP
- Version 1.20.4, `RemoteDebuggerOnStartup = True`; debugger WebSocket port **36203** (stable across launches).
- Launch on Wayland: `SDL_VIDEODRIVER=wayland setsid PPSSPPSDL <iso> &` (or `ppsspp-debug`). Plain launch fails: no X display.
- The debugger API cannot save/load savestates; use the PPSSPP UI.

## Ghidra notes
- Program: `EBOOT.BIN.dec`, language `Allegrex:LE:32:default`, ~10.4k functions, image base 0x0.
- Import list is empty and image base is 0: the PRX is relocatable. PPSSPP loads it at a runtime base (typically ~0x08804000), so Ghidra addresses need an offset to match runtime addresses from the ppsspp MCP. Verify the base before correlating.
- Ghidra GUI must be running for the `ghidra` MCP to work.

## Build tools for the port
cmake, ninja, clang, SDL2 2.32 installed.

## Port status (keep updated)
- Russian translation in progress on branch `translation-ru`: plan and pilot results in `docs/TRANSLATION_PLAN.md`, workflow in `translation/README.md`. Text is swapped at display time (`port/src/tr.cpp`); never patch `.ssb` files. `translation/ru/` must stay free of Japanese text.
- Playable from boot to all six route endings; see `docs/DIFFERENCES.md` for what is tested, untested, and different. User docs: `port/README.md`.
- Source: `port/` (C++20, SDL2, libav*, vendored clHCA). Build: `cmake -S port -B port/build -G Ninja && ninja -C port/build`. Reads assets directly from the unmodified ISO.
- Dev: `port/build/sakurasou --scan [filter]`, `--play SCN_x.ssb`, `--headless --frames N --shot-every N DIR --keys "frame:mask,..."` with `SDL_VIDEODRIVER=dummy`.
- Knowledge: `docs/RE_NOTES.md` and subsystem specs `docs/spec_*.md`. `work/` is regenerable scratch (extracted data, full decompile `work/eboot_decomp.c`; regenerate with tools/ghidra_scripts/DumpAll.java through analyzeHeadless on a copy of the project, without -noanalysis).
- Tools: `tools/` cpk.py img.py ssbdis.py ssbpretty.py fn.py nat.py mips.py mesh.py montage.py ppdbg.py. Image work uses `~/tools/sakura-venv/bin/python`.
- Long-running commands must use the tool's background mode: `nohup ... &` is killed when the Bash call returns. `pkill -f`/`pgrep -f` patterns match their own shell.
