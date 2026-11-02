# ADV presentation layer spec (everything in a scenario scene except characters)

Sakurasou PSP. Source: decompile of `extracted/EBOOT.BIN.dec`, script corpus (442 `.ssb`), texture inspection.
Tags: **[CONFIRMED]** read directly from code/data. **[ASSUMED]** inferred. Untagged statements in a [CONFIRMED] section inherit it.
Characters (natives 161-178), Live2D (179-196) are out of scope.

## 0. Conventions

- [CONFIRMED] Logic runs in fixed 60 fps frames. `ms → frames = ms*60/1000` (integer division). Negative ms values are passed through unchanged where noted (`-1` = infinite).
- [CONFIRMED] Coordinates: ortho `(-240,240,-136,136,-1,1)`: origin at screen centre, **Y up**. `screen = (240+x, 136-y)`. All numbers below are world units unless "screen" is stated.
- [CONFIRMED] Colours are PSP ABGR `0xAABBGGRR`.
- [CONFIRMED] Meshes are 20-byte `(u,v,x,y,z)` float triangle strips unless stated; `u,v` are 0..1 with v=0 at the top of the image; data addresses are vaddr (file offset - 0xc0).
- [CONFIRMED] Native return: `*ret=0` pushed unless the table says otherwise. "suspend" = native returns 1 and the same op re-executes next frame (see RE_NOTES). In each "wait" native below, it suspends *while the condition is false*.
- Pad: [CONFIRMED] `Fdd2e8`: `held`, `pressed` (edge), `released`, `repeat` (`pressed` on first frame, then with counter: at 15 frames, then every 4 frames while held). PSP bits: SELECT 1, START 8, UP 0x10, RIGHT 0x20, DOWN 0x40, LEFT 0x80, L 0x100, R 0x200, TRIANGLE 0x1000, CIRCLE 0x2000, CROSS 0x4000, SQUARE 0x8000.
- Pointer: [ASSUMED dead on PSP] object at `Game+0x2ff000` has flags (+0x38: 1 down, 8 tap, 0x10/0x20 drag dirs, 0x40 flick) and positions (+0x00 start, +0x10 prev, +0x20 cur). It is cleared every frame (`F1f99c`) and no setter exists in the binary: leftover from a touch build. All tap/flick paths below may be mapped to a mouse, hit boxes are given where useful.
- System SE: `PlaySys(cue)` means cue on the system cue player (§6) with no fade. Observed cues: 0x0f auto ON, 0x1b cloud spawn, 0x1c cloud pick, 0x1d cloud timeout, 0x1f cancel, 0x24 friendship up, 0x29 decide, 0x2a cursor move.

## 1. Scene structure

Scene id 0xb (ctor `F53b5c`, object size 0x6a3b80). Sub-objects:

| Offset | What |
|---|---|
| +0x144f80 | skip/auto controller: byte0 = AUTO, byte1 = SKIP |
| +0x144f82 | voice player (0x80 byte current filename) |
| +0x145040.. | `conversation_window.txg` (64x64) |
| +0x1529b0 | window A |
| +0x152c40 | window B (mail), texture `conversation_mail.txg` (64x32) |
| +0x15eaf0 | choice, `conversation_choice.txg` (128x32) |
| +0x1a9df0 | clouds, `conversation_cloud.txg`, `conversation_cloud_parts.txg` (128x32) |
| +0x1abdc0 | BG layer pair |
| +0x494f80 | location banner, `conversation_place.txg` (256x64) |
| +0x4a0ac0 | date stamp, `conversation_date.txg` (256x128) |
| +0x4a0b6c | screen fade |
| +0x618c00 | game-title OSK popup, `conversation_pop_window.txg` (128x64) |
| +0x618ffc | friendship popup, `conversation_friend.txg` (256x64) |
| +0x619800 | TIPS viewer (+0x4_98c4 tip index) |
| +0x663100 | active window selector (0=A, 1=B) |
| +0x663104 | name plate visible flag |
| +0x663105 / +0x663125 | current names for A / B (32 bytes) |
| +0x663148.. | BGM save info (state 0/1/2, name[64], volume 0xff default) |
| +0x663190.. | ambient save info (same) |

### Draw order (`F5455c`) [CONFIRMED]
Set ortho, then in order (z only matters for sprites; drawn in this order). "!L" = skipped while L is held (L hides UI).

1. BG current, then BG back (z -1)
2. Window B (!L) (z -0.9) — **behind characters**
3. Character slot 0 (z -0.8), slot 1 (z -0.7)
4. Live2D (z -0.6)
5. Window A at (-232,-136) (!L)
6. Choice at (0,16) (!L)
7. Clouds (not hidden by L)
8. OSK popup (!L)
9. Location banner at (-240,112) (!L)
10. Date stamp at (140,136) (!L and gameMode==0 only)
11. TIPS viewer (!L)
12. Friendship popup at (0,-40) (not hidden)
13. Screen fade (z 1)
14. System overlay (`F14f98`, save notice)

### Frame update order (`F54ec4` + `F4f278`) [CONFIRMED]
```
run VM (Fe835c) unless finished; if VM ended -> scene state "exit" (fades all audio, 5 frames, black, 5 frames)
inMsgWait = windowA.textState==WAIT || windowB.textState==WAIT
if inMsgWait: menu input (§4)
if no menu transition started:
    windowA.update1 (per-frame input), windowB.update1, choice.update1, clouds.update1, popup.update1, skipAuto.update
then always, also while the scene is fading to a menu (F4f278):
    A.update2, B.update2, choice.update2, clouds.update2, BG.update, chars, location, date, fade, Live2D, popup, friendship, tips
```
Note VM runs *before* the windows in a frame.

