# Sakurasou PSP (NPJH50745) reverse-engineering notes

Status tags: [CONFIRMED] = verified against data/binary; [ASSUMED] = inferred, untested.

## Containers
- [CONFIRMED] DATA.CPK 1682 files, STRM.CPK 19528 files. CRI CPK v7, TOC only, FileOffset is relative to TocOffset. Some files CRILAYLA.
- `tools/cpk.py` lists/extracts; `port/src/fs.cpp` reads directly out of the ISO.

## Images
- [CONFIRMED] TXG: `TXG\0, total, hdr(0x30), data, clutOff, imgOff, clutSize, imgSize, fmt, cfmt, w, h`. fmt 0=RGBA8888, 1=RGB565 (low bits = R), 4=CLUT8 (RGBA8888 clut). Always PSP-swizzled (16B x 8 lines).
- [CONFIRMED] XGTL: `XGTL, total, w, h, tileW, tileH, cols, rows, firstOff, tileStep, 0, 0` then TXG tiles.
- [CONFIRMED] CBG: `CBG\0, 0x10, xgtlOff(0x20), 0, cx, cy, ...` (cx,cy = image centre) wrapping XGTL.
- CSU: `CSU\0` sprite: XGTL body + overlay parts (eyes/mouth?) with small headers before a TXG.
- `.loc` files are plain TXG. Tool: `tools/img.py`.

## Fonts
- [CONFIRMED by size] `FNT\0, 1, cell(14|10), bpp 4`, u16 glyph-index table for U+0000..FFFF at 0x10, 8006 fixed cells at 0x20010 (cell^2/2 bytes).

## Audio/video
- [CONFIRMED] HCA v2, unencrypted (22050 Hz mono voice). ADX type 3 with loop info (BGM/SE). Decoder vendored: port/third_party/clhca.
- [CONFIRMED] PMF: H.264 480x272 + ATRAC3+ in private stream.

## Script VM (SSB)
- [CONFIRMED] dispatcher 0xe835c, op lengths table 0x216298, native dispatcher 0xa318 (208 natives). 442 scripts disassemble with no bad ops (`tools/ssbdis.py`).
- u32 word code, pc=0 start. Addresses are word indices. Strings UTF-8 inside the file (pushs = word index).
- Op 14 ncall argc,id: native returns 0=done (push result, advance), 1=suspend and re-execute same op next frame, other=stop without advancing.
- Op 1 = yield one frame. Op 0 = end.
- Globals: `pushg n` = stackEnd-(n+1); locals: `pushl n` = fp-(n+1). Globals are created by the pushes at script start.

## Working files (not source)
- `work/eboot_decomp.c`: full decompile (tools/ghidra_scripts/DumpAll.java); `tools/fn.py addr` prints a function.

## Native call layer
- Handler objects have vtable at obj+4; native N → slot `vt+0x14+8*N` (`tools/nat.py lo hi base|prog|act|stat|adv`).
  - base 0x22cbc4 (natives 0-58 common: debug print, RNG, GameData accessors), prog 0x22d99c (64-74), stat 0x22f484 (75-78), act 0x22e30c (79-86), adv 0x2306f4 (3,4,87-207).
- Args: arg0 is the FIRST pushed. Timings in ms converted to 60fps frames `ms*60/1000`.
- Main game object: `FUN_000164b8()` returns base; +0x348544 GameData, +0x59f978 game mode, +0x23811c file loader, +0x238364 sound manager.
- Scenario scene ctor FUN_00053b5c: VM at +0x663244, handler at +0x663a70.
  - +0x1529b0 msg window A, +0x152c40 msg window B, +0x15eaf0 choice, +0x1a9df0 ?, +0x1abdc0 BG/CG layer, +0x356180+i*0x9d300 character slots, +0x4a0bc0 Live2D, +0x494f80 ?, +0x4a0ac0 ?, +0x4a0b6c screen fade, +0x618c00 ?, +0x618ffc ?
  - +0x144f81 skip flag? +0x144f82 voice player.
- Sound: 0x1b3f0 BGM, 0x1b408 SE?, 0x1b410 ?, 0x1b400 ?

## Tools
- `tools/fn.py <addr|lo-hi>` prints decompiled functions; `tools/nat.py`; `tools/ssbdis.py`; `tools/ssbpretty.py` (run with `python3 -I`, cwd work/data for scripts).

## Reference comparison status
- [BLOCKED] GUI PPSSPP's debugger stopped answering (Vulkan render thread stuck in poll while no frames are delivered; window apparently not being presented). PPSSPPHeadless `--debugger` accepts TCP but never answers HTTP. tools/ppdbg.py is a ready websocket client if a responsive instance is available (GUI instance: port 36203).

## Port verification log
- [TESTED] HCA decode equals ffmpeg's within +-1 LSB at inverted polarity (correlation -1.0); ADX equal in shape, steady 0.1% amplitude offset.
- [TESTED] C++ VM runs SCN_MAIN_1Y_1100x_010 to halt with correct dialogue order.
- [TESTED] TXG/XGTL/CBG/CSU and font glyphs render correctly (visual check); opening.pmf video decodes at the right frame; ATRAC audio measured: durations match both movies, levels healthy; not heard.
- Full status: docs/DIFFERENCES.md.