## 2. Message window A

Texture `conversation_window.txg` 64x64. Top row (v 0..8px): "AUTO", "SKIP" text. Meshes [CONFIRMED]:

| Mesh | addr | geometry |
|---|---|---|
| Body | 0x1f4930, 8 verts | x: 0,8,448,464; y 0..52; u 0,.125,.25,.5; v .1875..1 |
| Name plate | 0x1f49e4, 8 verts | x 0,8,160,168; y 0..20; u .5,.625,.75,.875; v .6875..1 |
| Marker frames (5 quads) | 0x1f4a98 | f0 down-chevron ±12,±8 u.5-.875 v.375-.625; f1 AUTO ±12,±4 u0-.375 v0-.125; f2 play ±8,±8 u.5-.75 v.125-.375; f3 SKIP ±12,±4 u.375-.75 v0-.125; f4 ff ±8,±8 u.75-1 v.125-.375 |
| Link highlight (colour only, 12-byte verts) | 0x1f4c30 | x 0..1, y 2..-16 |
| Name text | 0x1f4c68, 10 verts | 256x16 |
| Body text | 0x1f4d44, 34 verts | 512x32, y 0..-32 (full texture, strip tessellated each 32 px) |

Layout, screen coordinates [CONFIRMED]:
- Body: rect x 8..472, y 220..272.
- Name plate: rect x 8..176, y 200..220.
- Name text: top-left screen (22,205). Colour white. RGBA8888 256x16.
- Body text: top-left screen (38,232). Colour 0xff000000. RGBA4444 512x32. Line 1 screen y 232, line 2 y 250: max **2 lines** in corpus (25700 messages: 15107 one line, 10593 two lines, none more; lines that overflow 32 px are clipped by the texture).
- Marker centre: screen (444,248).

Font: [CONFIRMED] advance 7 (half) / 14 (full), line pitch 18, bitmaps blitted into the text textures with a colour; [ASSUMED] face `font_m.fnt` (14 px cell) at `Game+0x3c1480`. Half-width = U+0020..7E or U+FF61..FF9F, drawn in half cell width. Pen is float, cast to int per glyph.

The text texture is cumulative: `clear` wipes it, each token blits one glyph.

### Window state machines
Window (+0x16c) [CONFIRMED]:
```
CLOSED(0) -- N115 --> OPENING(2) t++ each frame; when SKIP or t>=12 -> OPEN(1)
OPEN(1) -- N117 --> CLOSING(3) t++; when SKIP or t>=12 -> CLOSED(0)
draw: CLOSED nothing; OPEN y=0; OPENING dy = t*72/12 - 72; CLOSING dy = -t*72/12 (applied around the whole window incl. name plate and text)
```
Name plate (+0x74 relative) sub-machine, same shape: t 12 frames, SKIP short-circuits. Draw: OPENING y-scale t/12, CLOSING y-scale 1-t/12 about the plate origin (bottom of plate), OPEN normal, CLOSED nothing.

Text (+0x198) [CONFIRMED]:
```
IDLE(0)       done, N127 returns
VOICEWAIT(1)  entered by N126
TYPING(2)
WAIT(3)       at ▼
TIPSEL(4)     link selecting
TIPVIEW(5)    tip viewer open
```
**VOICEWAIT** (`F59ab0`) each frame:
```
if SKIP && (cfg.skipAll || read[msgId]) voice.clear()              // stop immediately, name cleared
if !cfg.protagVoice && voice.name[6..8]=="SRT" voice.clear()        // "V_A01_SRT_0010.hca" style names
if voice.name=="" -> TYPING
elif voice.state != PREPARED -> TYPING
elif voice ready -> voice.start(fade=0); voice.name cleared; TYPING
else stay
```
**TYPING** (`F5a7c4`). Accumulator float set to 0 on entry.
```
update1: if CIRCLE pressed or CROSS held or (SKIP && (cfg.skipAll||read[msgId])) acc = 999
         (window B instead does acc += 999)
update2:
  acc += {0:0.5, 1:1.0, 2:2.0}[cfg.textSpeed]        // default 1; values >=3 add nothing
  if acc > 0:
    loop:
      tok = next()
      END:  voice.stop(fade 4 frames); read[msgId]=1; -> IDLE; return
      NL:   pen newline
      WAIT: -> WAIT; return
      LINKOPEN(id):  link[n].id=id; link[n].x0,y0=pen; inLink=1; tipsSeen[id-1]=1
      LINKCLOSE:     link[n].x1,y1=pen; n++; inLink=0
      GLYPH half/full: colour = inLink?0xffe99222:0xff000000; blit; acc -= 1
      if acc <= 0: return
```
So: speed 0 = 1 glyph per 2 frames, 1 = 1 per frame, 2 = 2 per frame; tags and newlines are free. CROSS is tested with `held` in A and B (`Fdd420`) so holding CROSS fast-forwards; CIRCLE is edge. In WAIT both are edges.

**WAIT** (`F5ab0c` update1, `F59d2c` update2). Entry zeroes `a`,`b`.
```
update1:
  if CIRCLE pressed || CROSS pressed || (SKIP && (cfg.skipAll||read[msgId])): -> TYPING ; consumed
  if !consumed && !L held && SQUARE pressed && linkCount>0: sel=0; -> TIPSEL; PlaySys(0x2a)
  tap: inside a link box -> sel=i, TIPSEL, PlaySys(0x2a)
       tap outside (x<-212||x>112||y>-88||y<-136) -> TYPING
update2:
  a = min(a+1, 600)
  if !voice.playing: b++; if b>600: a=600
  if AUTO && a>=120 && b>=30: -> TYPING
```
Auto advance: at least 2 s after reaching ▼ and 30 frames after the voice has ended (`voice.playing` = state PLAYING and not finished). Skipped text is not voiced.

**TIPSEL** (`F5afa8`): highlight link `sel`.
```
CROSS pressed: sel=-1 -> WAIT; PlaySys(0x1f)
CIRCLE pressed: -> TIPVIEW; PlaySys(0x29)
SQUARE pressed (L not held): sel=(sel+1)%linkCount; PlaySys(0x2a)
```
**TIPVIEW**: enter: `tips.index = link[sel].id-1; tips.show` (`F1dde0`). CROSS pressed: `tips.hide`, -> TIPSEL, PlaySys(0x1f). The viewer itself lives in `F1e64c`/`F1ece0` [out of scope, see tips spec].

### Link highlight [CONFIRMED]
Drawn inside the text origin before the text: translate (link.x0, -link.y0), scale (x1-x0, 1), solid mesh colour 0xff30c7ff. Only one rectangle: a link crossing a line break is drawn wrong in the original too.

### Marker (`F5b4a4`) [CONFIRMED]
Drawn at window origin + (436,24), not moved by open/close. Counters always tick: `t30=(t30+1)%30`, `t60=(t60+1)%60`, `s30=(s30+1)%30` (+0xc,+0x10,+0x14).
```
if SKIP: at (0,4) alpha = tri(s30, 15) frame f4; at (0,-8) opaque frame f3 "SKIP"
elif AUTO: at (0,4) alpha = tri(t60, 30) frame f2; at (0,-8) opaque frame f1 "AUTO"
elif textState==WAIT: y = t30<20 ? 0 : (t30-20)*8/10 - 8; frame f0 opaque
tri(t,h) = t<h ? t*255/h : 255-(t-h)*255/h
```

### Tag parser (`F6d6c0`) — complete list [CONFIRMED]
UTF-8 input; buffer limited to 0xc0 bytes (0xbf + NUL).
| Token | Bytes | Meaning |
|---|---|---|
| 0 END | 0x00 | end |
| 1 NL | 0x0a | newline |
| 2 WAIT | `▼` E2 96 BC | wait for input |
| 3 LINKOPEN | `†digits‡` | `†`, up to 7 chars of digits, `‡`, atoi → TIPS id (1-based) |
| 4 LINKCLOSE | `‡` | closes link |
| 5 half glyph | | see half-width rule |
| 6 full glyph | | everything else including control chars; bad UTF-8 → '?' |
Escape: `\` + char → that char as literal glyph (bad sequence → U+FFFF).

There is **no** ruby, colour, size or wait-time tag. Links in the corpus: 105. Max 4 links per message (record array of 4 of 5 u32 `id,x0,y0,x1,y1`). Every real message ends in `▼`; text after `▼` is typed after the player advances (the original uses this for nothing more than END).

### Name plate plain parser (`Fe633c`) [CONFIRMED]
Used for names, choices, cloud text, backlog: no tags. 0=end, 1=NL, 2=half, 3=full. Bad UTF-8 → U+FFFF.

### Read tracking [CONFIRMED]
Third argument of N126 = message id, 0..38042 in corpus. Global bitset at `Game+0x3453dc+0x20`, one bit per id, `word = id>>5`, `bit = id&31`, set when text reaches END, **not** at display. Capacity 0x3124 bytes. TIPS seen bitset at `Game+0x3453dc+0x3144`, set on `LINKOPEN` in typing with `id-1`. Both are part of global save (see save spec).

### N126 extra work [CONFIRMED]
Before starting, strips tokens 2/3/4 from text and appends to the backlog (name=`Game+0x59f984`, voice=arg0, text=plain). Then dispatches to the active window. `Game+0x59f984` is the "current speaker" for backlog, written by N119/121/128/129/134.

## 3. Window B (mail)

[CONFIRMED] Text and input logic identical to window A (same tokens, speeds, auto, skip) with its own state objects: states at +0x38c (window), +0x3b8 (text). Differences:
- Slide is vertical from the top: OPENING `dy = 176 - t*176/12`, CLOSING `dy = t*176/12`, 12 frames each.
- No name plate state; name always drawn in the header.
- CIRCLE/CROSS skip uses `acc += 999`.
- Tap box y uses `16 - y` instead of `-96 - y`, click-away box x -212..112, y -24..24.
- Marker: no AUTO/SKIP icons. In WAIT draws quad 0x1f6338 (±12,±8, u .375-.75, v 0-.5) at (0,-24), no bounce.

Layout, world, drawn around origin (draw is behind characters, see order):
```
translate (0,64):  header 9-slice, mesh 0x1f5f58, rows y 16..8, 8..-8, -8..-16; x -232,-224,224,232
  at (-204,7): fixed label "差出人：" 64x16, mesh 0x1f6390 (text black)
  at (-148,7): name text 256x16, RGBA4444 black
translate (0,0):   body 9-slice, mesh 0x1f6148, rows y 40..32, 32..-32, -32..-40
  at (-204,16): link highlight (same rule as A), then body text 512x32, RGBA4444 black
if text state==WAIT: at (0,-24) marker
```
UV rows: top row v 0-.25, middle .25-.5, bottom .5-.75; u 0,.125,.25,.375.

Natives 130-133 open/wait/close/wait B; N134 selects which window receives 123, 126-129.

## 4. Input during ADV

Only while window A or B is in text WAIT (`F54ec4`) [CONFIRMED]. `gameMode` = `Game+0x59f978`: 0 main story; 1 replay (memories); 3,4,5,6 other (DLC/special) [ASSUMED names].

| Input | Condition | Effect |
|---|---|---|
| TRIANGLE (no L) | mode 0 or 3-6 | Save snapshot (`F01f6c`), voice stop fade 10, screen fade out to colour 2, 10 frames, push menu scene 0x10 (mode 0) / 0x11 (others). On return fade in 10 frames |
| TRIANGLE (no L) | mode 1 | Modal "終了しますか？" (Yes/No, `F147f0`). Yes -> leaves scene |
| Flick with Δy ≤ -20 (cur-start) | as TRIANGLE | same (pointer, dead on PSP) |
| SELECT | any | PlaySys(0x29), voice stop 10, fade black 10, push scene 0x1d (backlog), fade in 10 on return |
| L + TRIANGLE | mode 0, 3-6 | Quick save: `F01f6c`, `F01e80`, PlaySys(0x29), notice overlay `F14f20` |
| L + SQUARE | mode 0, 3-6 | PlaySys(0x29), voice stop 10, fade black 10, push scene 0x1a (mode 0) / 0x24 (others): save/load UI (`saveload_*`, [ASSUMED] quick load page), fade in on return |
| L held | any | Hides UI (see draw order); also suppresses SQUARE tip select |
| START | any | Toggle AUTO. Turning on plays cue 0x0f, turning off silent. Any time skipAuto updates |
| CIRCLE / CROSS / tap | | Also clears AUTO |
| R held | any | SKIP = 1 for the frame, updated every frame (`F6d45c`) |
| SQUARE | WAIT with links | TIPS selection |

### Skip semantics [CONFIRMED]
`SKIP = byte1 = R held`. Passing the read test: `cfg.skipAll (cfg+0x18) || read[msgId]`. If it fails, SKIP does nothing to the text (message types normally) but all of the below still apply:
- window, name plate, BG alpha/scroll/shake, screen fade: finish on the next update (check `iVar1 + 1` byte in each).
- N4 script wait: zeroes counter.
- N98, N108, N125 waits are bypassed.

### Config object `Game+0x3453c0` [CONFIRMED]
| Offset | Default | Meaning |
|---|---|---|
| +0x00 | 5 | SE volume 0..5 (applies to system SE, script SE, SE stream, ambient) |
| +0x04 | 5 | BGM volume 0..5 |
| +0x08 | 5 | Voice volume 0..5 |
| +0x0c | 5 | [ASSUMED unrelated: read by DLC/PMF code] |
| +0x10 | 1 | Protagonist voice on |
| +0x14 | 1 | Text speed 0..2 |
| +0x18 | 0 | Skip unread text |
| +0x19 | 0 | Install mode flag |

## 5. Backlog (scene 0x1d, ctor `Fc5d80`)

Storage [CONFIRMED]: `Game+0x59f9a4`, ring of **1000** entries (oldest recycled, `F0eba4`), entry 0x164 bytes:
`u32 type; char name[32]; char voice[128]; char text[192]`. Type 0 message (N126), 1 choice picked (`F45960`, name/voice empty), 2 cloud picked (`F49d94`). Text keeps `\n`.

Textures `backlog_bg.txg`, `backlog_parts.txg`, `backlog_arrow.txg`. Scene: BG quad repeated in 32 px rows from y 136 downward with offset `scroll - floor(scroll/32)*32` (`Fc5b24`; scroll is the list offset), list drawn at x -200.

Item height `h = lines*18 + 24`. Item layout, relative to item origin with x offset `+16` when `type != 0` [CONFIRMED]:
- selected highlight (`Fc736c`): back quad y-scaled to `h`, only if selected
- voice mark: at (0,-12) frame 0 of parts mesh 0x211208 region if `voice[0]!=0`
- type icon at (16,-12): frame 0 for type 1, frame 1 for type 2 (mesh 0x211108)
- name at (16,-12), RGBA4444 256x16, pen (0,1)
- text at (32,-24), 512x(lines) black, line pitch 18

Items are centred around the list with a running offset; 9 pooled items, up to 4 either side of the selection. Selection starts at the newest entry, at list centre. Scrolling `offset` tweens: `offset += (target-offset)*0.5` until |d|<1 (`Fc816c`, state +0x820). Arrows at x 360 relative to the list, alpha `tri(t,15)` over 30 frames; each hidden at the respective end.

Input (`Fc97fc`, disassembly only) [CONFIRMED by constants]:
- CIRCLE pressed: if selected voice non-empty, play it on the voice channel with no fade.
- UP / DOWN (repeat): selection moves by one (UP towards older [ASSUMED]), target offset moves by `(h_a + h_b)/2`, PlaySys(0x2a), state TWEEN. No wrap.
- CROSS pressed: PlaySys(0x1f), voice stop 10, fade black 10, pop scene.
- Drag/flick: dead on PSP.

Entry: `F20f78` fade in 10 frames. Scene fade colour 0.

## 6. Sound

Manager `Game+0x238364` [CONFIRMED]:

| Getter | Offset | Class | Use |
|---|---|---|---|
| `F1b3f0` | +0x0000 | 0x234a1c two-deck stream | BGM |
| `F1b3f8` | +0x1704 | 0x2348ac cue | system SE |
| `F1b400` | +0x2284 | 0x2348ac cue | script SE (N106-109) |
| `F1b408` | +0x2e04 | 0x234ad4 stream | SE stream (N93-99): one-shot .adx sounds |
| `F1b410` | +0x3980 | 0x234ad4 stream | ambient (N100-105), state saved with game |
| `F1b418` | +0x44fc | 0x234ad4 stream | voice (.hca) |

Files load from the STRM loader (`Game+0x23811c+0x10`); cues index `sound_se.awb` at `Game+0x27d400`.

Stream state: 0 stopped, 1 prepared (paused at start), 2 playing/ended (stays 2 after end). Commands execute from a queue (`Fecb58`). Methods (vtable slots of `+offset`):
```
+0x24 stop(fadeFrames)              fade to 0, then stop; state=0
+0x2c prepare(loader, file)         stop; open; start paused; state=1 when ready
+0x3c play(loader, file, fadeFrames) stop; open; start; volume fade 0→1; state=2
+0x44 playCue(awb, cue, fadeFrames) same with cue
+0x4c startPrepared(fadeFrames)     resume; fade in; state=2
+0x64 setMaster(v, frames)          group 1
+0x6c setVolume(v, frames)          group 2
+0x7c state
+0x9c ready                         state==1 && queue empty && lib status==2
+0xac finished                      state==2 && queue empty && lib status==3
```
[CONFIRMED] Gain = `m1*m2*m3*fade`, four independent floats. Each ramps linearly: every frame while `n>0`: `v += (target-v)/n; n--`, frames<=0 applies immediately. `m1` = config/5, `m2` = script volume/255 (default 1), `m3` unused here, `fade` = play/stop.
[ASSUMED] Looping comes from the file's ADX loop info (no loop argument exists).

BGM deck rule [CONFIRMED]: `play`/`playCue`/`startPrepared` swap decks, new deck plays with fade `f`, old deck `stop(f)`. So N87(file, ms) and N91(ms) are crossfades of the same length. Prepare goes to the back deck.

Voice player: `voice.prepare(name)` (`F6dc8c`): `voice.clear()` (stop, name=""), copy name, if non-empty `prepare`. Used by N124 (preload) and N126 (`F6ddc0`: only if different from the current name). Start in VOICEWAIT with fade 0. On message END stop with 4 frame fade. On scene menus stop with 10. `voice.playing` = state==2 && !finished. Volume master cfg+8/5. Voice lines are 22050 Hz HCA mono.

Save data keeps BGM and ambient name, state and script volume (`0x663148`, `0x663190`); SE stream, cue SE, voice are not saved. Scene ctor stops BGM, ambient, SE stream, voice immediately. Scene exit fades all six channels over 5 frames.

## 7. Choices (natives 135-138)

`conversation_choice.txg` 128x32. Max 5 lines, split by `\n`, each ≤63 bytes (total ≤0x140); plain parser. Text texture RGBA4444 256x16, pen (0,1), 14/7 px, pitch 14. `\n` in the total string is the separator only.

Layout [CONFIRMED], relative to (0,16), `n`=count:
- Frame: 9-slice from mesh 0x1f2b10 (rows 3 × strips 3): origin (-134, n*24/2), stretched size 268 × n*24; corners 8 px. Top row y 8..0, stretch row 0..-1 scaled by H, bottom 0..-8; columns x -8..0, 0..1 scaled by W, 0..8.
- Item i centre y: `n*12 - 12 - i*24`. Bg at x -126: mesh 0x1f2de8 three pieces scaled 252 wide, ±12 high (white pill). Selected tint 0xffccffff, else white.
- Item text at x -118: colour black if selected, else 0xff808080. Mesh 0x1f2fd8, 256×±8.
- Cursor at x -126: orange outline mesh 0x1f2ee0, 252 wide.

States (`+0x404`) [CONFIRMED]:
```
IDLE                N137 ready
APPEAR  (N135)      t=0: frame.open (scale x,y t/12 over 12 frames, about origin);
                    each item i at t==12+6*i starts open: y-scale t/12 over 12 frames.
                    Cursor off. When frame open and all items open -> SELECT.
SELECT  input (below)
PICKED  t=0: every other item closes (y-scale 1-t/12 over 12); stays until t==60 -> LEAVE
LEAVE   t==0: picked item closes; t==12: frame closes (1-t/12 over 12); when closed -> IDLE
```
Input in SELECT: cursor starts at -1 (no selection).
```
CIRCLE: if cursor<0: cursor=0, PlaySys(0x2a) else: push backlog type 1; -> PICKED; PlaySys(0x29)
UP repeat: cursor<=0 → n-1, else cursor-1; PlaySys(0x2a)
DOWN repeat: cursor<0 → 0, cursor>=n-1 → 0, else cursor+1; PlaySys(0x2a)
tap: box x ±134, y item±12 (+16): select and pick
```
Not skippable, CROSS does nothing. Cursor uses plain loop, wrap on both ends. N138 returns cursor. Save data stores text, cursor, state.

## 8. Clouds (natives 139-144)

"Button prompt clouds": up to 32 cloud objects keyed by slot id, 4 screen positions, each mapped to a face button. Script builds a tree with N140 then runs it. N144 unused in the corpus (script does not branch on result here).

N140 args: `slot, text, posIdx(0..3), timeoutMs(-1 none), ok0, ok1, ok2, to0, to1, to2`, chains (-1 none).
N141 `(slot, ms)`: schedule cloud `slot` to spawn after `ms` (pool of 32 timers, ticked in RUN).
N139 reset: all clouds idle, no timers, result=-1.
N142 start RUN, result=-1.
N143 wait until manager IDLE.

Per-frame RUN (`F4b760`,`F4b994`):
```
buttons: pos0 SQUARE, pos1 CIRCLE, pos2 CROSS, pos3 TRIANGLE
for i in 0..3: if pressed(btn[i]) && activeAt[i]: press(cloud); break
timers tick; on expiry spawn(slot, defaultPos)
clouds update
if no timers and no active clouds -> IDLE
press(c):
  if any ok chain >=0: spawn each ok slot at c.pos; c -> LEAVE
  else: PlaySys(0x1c); c -> PICKED; result=c.slot; other active -> FADE; backlog type 2; manager -> DRAIN (IDLE when empty)
```
Timeout: `c.age >= timeoutFrames`: spawn `to*` chains from `c.pos`; PlaySys(0x1d); c -> LEAVE.
Spawn: PlaySys(0x1b), occupies pos slot (kicks existing occupant to FADE).

Default target positions (`F4955c`): if pos2 and pos3 both empty: p0(-152,30) p1(152,30) p2(0,-36) p3(0,92); else p0(-152,4) p1(152,52) p2(96,-36) p3(-96,92). Cloud moves `pos += (target-pos)/15` each frame; frozen while PICKED.

Cloud draw (no base translate): at pos. Text 128x32 RGBA4444 centred: `H=14+18*nl`, pen `y=16-H/2`, each line `x=64-w/2`. Button icon at (42,-30): `conversation_cloud_parts` frames 3,0,1,2 for pos 0..3. Spawn (50 frames): scale and alpha `t/50`; sparkle rings staggered by 5 frames: scale `t/35*1.5`, alpha `255-255*t/35`. Frame counter `f%60` picks cloud sprite: 0-9→0, 10-19→1, 20-29→2, 30-39→3, 40-49→4, else 5. PICKED lasts 120 frames (`F4b4b8`): scale `1+0.5*t/10` to t=10, `1.5-0.5*(t-10)/10` to t=20, then 1. Exit animation `F4c598`: 50 frames, alpha `1-t/50`, same staggered sparkle rings as spawn.

## 9. BG layer (natives 147-160)

Two layers `cur` (+0x1aa300) and `back` (+0x1aa304), pointers swap. Each: CBG loaded into a 0xc8000 buffer on a thread, tile mesh scaled 32, alpha (int 0..255), pos, shake offset. Not drawn until loaded or if alpha 0. Draw `cur` first, then `back` **on top**. Draw translate `pos + shake - (cx, -cy)` with `F00914(alpha<<24|0xffffff)`; [CONFIRMED] `pos` is image-centre-relative: image is placed so its centre is at the screen centre plus `pos`, Y up.

Alpha machine: HIDDEN(0), OPAQUE(255), FADEIN(n), FADEOUT(n). `alpha=(t*255)/n` and `255-(t*255)/n` where t increments first. n<=0 applies instantly. Ends at `t>=n` or SKIP.

Group state machine (+0x1aa308):
| Native | Action |
|---|---|
| 150 (ms) | **Fade in cur**: back HIDDEN, cur FADEIN(n). Waits for cur OPAQUE |
| 151 (ms) | **Fade out cur**: back HIDDEN, cur FADEOUT(n) |
| 152 (ms) | **Cross-fade**: back FADEOUT(n), cur OPAQUE (underneath). Waits for both |

Normal sequences in scripts [CONFIRMED by corpus]:
```
N147(file); N148(); N149(); N155(0,0); N150(ms or 0); N153()    fade from nothing
N147(file); N148(); N149(); N152(ms); N153()                     crossfade
N151(ms); N153()                                                 fade to nothing
```
`N149` swaps: new image becomes `cur`, previous image is `back`. [CONFIRMED] `N155` (`F4473c`) targets `back`, so `N155(0,0)` right after `N149` resets the *old* image's layer; that layer is the next load target, so every newly loaded image starts at pos (0,0) (unless a scroll on `cur` happened and it is the current one, which then gets reset on the next load cycle). N154 sets `cur`, unused in corpus.

Scroll: N156(x,y,ms) on `cur`: `pos = lerp(start,target,t/n)`, linear, t++ first. ms<0 passes through: n<1 = instant. N157 wait. Skip jumps to target. Corpus: 4 uses, `(0,540,25000)`, `(0,4,20000)`, `(0,270,80000)`, `(0,26,20000)`.

Shake: N158(x,y,ms) on both layers: each frame `offset = (2x*r/32767-x, 2y*r/32767-y, 0)` with r from `seed=seed*0x41c64e6d+0x3039; r=(seed>>16)&0x7fff`, seed=1 at start, new random per axis including z. ms<0 infinite until N159, ms==0 same as stop. `N160` waits for timed shake only: returns suspend while active; do not use after infinite shake. Skip ends timed shake. Corpus amplitude 4 or 8. Stop clears the offset to (0,0,0).

No zoom, rotation, mask or wipe native exists for BG in ADV. [CONFIRMED] `img_alpha_0*.txg` belong to action scene `F7fd8c`, not ADV. All BG transitions are alpha. Flash: not in the layer; scripts use screen fade 110-113.

## 10. Screen fade (natives 110-114)

Black quad (-240..240,-136..136), z 1, mesh 0x1f5090. States: CLEAR, BLACK, FADEOUT(n), FADEIN(n). `t` increments before test, `alpha=t*255/n`, or `255-t*255/n`. Ends when `t>=n` or SKIP, n<1 instant.

| Native | Effect |
|---|---|
| 110 (ms) | fade to black, 111 waits BLACK |
| 112 (ms) | fade from black, 113 waits CLEAR |
| 114 (kind) | fade kind, 0 black, nonzero draws nothing. Unused in corpus |

## 11. Location banner (natives 197-200)

File arg `LC_*.loc` (plain TXG): N197 loads on a thread into its slot. N198 waits. N199 starts. N200 waits done (state returns to HIDDEN).

`conversation_place.txg` 256x64: mesh row A (blue tape) and row B (lines) from 0x1f3e30 / 0x1f41cc, each 42 verts 320×32 (20 columns of 16, `x 0..320`, y ±16). Drawn at base (-240,112) then (-16,-16) rotated 15° (2π/24) CCW. Text texture `.loc`: mesh 0x1f4568 same size, drawn only after load.

Bend: `y' = y + amp*(1 - sqrt(1 - min(x/160,1)^2))`.

| State | Length | Draw |
|---|---|---|
| IN | 25 frames (t>24) | Tape: x-scale `t/15` until t=15 else 1; amp `32*sin(t/15*π/2)` for t<15, then 0. Text (from t=10): x-scale `(t-10)/15`, amp `32*sin((t-10)/15*π/2)`; amp 32 before t=10 |
| SHOW | 180 frames | static, amp 0 |
| OUT | 20 frames | translate x `-160*t/20`, alpha `255-255*t/20` |

Not skippable (no skip check) [CONFIRMED]. Draw is !L.

## 12. Date stamp (natives 201-205)

N201 = show 180 frames, N202(ms; -1 stays until 204), N203 wait state HOLD, N204 hide, N205 wait hidden.
States: IN (30 frames), HOLD (auto-hide after duration if >=0), OUT (10 frames).
IN: y offset `8-8*min(t/10,1)`; page angle `a = -((sin(πt/30)+1)/2 * π/2 * sin(2πt/40))`. OUT: y offset `40*t/10`.
Draw at (140,136), `conversation_date.txg`, only in gameMode 0 and !L. Date from `GameData` (`F10f08`: month, day, weekday, holiday):
- Static back: mesh +0x38 frame 1. Page: y-scale `sin(a)`, shear `(cos(a)*-0.5,0)`, mesh +0x4c.
- Glyphs 16x16 at y -24: month tens (x12, if ≥10), month ones (20), 月 (32), day tens (44), day ones (52), 日 (64), `(` (74), weekday (82), `)` (90).
- Frames (mesh 0x1f366c, 0x690 bytes, 21 quads): 0 月, 1 日, 2 `(`, 3 `)`, 4+wd weekday, 11+d digit.
- Colours: white except weekday: Sun or holiday 0xff0000ff (red), Sat 0xffff5000 (blue), others black.
Exact scene use: `N201(); N203(); ... N204(); N205();` `[ASSUMED]`.

## 13. Friendship popup (N206/N207)

Arg: 0 up small, 1 up medium, 2 up big, 3 **level** up. N206 calls `load+start`, PlaySys(0x24). N207 waits until HIDDEN.
Strip at (0,-40) with `conversation_friend.txg`: mesh 0x1f52f8: 3 × 4 verts: left cap, 30 × 16 px stretch from x -240, right cap; ±20 high. Text panel mesh 0x1f53f0: frame 0 "親密度アップ!", frame 1 "親密度Levelアップ!" (arg 3). Particles: petal mesh 0x1f54ec ±12, count 5/10/20/20, tables at 0x1f50c8 / 0x1f5118 / 0x1f51b8.
States [CONFIRMED]: IN 10 frames: alpha and scale `t/10` (pivot translate (240,-20), scale, translate (-240,20)); HOLD 120 frames; OUT 10 frames: alpha and scale `1-t/10` (pivot (-240,20)); N207 true when HIDDEN.
Petal (`F62b0c`), record `{float s; u32 delay; float phase; float lift}`: waits `delay` frames, then lives 90 frames, `p=t/90`, `th=2π p+phase`; x `= 500p + 80 cos(1.3π p) - 250`, y `= 20*(sin(th) + 4p² - 2 lift p)`; scale (s,s) and y-scale `cos(th)` for the flutter.

## 14. Game title popup (N145/N146)

`conversation_pop_window.txg`. N145 opens (scale t/10 over 10 frames), N146 waits until closed. Fixed labels "ゲームタイトル名" and "決定". Row 0 opens PSP on-screen keyboard (`Fca9xx`) up to 0x18 chars, row 1 confirms. UP/DOWN moves, CIRCLE activates, cue 0x2a/0x29. Confirm refuses empty name [ASSUMED]. Result string at +0x1dc, re-rendered by `F63180`, saved [ASSUMED into GameData]. In the port: replace OSK with native text entry.

## 15. Native table

Arg0 = first pushed. "S" = may suspend. Unused = not in corpus. Args in ms unless stated.

| # | Args | Semantics | S | Ret |
|---|---|---|---|---|
| 3 | ms | Timer = ms frames | | 0 |
| 4 | | Wait timer; SKIP sets 0 | S | 0 |
| 87 | file, ms | BGM crossfade play. BGM state=2 | | 0 |
| 88 | ms | BGM stop. state=0 | | 0 |
| 89 | file | BGM prepare back deck. state=1 | | 0 |
| 90 | | Wait BGM ready | S | 0 |
| 91 | ms | Start prepared with crossfade. state=2 | | 0 |
| 92 | vol, ms | BGM volume 0..255 | | 0 |
| 93 | file, ms | SE stream play. Corpus 2 | | 0 |
| 94 | ms | SE stream stop | | 0 |
| 95 | file | SE stream prepare | | 0 |
| 96 | | Wait prepared | S | 0 |
| 97 | ms | SE stream start | | 0 |
| 98 | | Wait finished unless SKIP | S | 0 |
| 99 | vol, ms | SE stream volume | | 0 |
| 100 | file, ms | Ambient play. state=2. Unused | | 0 |
| 101 | ms | Ambient stop. state=0 | | 0 |
| 102 | file | Ambient prepare. state=1 | | 0 |
| 103 | | Wait prepared | S | 0 |
| 104 | ms | Ambient start. state=2 | | 0 |
| 105 | vol, ms | Ambient volume | | 0 |
| 106 | cue, ms | Script SE cue play | | 0 |
| 107 | ms | Script SE stop | | 0 |
| 108 | | Wait finished unless SKIP | S | 0 |
| 109 | vol, ms | Script SE volume. Unused | | 0 |
| 110 | ms | Fade to black | | 0 |
| 111 | | Wait black | S | 0 |
| 112 | ms | Fade from black | | 0 |
| 113 | | Wait clear | S | 0 |
| 114 | kind | Fade kind. Unused | | 0 |
| 115 | | Open window A | | 0 |
| 116 | | Wait A open | S | 0 |
| 117 | | Close window A | | 0 |
| 118 | | Wait A closed | S | 0 |
| 119 | | Show name plate; speaker=name | | 0 |
| 120 | | Wait plate open | S | 0 |
| 121 | | Hide plate; speaker="" | | 0 |
| 122 | | Wait plate closed | S | 0 |
| 123 | | Clear text of active window | | 0 |
| 124 | file | Voice preload. "" = none | | 0 |
| 125 | | Wait voice prepared unless SKIP | S | 0 |
| 126 | voice, text, msgId | Speak on active window, append backlog | | 0 |
| 127 | | Wait active window text IDLE | S | 0 |
| 128 | | Clear name of active window | | 0 |
| 129 | name | Set name of active window | | 0 |
| 130 | | Open window B | | 0 |
| 131 | | Wait B open | S | 0 |
| 132 | | Close window B | | 0 |
| 133 | | Wait B closed | S | 0 |
| 134 | which | Active window 0=A 1=B; updates speaker | | 0 |
| 135 | text | Start choice, `\n` separated | | 0 |
| 136 | a, text | Same, arg0 ignored. Unused | | 0 |
| 137 | | Wait choice IDLE | S | 0 |
| 138 | | | | cursor index |
| 139 | | Clouds reset | | 0 |
| 140 | slot, text, pos, timeoutMs, ok0-2, to0-2 | Define cloud | | 0 |
| 141 | slot, ms | Schedule spawn | | 0 |
| 142 | | Run | | 0 |
| 143 | | Wait done | S | 0 |
| 144 | | Result slot or -1. Unused | | slot |
| 145 | | Open title popup | | 0 |
| 146 | | Wait closed | S | 0 |
| 147 | file | Load CBG to back | | 0 |
| 148 | | Wait load | S | 0 |
| 149 | | Finish load, swap cur/back | | 0 |
| 150 | ms | Fade in cur. -1 passes unconverted: instant | | 0 |
| 151 | ms | Fade out cur | | 0 |
| 152 | ms | Cross-fade (back out over cur) | | 0 |
| 153 | | Wait fade | S | 0 |
| 154 | x, y | Set cur pos | | 0 |
| 155 | x, y | Set back pos | | 0 |
| 156 | x, y, ms | Scroll cur | | 0 |
| 157 | | Wait scroll | S | 0 |
| 158 | x, y, ms | Shake, ms<0 infinite | | 0 |
| 159 | | Stop shake | | 0 |
| 160 | | Wait shake | S | 0 |
| 197 | file | Load location TXG | | 0 |
| 198 | | Wait load | S | 0 |
| 199 | | Show banner | | 0 |
| 200 | | Wait banner done | S | 0 |
| 201 | | Date stamp 180 frames | | 0 |
| 202 | ms | Date stamp, -1 hold | | 0 |
| 203 | | Wait hold | S | 0 |
| 204 | | Hide date | | 0 |
| 205 | | Wait hidden | S | 0 |
| 206 | kind | Friendship popup | | 0 |
| 207 | | Wait popup done | S | 0 |

Typical message in scripts [CONFIRMED]:
```
N124(voice) N125()                           // preload
N129(name)                                   // set plate name
N126(voice, text, id)  N127()                // speak and wait
```

## 16. Unresolved

- Backlog exact pixel layout (mesh data referenced by address only), which backlog direction key is older.
- Order of the matrix ops in the petal draw is inferred from call order.
- Meaning of game modes 3-6, config +0x0c, screen fade colour 2.
