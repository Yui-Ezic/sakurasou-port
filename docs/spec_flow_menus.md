# Spec: game flow outside ADV, menus, save files

Tags: [CONFIRMED] = read directly from the decompile/binary data. [ASSUMED] = inferred, not exercised at runtime.
All addresses are Ghidra addresses of `EBOOT.BIN.dec` (file offset = addr + 0xc0). "G+x" = field at offset x from the main game object (`FUN_000164b8()`).

## 0. Conventions used everywhere

### Coordinates [CONFIRMED]
UI is drawn with ortho(-240,240,-136,136,-1,1): origin at screen centre, **Y up**.
`screen_x = 240 + x`, `screen_y = 136 - y`. All positions below are in this space unless stated otherwise.
All coordinates are 480x272 PSP pixels, all timing in frames at 60 fps.

### Sprite data [CONFIRMED]
A sprite frame is 4 verts of 5 floats `(u, v, x, y, z)`, triangle strip, order TL, BL, TR, BR. Stride 0x50 per frame. x,y are local to the sprite anchor (usually centred), `u,v` normalised against the texture size.
Two forms exist:
- plain table: `FUN_xxx(obj, 0, tex, &DAT_table)`, n entries at stride 0x50.
- sprite-class: vtable with +0x14 frame count, +0x1c verts/frame, +0x24 pointer to data, atlas classes have +0x0c texture name. `FUN_00003514(sprite, gfx, frame)` draws a frame at the current matrix with the current colour.

Generic sprite object (0x48 bytes): +0 verts, +4 indices, +8 texture, +0xc pos vec3, +0x1c scale vec3, +0x2c rotation vec3 (radians), +0x3c ARGB, +0x40 alpha byte (copied into colour top byte at draw), +0x41 use scale, +0x42 use rotation, +0x43 visible.
Draw order for transforms: translate, then rotZ/Y/X, then scale.
Colours are 0xAABBGGRR (PSP order).

### Input [CONFIRMED]
Buttons object at `*(G+0x2fefc0)` (FUN_000dd2e8 updates it each frame). Masks are PSP: SELECT 0x1, START 0x8, UP 0x10, RIGHT 0x20, DOWN 0x40, LEFT 0x80, L 0x100, R 0x200, TRIANGLE 0x1000, CIRCLE 0x2000, CROSS 0x4000, SQUARE 0x8000.
- `FUN_000dd420` held, `FUN_000dd430` pressed (edge), `+8` released, `FUN_000dd450` repeat.
- Repeat: fires on the press frame, then after 15 frames, then every 4 frames while held (counter +0x20: on new press =0; fires if c==0, or c>=15 and (c-15)%4==0; c++ and wraps to 15..18).
- CIRCLE = confirm, CROSS = cancel everywhere (JP convention).
- G+0x2ff000 is a touch/pointer input object (flags: 8 = tap, 0x10 = down, 0x20 = ?, 0x40 = swipe). Save/load grids support it. It has no PSP source [ASSUMED Vita leftover]; a port can feed it from the mouse or ignore it.

### Sound shorthand
`SE(n)` = `G+0x238364 -> FUN_0001b3f8` player `+0x44(awb=G+0x27d400, id n, 0)`, cue in sound_se.awb.
Known ids: 0x29 decide, 0x2a cursor move, 0x1f cancel, 0x23 title jingle/whoosh at title entry.
Sound manager channels: FUN_0001b3f0 BGM, FUN_0001b3f8 SE, FUN_0001b400 and FUN_0001b408 and FUN_0001b410 further SE-class channels (all follow SE volume), FUN_0001b418 voice. Player vtable: +0x24 stop(fade), +0x3c play(loader, name, 0), +0x64 setVolume(float, fade), +0xa4 isPlaying.

## 1. Job (scene) system [CONFIRMED]

Every scene is a `Job` (base FUN_000dd06c, vtable at +0x6c, id at +0x70):
```
+0  parent job    +4 top child    +8 next child down the stack
+0xc..+0x54 ring storage (9 entries of {cmd,arg})
+0x54 ring ptr  +0x58 capacity(8)  +0x5c read  +0x60 write  +0x64 count
vtable: +0x0c create(id) +0x14 destroy(child) +0x1c dtor +0x24 onEnter +0x2c onExit
        +0x34 onResume +0x3c onPause +0x44 onUpdate +0x4c onDraw
```
Commands are queued on the **parent**, and processed at the start of the parent's update:
- `FUN_000dccdc(job,id)` PUSH: create(id), link on top of job's stack, call its onEnter.
- `FUN_000dcd24(job)` POP: onExit top, unlink, destroy.
- `FUN_000dcd70(job)` POPALL.
Processing (FUN_000dcdbc), only if count>0: call onPause on current top, run all commands in order, call onResume on the resulting top.
`FUN_000dd1c0(job)` = process queue, then update **only the top child**. Draw also draws only the top child (FUN_000dd208). So a job below another neither updates nor draws, and a scene that needs a backdrop draws it itself.
Note that onResume runs for a freshly pushed scene too, immediately after onEnter. Most scenes start their fade-in there.

Root update (FUN_00016fe4), each frame: file loader, audio, input, touch, common systems, then update children, then save managers; **if root has no child: PUSH 3**.
Root onEnter (FUN_000175b0): mount, load `sound_se.awb, slump.slpd, task_list.tskd, team_list.tmd, team_rank.rkd, comment_list.cmed, tips_list.tipd, font_m.fnt, font_s.fnt`, init config, global data, GameData, then PUSH 1.
Root draw order (FUN_00017f34): common bg layer, jobs, help bar (G+0x3300c0), popup (G+0x307040), transition overlay (G+0x2ff080).

### Scene id table
| id | class ctor | enter / update / draw | role |
|---|---|---|---|
| 1 | b6d0 | b760/b860/- | boot notice popup, then POP self, PUSH 2 |
| 2 | b8b4 | b944/b9d8/- | read SYSTEM.DAT, then POP self, PUSH 0x20 |
| 3 | ba78 | bb08/bbb4/- | write SYSTEM.DAT, then POP self, PUSH 0x14 |
| 4 | bc20 | bcb0/bd5c/- | write SYSTEM.DAT, then POP self |
| 5 | 237c8 | 23858/23c3c/23c74 | progress host; mounts DLC, PUSH 6 |
| 6 | 23ef4 | 23fd8/240d8/24138 | progress script VM |
| 7 | 22af0 | 22edc/22fe8/23028 | system (carry-over) save, native 70 |
| 8 | 36d98 | 38f04/37910/3947c | status/spec scene, native 65 |
| 9 | 24e64 | 287d0/25ff8/26074 | action selection, native 66 |
| 0xa | 3ce34 | 3d804/3db90/3dcc4 | spec square (仕様スクエア) |
| 0xb | 4e3cc | 53b5c/4f14c/5455c | ADV/conversation scene, native 67 |
| 0xc | 71b34 | 71ccc/72088/721c0 | calendar, native 64 |
| 0xd | 76fa8 | 77770/773a0/774a0 | task list |
| 0xe | 7db34 | 7dc38/7def4/7e068 | contest, native 68 |
| 0xf | 84550 | 846e8/84b38/84c34 | option/config |
| 0x10 | 8e08c | 8e224/8e71c/8e95c | in-game menu |
| 0x11 | 8d724 | 8d8bc/8dc94/8de94 | DLC drama in-game menu |
| 0x12 | 91cc0 | 91e58/92228/92438 | system menu (main-mode, reduced) |
| 0x13 | 966a4 | 96824/96b18/96c14 | tips list |
| 0x14 | 9a300 | 9a498/9ab58/9a98c | title |
| 0x15 | 9ff84 | a0118/a0574/a0704 | omake menu |
| 0x16 | a62e8 | a645c/a6734/a6838 | omake standing pictures (立ち絵) |
| 0x17 | b4890 | b4a04/b4cd4/b4dd8 | omake voice |
| 0x18 | aaed8 | ab04c/ab460/ab6c4 | omake character events (キャラ イベント) |
| 0x19 | ae888 | aee68/aeb40/aec44 | omake event CG |
| 0x1a | bba50 | bc09c/bc350/bc3e8 | quick load (10 history snapshots) |
| 0x1b | c36e4 | c3fec/c437c/c5248 | save, 60 slots |
| 0x1c | b9490 | b9d48/ba0d0/bad1c | load, 60 slots |
| 0x1d | c5d80 | c6300/c65b4/c663c | backlog |
| 0x1e | cef18 | cf654/cf244/cf450 | DLC menu |
| 0x1f | d6da8 | d7388/d7074/d7170 | character information |
| 0x20 | d7d8c | d7e94/d8080/d815c | logo_dontcopy, then POP self, PUSH 0x21 |
| 0x21 | d8d94 | d8eb0/d9100/d9328 | logos, then POP self, PUSH 0x22 |
| 0x22 | d9bcc | d9c5c/d9c9c/d9ed8 | opening.pmf, then POP self, PUSH 3 |
| 0x23 | d9ee0 | d9f70/d9fa4/da18c | ending.pmf, native 69 |
| 0x24 | b67d4 | b6cb4/b6f04/b6f8c | DLC quick load |
| 0x25 | b79dc | b7ffc/b826c/b82f4 | DLC save, 10 slots |
| 0x26 | b4fdc | b55ac/b5814/b589c | DLC load, 10 slots |
| 0x27 | cfbcc | cfc5c/cfc9c/cfec4 | DLC pmf player (`%s.pmf`), native 71 |
| 0x28 | da194 | da2e4/da504/da7a8 | debug test scene, not reachable in retail [ASSUMED] |

Classes 0x1a/0x24/0x25/0x26 meanings were verified by their help strings and file lists, see section 7.

## 2. Boot [CONFIRMED]

```
root.onEnter -> PUSH 1
1 onEnter: popup(text=NOTICE, oneButton=no-yes, ...)   // FUN_000147f0(0,0,popup,text,1,0,1); FUN_000135b0(popup)
1 onResume: transition style 0, reveal in 0 frames
1 onPause:  transition style 0, cover in 0 frames
1 update: when popup closed (G+0x3300a4==0): POP self, PUSH 2
2 onEnter: async read "SYSTEM.DAT" (size 0x356c) into G+0x522cc0
2 update: when done: if result==0 apply it (FUN_00004540); POP self; PUSH 0x20
```
NOTICE text (0x1e6e38): `本作はオートセーブに対応しています。\nメモリースティック™アクセスランプ点灯中は\n記録メディアを抜き差ししたり、\n電源を切ったりしないでください。`
No DLC or install check runs at boot. DLC is checked on the DLC menu, and the install choice comes from the option `install` flag applied in FUN_0000d2c0 when SYSTEM.DAT loads. A missing SYSTEM.DAT just leaves defaults.
The port replaces both with "file exists" checks; install data is not meaningful on PC.

### 2.1 Scene 0x20, logo_dontcopy [CONFIRMED]
`logo_dontcopy.txgtl` fullscreen (sprite placed at (-240,136), scale 32). Default transition style is whatever was last set (0 at boot).
```
onResume: reveal(30); hold=0; closing=false
update:
  if !closing: if transition idle-clear and ++hold >= 180: cover(30); closing=true
  else if covered: POPALL(parent); PUSH 0x21
```
No skip input.

### 2.2 Scene 0x21, logos [CONFIRMED]
Sequence index n: 0 logo_amw, 1 logo_kadokawa, 2 logo_nb, 3 logo_cri. The next image loads on thread `Logo-LoadThread`.
onEnter also rerolls the voice index (FUN_0001aa28, see 8.2).
```
state 0: on idle-clear: play logo voice for n (only n<3); state=1
state 1: if START or CIRCLE pressed, or ++hold >= 300:
           style = n<3 ? 4 (white) : 0 (black); cover(30); state=2; n++
state 2: on covered: if n==4 {POPALL; PUSH 0x22} else {start loading next logo; state=3}
state 3: on loaded: reveal(30); state=0; hold=0
```
Logo voice: `<name>.hca` where name comes from one of 3 tables of 31 cues, indexed by `global.voiceIndex` (+0x3164): n=0 table 0x22b2d0 (`V_G04_MSR_0010a, V_G04_SRT_0010b ...`), n=1 table 0x22b34c, n=2 table 0x22b3c8. Played on the voice channel from the STRM loader only if the file exists. Full tables in appendix A.

### 2.3 Scene 0x22, opening.pmf [CONFIRMED]
```
0: start pmf "opening.pmf" (buffer 0x4c8000); ->1
1: when movie state==2 (ready): set movie volume = cfg.movie/5; reveal(10); ->2
2: if movie state==3 (ended): cover(10); ->3
   else if START or CIRCLE pressed: cover(10); ->3
3: on covered: stop movie; ->4
4: when movie fully closed: POPALL; PUSH 3
```
(Skip is available from frame 0; the 30-frame counter at +0x13201f is not checked in retail.)
Scene 3 then saves SYSTEM.DAT and pushes the title.

### 2.4 Scene 0x23, ending.pmf [CONFIRMED]
Same machine with "ending.pmf", and state 4 is POP self. Called by native 69 (callEndRoll).

### 2.5 Scene 0x27, DLC movie [CONFIRMED by structure]
Same machine, file name `%s.pmf` from the string set by native 71 (FUN_000cfba8 copies it to 0x247f00), then POP self.

## 3. Screen transition (G+0x2ff080) [CONFIRMED]

States: clear (+0x7e58), covered (+0x7e60), covering (+0x7e68), revealing (+0x7e78).
`FUN_00020f2c(obj,n)` = cover in n frames (n<1 jumps to covered), `FUN_00020f78(obj,n)` = reveal in n frames (n<1 jumps to clear), `FUN_00020fc4` isCovered, `FUN_00020fd8` isClear, `FUN_00020fec(obj,style)` set style (stays set until changed; initial 0).
With t = frame counter 1..n:

| style | covered draw | covering (t/n) | revealing (t/n) |
|---|---|---|---|
| 0 | black full quad | alpha 255*t/n | alpha 255-255*t/n |
| 1 | black full quad | same quad offset by +(2-2t/n) in clip Y, slides in from the top | offset +2t/n, slides out the top |
| 2 | menu_bg tiles, see below | tiles slide up from below | tiles slide up and out |
| 3 | note page FUN_00020400 | same as covered | FUN_000214c0 note lift, used by calendar only |
| 4 | white full quad | white alpha in | white alpha out |

Style 2: 8x5 tiles of `menu_bg.txg` (64x64, anchor corner x=-240+64*i, y=136-64*j) plus `menu_bg03.txg` scaled 8x in X (64x32 -> 512x32).
- covered: band at y=-152.
- covering: tile y = 136-64j + 272 - 272t/n; band y = 120 - 272t/n.
- revealing: tile y = 136-64j + 272t/n; band y = -152 + 272t/n.

Style 3 parts (for calendar, full constants FUN_00020048, reveal maths FUN_000214c0): note_bg.txg sprites. Base part scaled (90,36) at (0,0); part scaled (2.5,2.5) at (0,55); 5 bars x=-222.5+19i y=-72 scale(1,16); (-57,-43) and (85,-90) scale 2; top band (0,114) scale(90,5.5); 20 holes x=-228.5+24i y=132; band (0,132) scale (90,2). When t>9, a = t-9: y lifts by 6a times per-part factor.

Common usage: almost every scene uses fade style 0 at 10 frames. Style 2 is used going into and out of the in-game menu (see 5), 5 frames for ADV in/out.

## 4. Title (scene 0x14) [CONFIRMED]

Textures: title_parts.txg 256x128, title_logo.txg 512x256, title_copyright.txg 256x32, title_start.txg 256x32, title_obj.txgtl 512x512.
BGM `SK_BGM_01.adx` (loop) starts on enter, then `SE(0x23)`. Camera: ortho(-240,240,-136,136) with z range -1..1.

Sprites (index; px rect in atlas; size). title_parts table 0x202c20, 256x128:
```
0 はじめから blue   (80,0)-(150,16)    70x16    6 same orange  (0,0)-(70,16)
1 つづきから blue   (80,16)-(150,32)   70x16    7 orange       (0,16)-(70,32)
2 おまけ blue       (80,48)-(127,64)   47x16    8 orange       (0,48)-(47,64)
3 ダウンロードコンテンツ blue (80,64)-(160,96) 80x32   9 orange (0,64)-(80,96)
4 設定 blue         (80,32)-(116,48)   36x16    10 orange      (0,32)-(36,48)
5 インストール blue (80,96)-(158,112)  78x16    11 orange      (0,96)-(78,112)
12 blue circle (160,56)-(216,112) 56x56   13 yellow circle (160,0)-(216,56)
14..20 petals 16x16 at u=216, v=0,16,...96
21 white 8x8 dot (0,112)-(8,120)
```
title_logo (0x203300): 22 logo 200x152 (200,0)-(400,152), 23 logo shadow/light 200x152 (0,0)-(200,152), 24 88x24 (424,232)-(512,256).
title_copyright: 25 256x16 at v=0, 26 256x16 at v=16. title_start: 27 184x32 (0,0)-(184,32).
title_obj.txgtl: sprite 344x272, uv (0,0)-(0.671875,0.53125).

Item order (index i): 0 はじめから, 1 つづきから, 2 おまけ, 3 DLC, 4 設定, 5 インストール.

Layout and draw order (FUN_00099338), all [CONFIRMED]:
1. Backdrop: sprite 21 (white 8x8) scale (70,38), pos (0,0,-1), a 560x304 white rectangle.
2. Character: title_obj mesh (FUN_000012c8, 344x272 local) scale (32,32), pos (-104,136,-1). Same scale-32 convention as logos: the mesh is authored in 1/32 units from a top-left origin.
3. Petals, states 3 and 4 only.
4. Six buttons. Button i centre x=-175+70i, y=-80, z=1. Selected (only after arrival): circle 13 + orange label 6+i. Otherwise circle 12 + blue label i.
5. PRESS START sprite 27 at (0,-80), z=1, states 0 and 1 only.
6. Copyright sprite 25 at (-90,-120), sprite 26 at (166,-120), z=1.
7. Logo group: sprite 23 at (-145,45,1), then sprite 24 at (-151,-13,1) with rotation, then sprite 22 at (-145,45,1).

State machine (+0x4ab8):
```
0 logo-in: run logo anim until it returns 1. On first time only play title voice. ->1
1 press start: if ++idle >= 3600 return 7 (attract). Blink sprite 27. On CIRCLE,CROSS,SQUARE,TRIANGLE,START,SELECT: SE(0x29); ->2
2 menu slide: each frame each button x += 20 up to target (start x=-280). Buttons not yet arrived draw unselected. When all arrived ->4
4 menu: RIGHT i=(i+1)%6 SE(0x2a); LEFT i=(i+5)%6 SE(0x2a); CIRCLE: SE(0x29) then
     i==0: play voice "V_G04_MID_0130.hca"; wait=180; ->3
     i==5: run install flow (see below)
     else return i+1
3 start wait: if --wait<1 and voice finished: wait=0; ->4; return 1
```
Petals: drawn in states 3,4 only, updated always. 30 emitters x=-120+16i, base y=140, start y=base+(rand%10)*20. Sprite 14+frame, frame ping-pong every 5 updates (0..6), start frame rand%7, direction rand&1. Colour = one of 0x80ffcdff, 0x80707bf2, 0x8040b7f7, 0x80b0ff96, 0x80ffebbb (rand%5). Rotation Z rand%360 deg, fixed. Speed from rand%10: <5: 0.5+v*0.025, else 0.6+(v-5)*0.025. y-=speed, fades in by 2 per frame up to max (0x80 - rand&0x3f) while y in (-130,140), fades out below -130, reset below -150, drifts left: when y<170 x-= (r=rand%3: speed/((rand&3)+2) | speed | speed*0.5). Scale 1-(rand&3)*0.1. RNG: FUN_000dd6d0 seeded from RTC.
Blink of sprite 27: alpha 255 down by 6 to <0x4c, clamp 0x4b, then up by 6 to 255.
Logo anim (FUN_00097fbc): sprite 24 hinge at logo anchor. Rotates A=0->180 by +18/frame, then ->-24 by -12, then ->0 by +3, then done (return 1). Also spin 22 and 23: Z rotation +18 deg per frame from 0 for one full 360 turn. Rotation is about the anchor: pos = anchor + R(A)*(hinge-anchor).

Result dispatch in update (FUN_0009ab58), result r:
```
map[r] = {1:5, 2:0x1c, 3:0x15, 4:0x1e, 5:0xf, 7:0x20}
r>=0 and r!=6: stop voice if playing; cover(10) (style unchanged); pending=r
when covered:
  5:     stop BGM; clear save buffer (FUN_00001cf8); mode=0; POPALL; PUSH 5
  0x20:  stop BGM; POPALL; PUSH 0x20
  0x15, 0x1e: POPALL; PUSH id
  0x1c, 0xf:  PUSH id (title stays beneath)
onResume: reveal(10)
```
Title is left alone with its BGM playing under load and option.

Install flow, i==5: state object 0xcad20 drives sceUtility install (`INSDATA`, `INSSTRM`). When install completes: cfg.install=1. Port: remove the item or leave it inert [port decision].

## 5. In-game menus

### 5.1 Scene 0xf, option [CONFIRMED]
Textures: option_bg.txg 256x64, option_bg_board.txg 512x256, option_cursor.txg 64x64, option_bg_text.txg 256x128. Sprites 0x1fd760, 26 entries:
```
option_bg (256x64): 0 (0,0)-(64,64) 1 (64,0)-(168,64) 2 (168,0)-(64,64) mirrored
board (512x256):    3 (0,0)-(456,216)  4 (0,216)-(80,256)
cursor (64x64):     5 (0,0)-(24,24) cursor icon  6 (56,0)-(64,20) highlight  7 (0,24)-(56,48) big red ellipse  8 (24,0)-(56,24) small circle
text (256x128):     9..16 labels 104x16 rows at y=0,16,...112
17 遅 (104,0) 18 速 (120,0) 19 無 (136,0) 20 大 (152,0) 16x16 each
21 オン (168,0) 24x16   22 オフ (192,0) 24x16
23 全て (104,16) 24x16  24 既読のみ (128,16) 48x16  25 / (216,0) 16x16
```
Layout:
```
backdrop: sprite 0 tiled 8x5 at (-240+64i, 136-64j) z=-1; sprite 1 (-120,130) z=-1; sprite 2 (120,130) z=-1
board: sprite 3 at (5,-2);  sprite 4 at (175,-75)
labels: sprite 9+row at (-110, 74-21*row), z=1
cursor icon: sprite 5 at (-170, 75-21*cur)
highlight: sprite 6 scale(45,1) at (5, 74-21*cur), alpha 5..0x80 step 3 triangle wave
row 0 Message speed: 遅(-25,74) 速(165,74); sprite 8 at (13+56i, 74) for every i in 0..value
row 1 Skip: 既読のみ(30,53) 全て(100,53), '/' (65,52); ellipse sprite 7 at x=30 if v==0 else 100, y=53
rows 2..5 volumes: 無(-25, 32-21k) 大(165, 32-21k); sprite 8 at (-1+28i, y) for every i in 0..value; y=32 BGM, 11 SE, -10 voice, -31 movie
row 6 空太ボイス: オン(30,-52) オフ(100,-52) '/' (65,-53); ellipse sprite 7 at x=30 if on else 100, y=-52
row 7 install: オン(30,-73) オフ(100,-73) '/' (65,-74); ellipse sprite 7 at x=30 if on else 100, y=-73
```
Help: `○設定変更を決定　×変更せずに戻る　U+FFFD項目の選択　U+FFFC設定の変更`
Header page 7 (common_page_option).
Markers fade in staggered: alpha+10 per frame after a delay; speed delays 0,6,12; skip 3; BGM 6,9..21; SE 9+; voice 12+; movie 15+ (step +5); 空太 0x12; install 0x15.

Input (pressed, no repeat): DOWN cur=(cur+1)%8, UP cur=(cur+7)%8, SE(0x2a).
RIGHT/LEFT change the value: row 0 clamp 0..2; rows 2..5 clamp 0..5; row 1 RIGHT sets 1 LEFT sets 0; rows 6,7 LEFT sets 1 (on), RIGHT sets 0 (off). SE(0x2a) only if changed.
Volume changes apply immediately to audio (setters run on the working copy but drive the mixer).
CIRCLE: commit working copy, SE(0x29), exit. CROSS: restore the copy taken on entry, SE(0x1f), exit.
Exit: cover(10), hide help, POP self, PUSH 4 (which writes SYSTEM.DAT, then POP self).

### 5.2 Scene 0x10, in-game menu [CONFIRMED except status panel]
From ADV FUN_0004f914 (cover style 2, 10 frames): mode 0 pushes 0x10, modes 3..6 push 0x11, other modes open nothing [CONFIRMED]. The quick-load path FUN_0004fbf8 likewise pushes 0x1a or 0x24, backlog FUN_0004fedc pushes 0x1d. The status scene pushes 0x10, 0x1a, 0xd, 0x1f (FUN_000382bc..388bc). Textures: menu_bg.txg, menu_button.txg 512x256, menu_psrts.txg 512x128, menu_img.txg 512x128. Sprites: index 0 menu_bg, 1..33 menu_button (table 0x1ff330), 34..78 menu_psrts (0x1ffd80), 79..88 menu_img (0x200b90).
menu_button sprites, px rect:
```
1 仕様スクエア (0,0,88,56)   2 タスク一覧 (88,0,176,56)  3 キャラ情報 (0,56,88,112)  4 TIPS一覧 (88,56,176,112)
5 セーブ (0,112,88,168)      6 ロード (88,112,176,168)   7 設定 (0,168,88,224)       8 タイトル (88,168,176,224)
9 folder (176,0,288,120)  10 system folder front (176,120,288,240) 11 (288,0,400,120) 12 (352,120,416,176) 13 (288,120,352,176)
14..22 nine-patch frame A  23..30 nine-patch B  31 system folder flap with pin (288,176,352,224) 32 (400,0,512,120) 33 (56,224,72,240)
```
menu_psrts: 34..50 frame parts, 51..60 small digits 16x16, 61 '/' 8x8, 62..71 large digits 16x24, 72 "SCE Ranking" 112x16, 73..75 arrows 24x16, 76 hexagon 128x96, 77 (200,0,256,64), 78 thumb bar 120x24.
menu_img: 79..88 game genre screenshots 96x56.
Items:
```
phase = GameData.phase (dword 0xe1a). unlocked = phase != 0.
Top row: if unlocked, 4 buttons sprite 1+i; if !unlocked, 2 buttons sprite 3+i.
  target x=-180+90i, y=60, z=0. Start x=-280, slide +20 per frame; SE(0x21) on arrival.
System folder: index 4 in cursor space (FUN_00086b34). Pos (183,73), slides in from x=600 at -20/frame in state 0 (FUN_00086d24). Draw FUN_00086bc8: selected -> glow 32 (closed) or 11 (open); open -> back 9; front 10; closed -> flap 31 at (x-16,y+27). No cursor frame on the closed folder.
System row: 4 buttons sprite 5+i at x=-170+90i, y=30, z=1, visible when expanded.
Backdrop: sprite 0 tiled, drawn by FUN_00086030.
```
Update states (+0x1e15c): 0 slide buttons; 1 glints (scale 1.4->1.0 step 0.1, alpha +60, stagger 4 frames); 2 wait panel; 3 expand system row: t+=0.8 to 8; 4 collapse: t-=0.8 to 0; 5 input.
Input in state 5, top: cur space 0..4. RIGHT/LEFT cycle, skipping 2,3 when... if !unlocked, cur 0->1->4->0 (RIGHT at 1 jumps to 4; LEFT at 4 jumps to 1). CIRCLE SE(0x29): cur==4 -> expand; else return cur+1 (unlocked) or cur+3 (!unlocked). CROSS SE(0x1f) return 0.
Input expanded: sub=0..3, RIGHT/LEFT cycle, CIRCLE: sub==3 popup `タイトルへ戻りますか？` two buttons, yes returns 8. Else return sub+5. CROSS SE(0x1f): sub=0, ->4.
Return mapping: 1 scene 0xa (first FUN_0003ce28(0)), 2 0xd, 3 0x1f, 4 0x13, 5 0x1b, 6 0x1c, 7 0xf, 8 title.
Return 0: style 2, POP self, ADV resumes.
Else: style 0, cover(10), PUSH id on covered. For title: reroll voice (FUN_0001aa28), POPALL on root, PUSH 3.
onResume: if style==2 reveal(0) else reveal(10). Header page 8.
Cursor frame: FUN_00087c84 draws a 4-corner frame around the selected button pos.
Status panel at +0x6c0 is shown only when phase>=2 (scale pop 1.1). Fields: score radar hexagon sprite 76, digit groups, genre/ranks from GameData+0x3598. See open items.

### 5.3 Scene 0x12, system menu, phase-less list [CONFIRMED]
Return map: 1 0x1f, 2 0x13, 3 0x1b, 4 0x1c, 5 0xf, 6 title. Popup `タイトルへ戻りますか？` (0x200ef0). Same transition logic as 5.2. Caller not traced [ASSUMED action/status path, no square or task entries].

### 5.4 Scene 0x11, DLC drama menu [CONFIRMED]
4 items, LEFT/RIGHT, cursor +0x524 wraps 0..3. Item 3 uses popup `ダウンロードコンテンツの\nメニューへ戻りますか？`, then exits. Return map: 1 0x25 DLC save, 2 0x26 DLC load, 3 0xf option, 4 POP-to-DLC menu: POP self and POP parent (progress), which returns to scene 3 via empty root. Sprites from menu_button incl. 31.

### 5.5 Scene 0x13, Tips [CONFIRMED structure]
Textures tips_parts.txg 128x64, tips_page.txg 32x32. Data tips_list.tipd (G+0x3b7ac0).
11 pages. Page dots at x=220-16i, y=115. Arrows at (-233,44) and (233,44), alpha triangle 0..255 step 5. Selected page panel at x=-480..., y=80.
Input: UP/DOWN move cursor in the page, LEFT/RIGHT scroll the tip text body, L/R change page, CROSS SE(0x1f) exit, all SE(0x2a).
Tip k (1-based, 9 per page, k=index+9*(page-1)): shown if `global.tips[k]`, opening it sets `global.tipsSeen[k]` so NEW disappears.
Exit: cover(10), POP self.

### 5.6 Scene 0x1d, Backlog [CONFIRMED structure]
Textures backlog_bg.txg 16x32, backlog_parts.txg 128x64, backlog_arrow.txg 64x32. Backdrop drawn in 32px strips at scrolling y, header page 14. Help `○音声再生　×戻る　U+FFFDスクロール`.
Input: CIRCLE or UP/DOWN repeat in list state; CROSS exit SE(0x1f). CIRCLE replays the voice of the current line. See spec_adv for line storage [ASSUMED owned by ADV].

## 6. Popup and help bar

### 6.1 Popup (G+0x307040) [CONFIRMED]
`FUN_000147f0(x,y,obj,text,centerFlag,twoButtons,defaultYes)` then `FUN_000135b0(obj)` opens.
Flags: G+0x3300a4 open, G+0x3300a5 result yes, G+0x3300a6 result no-from-cross.
Geometry: common_pop_window.txg 64x64, 6 columns of 3-slice body. Height grows from 1 by +4 per frame until target, closes by -4 per frame.
Text uses font_m. Lines <3: body y offsets {50,42,24,6,-22,-52} top... lines>=3: {86,78,42,6,-22,-52} and y shifted -16. Target scale h = max((textH/lineH)*2-2, twoButtons?38:20).
Buttons: one button at (x, y-25); two buttons at (x-75,y-25), (x+75,y-25), LEFT/RIGHT, cursor starts on yes unless defaultYes=0.
Input: CIRCLE confirm (SE 0x29 yes or 0x1f no), CROSS cancels (SE 0x1f), LEFT/RIGHT SE(0x2a).

### 6.2 Help bar (G+0x3300c0) [CONFIRMED]
common_help.txg 32x32, common_help_parts.txg 128x64. Bar y=-148 hidden, rises +2/frame to -124, leaves -4/frame.
`FUN_00010600(bar,text)`, FUN_0001052c show, FUN_000105d4(bar,1) hide. Text over 32 full-width chars scrolls as a ticker with 480px wraps.
Inline private-use glyphs in strings: U+FFFA L, U+FFFB R, U+FFFC left/right pad, U+FFFD up/down pad (from option help) [CONFIRMED by context], U+FFFE scroll pad [ASSUMED].

### 6.3 Page header (FUN_00018714 class) [CONFIRMED structure]
`FUN_0001882c(obj,0,0,page)`. common_page_*.txg 256x32 and common_page_decoration.txg. Slides via sine: over 25 frames, y=sin(((f-10)/15)*pi/2... )*32. FUN_00018d14 starts, FUN_00018e94 re-enters. Update FUN_00018ffc, draw part A FUN_0001937c under content, part B FUN_00018a4c over it.
Page index -> name (array 0x228d18): 0 action_choice, 1 calendar, 2 task, 3 square, 4 action1, 5 action2, 6 action3, 7 option, 8 menu, 9 quick_load, 10 save, 11 load, 12 info, 13 tips, 14 backlog, 15 omake_menu, 16 omake_evcg, 17 omake_voice, 18 omake_friend, 19 omake_standup, 20-23 dlc_save01-04, 24-27 dlc_load01-04, 28 dlc.

## 7. Save/load screens

### 7.1 Scene 0x1b Save, 0x1c Load [CONFIRMED]
Both use one class layout. 6 pages of 10 slots. Backdrop: common_bg (G+0x341000) at -1, header page 10 or 11.
Slot grid: 5 columns x 2 rows, `x=-180+90*(i%5)`, `y=16 + 39 - 78*(i/5)` (group translate +16, i.e. y=55 and -23). Colours: save base 0xffeab6ff, accent 0xffa877f0; load base 0xffffd5a9, accent 0xffff8b60.
saveload_parts.txg 256x256 sprites (class vt):
```
vt2394f4 slot base 88x72 (0,0)-(88,72)
vt239524 frame 88x72 (88,0)-(176,72), shown when selected
vt239554 top pill 88x16 (0,88)-(88,104)
vt239584 fill 88x16 (0,72)-(88,88)
vt2395b4 digits 0-9 8x8 at (0..,240), slot number
vt2395e4 small digits+symbols 12 frames 8x8 at (0..,248); 10='/', 11=':'
vt239614 pill 88x16 (0,104)-(88,120)
vt239644 genre tag 7 frames 48x16 x[-41,7]
vt239674 heroine tag 6 frames 48x16 x[-7,41]
vt2396f4 thumbnail frame 72x40.8 uv (0,0)-(0.9375,0.53125) of the 64x64 thumb
vt239724 no data 80x48 (88,72)-(168,120)
```
Slot draw at slot origin, each child at translated y (FUN_000c134c):
```
base: colour empty 0xffd0d0d0, normal = base colour, selected 0xff80e0ff
frame, if selected
y+34: number pill (colour empty 0xffd0d0d0 else white), inner pill (selected 0xff2090ff, empty 0xff808080, else accent)
      slot number at x-35: digits of (page*10+i), 0-based, zero padded, tens at dx-3, ones at dx+3, sprite vt2395b4
      at x+8, only if valid, sprite vt2395e4: year%100 at -29,-24; '/' -20; month -16,-11; '/' -7; day -3,2; hour 10,15; ':' 19; minute 23,28
y-24: tag pill: genre (frame by GameData.genre 0..6) left, heroine right (frame = top affection char, see below)
y+5:  thumbnail (if valid) 72x40.8 else no data sprite
```
Heroine frame: over 6 chars at GameData dword 0xd26 stride 6, take max of field[4] then field[3] (ties keep the earlier one).
Detail panel, translated y=-136, FUN_000bebfc:
```
(-166,56) date box text: "MM月DD日(曜)" etc FUN_000bef90, font size 14, colour black; weekday red 0xff0000ff on Sun/holiday, 0xffff5000 Sat
(-166,34) second text FUN_000bd6b8, game title/plan name [ASSUMED]
(-54,56)  (-54,34) two more text boxes
(43,46)   FUN_000be040 route/genre icons
(155,46)  FUN_000be3f0 clear route: thumbnail icons, ranking
```
Panel slides between empty (FUN_...+0x1a4) and filled with slot validity.
Page indicator (FUN_000bb870) at (216,110): saveload_page.txg 2 frames, 6 positions. Arrows saveload_arrow.txg 2 frames via FUN_000bb5d8 at y+16.

Save states:
```
0 fade: style 0, reveal(10), on clear ->1
1 read: for i in 0..9 read GAME(page*10+i).DAT into a temp, copy first 0xb090 from +0x10 into cache[i].
        Result 0 ok, 1 no file (slot empty), other: stop and go on. ->2
2 appear: slots draw in 2-at-a-time columns, 20 frames (col c drawn when t>2c). ->3
3 input:
  CROSS: ->9 SE(0x1f)
  grid input FUN_000c2604: CIRCLE confirm, LEFT/RIGHT repeat wrap in row, with wrap at edge also page change; UP/DOWN repeat swap rows; L prev page; R next page
  moved: refresh detail, SE(0x2a)
  R or wrap right: dir=+1 ->4; L or wrap left: dir=-1 ->4, SE(0x2a)
  confirm: cache[i] empty ->5 else ->6, SE(0x29)
4 page: slots exit 20 frames; page=(page+dir+6)%6; ->1
5 popup "選択した場所にセーブしますか？" (1,1,1)
6 popup "選択した場所にはすでにセーブデータが存在します。\n上書きセーブしますか？" (1,1,0)
  yes ->7, else ->3
7 write SYSTEM.DAT (0x356c) ->8
8 write GAME(page*10+i).DAT (0x79640) from G+0x526240; on ok lastSlot=page*10+i, cache[i]=slot 0, refresh; ->3
9 cover(10) style 0, hide help; POP self
```
`lastSlot` G+0x5f87bc persists until reboot. On enter page=lastSlot/10, cursor=lastSlot%10.
Help strings:
```
save: ○決定　×キャンセル　￺￻ページ切替　　セーブするスロットを決定してください
load: ○決定　×キャンセル　￺￻ページ切替　　ロードするスロットを決定してください
```
Load differs [CONFIRMED]: confirm valid slot only; popup `選択したセーブデータをロードしますか？`; on yes, cover(10) then copy the file buffer 0x79640 bytes to G+0x526240, lastSlot set, mode(G+0x59f978)=0, and if a progress scene exists: POP it from its parent and PUSH 5 there; else root POPALL, PUSH 5.
Scene 5 onEnter FUN_00001e18 restores GameData and the job stack if slot.valid.

### 7.2 Scene 0x1a Quick load, 0x24 DLC quick load [CONFIRMED]
Slots from snapshot history. Page shows 10 grid entries copied from G+0x526240+0xb0a0+i*0xb090, no file IO. On yes, FUN_00001f18(save,i) copies snapshot i+1 to the current slot, then the same reload as 7.1.
Popup and help as above with クイックロード.

### 7.3 Scenes 0x25/0x26, DLC save/load [CONFIRMED]
One page of 10 slots using files DR0m0n.DAT, m=mode-2, n=0..9 (arrays at 0x228...).

## 8. Save file layout

The port replaces PSP savedata dialogs with files. PSP directory: NPJH50745 + SAVEDATA, files SYSTEM.DAT, GAME00-59.DAT, DR0100-0409.DAT, icon savedata_icon0.png. PSP-side encryption has no effect on this layout.

### 8.1 SYSTEM.DAT, 0x356c bytes [CONFIRMED]
```
+0x000 u32 unused 0
+0x004 u8[0x400] config block
         +00 u32 se 0..5       default 5
         +04 u32 bgm 0..5      default 5
         +08 u32 voice 0..5    default 5
         +0c u32 movie 0..5    default 5
         +10 u32 sorataVoice   default 1
         +14 u32 textSpeed 0..2 default 1
         +18 u32 skipAll       default 0 (0 read only, 1 all)
         +1c u32 install       default 0
+0x404 global block, 0x3168 bytes
         +0000 u32 clearFlags   bit n, native 47 (also sets GameData flag n), bits 1..6 heroine routes
         +0004 u32 a  native 51
         +0008 u32 b  native 52
         +000c u32 c  native 49 (also sets GameData flag set 2)
         +0010 u32 d  native 53
         +0014 u32 e  native 54
         +0018 u32 f  native 55
         +001c u32 g  native 56 (14 bits)
         +0020 u32[0xc35] text read bitset, 100000 bits (msg id)
         +30f4 u32[4] CG unlocked, 0x61+ bits      native 57
         +3104 u32[4] CG seen (clears NEW)
         +3114 u32[4] character event unlocked     native 58
         +3124 u32[4] character event seen
         +3134 u32    standing pose chars unlocked  (11 bits)
         +3138 u32    standing pose seen
         +313c u32    voice gallery chars unlocked  (12 bits)
         +3140 u32    voice gallery seen
         +3144 u32[4] TIPS unlocked (99 ids)
         +3154 u32[4] TIPS seen
         +3164 i32    voice index, init -1
```
FUN_00019a00(clear bit n): sets the bit, then:
n heroine map: bit1 -> char 0, bit2 -> 4, bit3 -> 1, bit4 -> 5, bit5 -> 2, bit6 -> 3: pose[c] and voice[c] unlocked.
Any clear: pose 7,8,9,10 and voice 7,8,9,10,11 unlocked. All six: pose 6 and voice 6 unlocked.
voiceIndex (FUN_0001aa28): first call (value <0) sets 0, later calls = RNG%31. Called on title return and DLC exit. So first boot always gets table entry 0 (`V_G04_MSR_...`, Mashiro).
Bit getter: `words[n>>5] & 1<<(n&31)`.

### 8.2 GAMExx.DAT, 0x79640 bytes [CONFIRMED]
Image of G+0x526240:
```
+0x00000 u32 [4] header, 0 after FUN_00001cf8
+0x00010 snapshot[0]  current
+0x0b0a0 snapshot[1..10]  history (quick load), newest first
```
Snapshot, 0xb090 bytes, FUN_00001f6c:
```
+0x0000 u32 valid = 1
+0x0004 u16 year, month, day, hour, minute, second (local time)
+0x0010 thumbnail 0x4000 bytes: 64x64 RGBA8888 swizzled (16 B x 8 lines, same as TXG).
        Pixel (x,y) for x<59,y<34 = average of framebuffer samples in the 8x8 block at (8x,8y), sampled at step 2 (16 samples), alpha 0xff. Rest zero.
+0x4010 GameData, 0x3c78 bytes, see below
+0x7c88 job stack blob
```
A new snapshot is taken at save points: FUN_00054ec4 in ADV and FUN_0003a160 in the status scene (with history shift FUN_00001e80 at those points), and on carry-over.
FUN_00001e80 shift: slots 9..1 move to 10..2, slot0 copies to slot 1.
Save writes the whole 0x79640, so a save file carries its history.
Job stack blob (FUN_00023c90), records from bottom child of scene 5 to top:
```
{ u32 id; u32 size; u8 data[size] } ... { -1, 0 }
```
Sizes: VM (6) 0xc00, status (8) 4, action (9) 0xc2c, ADV (0xb) 0x1a38, other scenes 0 bytes (no fields).
Restore: for each record, push job id directly (no queue), call its load slot with data.
VM data 0xc00 (FUN_0000a188): +0 stack 0x800 raw, +0x800 sp, +0x804 fp, +0x808 pc (all as word indices relative to base pointers), +0x80c, +0x810 frame limits, +0x814 flags field param[8].
ADV layout (FUN_0004f3a0): +0 0x100 string (script name), +0x100 VM 0xc00, +0xd00.. subsystem blobs, see spec for ADV.

GameData dword map, FUN_000114b8 (offset = dword*4):
```
0     date index (init 3)
1,2
3..8  3 pairs init -1
9     init -1
10    u32[0x80] script vars
0x8a  entries[500] of 6 u32 (task/spec records)
0xc42 6 x 0x88 bytes
0xd0e 6 x 0x10 bytes
0xd26 6 x 0x18 bytes  character params; field[3],[4] affection ranks
0xd4a u32[6]
0xd50..0xd55
0xd56 0x40 bytes string
0xd66 60 x 12 bytes
0xe1a phase (0x3868)
0xe1b genre 0..6 (0x386c)
0xe1c flags bitset (0x3870), natives 47,48
0xe1d
0xe1e u32[0x80]
0xe9e ...
```
Remaining fields continue to 0x3c78; another dword-level doc belongs with GameData spec.
FUN_00001d2c carry-over: after FUN_00001f6c, replaces the snapshot GameData with the pristine copy kept at G+0x34c1bc (FUN_00010c48 stores it after reset), except it keeps from the live data: flags at GameData +0x3870, +0x3874, and field +0x14 of each of the 500 entries. That is the 引き継ぎ data.

### 8.3 DR0mnn.DAT [ASSUMED same as GAME, size 0x79640]
Written from the same buffer.

## 9. Progress scene and mode [CONFIRMED]

G+0x59f978 mode: 0 main, 1 eventview, 2 trial, 3..6 drama1-4.
Scene 5 onEnter:
```
mode 3..6: mount EDATs named "DR01".."DR04" (FUN_0000e62c, FUN_0000e784), unmounted on exit
script file = {system,eventview,trial,drama1..4}_progress.ssb, name kept at 0x242cc0
clear current scenario (FUN_0004e3a8 "")
init loader cache, GameData reset (FUN_00010c78)
if save.valid: restore GameData and job stack (FUN_00001e18)
if no child: PUSH 6
```
Update: update children; if no child POP self.
onExit: unmount.
Note DAT_00228e50 holds pointer to scene 5 while it exists (FUN_000237bc).
Scene 6: loads script, runs op loop once per frame when VM state==0 (FUN_000e835c); state 1 = finished -> POP self.
Natives queue on scene 5, so called scene sits on top of 6 and VM pauses (`yield` after call).
```
64 callCalendar      PUSH 0xc (arg none)
65 callActionChoice  set arg, PUSH 8
66 callAction        PUSH 9
67 ADV(file)         set scenario name, PUSH 0xb
68 callContest       PUSH 0xe
69 callEndRoll       PUSH 0x23
70 callFinish        PUSH 7
71 movie(file)       PUSH 0x27
72 quit              POP self (scene 6) -> script ends -> scene 5 pops -> root pushes 3
73 reset per-round    FUN_00010ef0, FUN_00010f4c; heal chars in state 3
74 periodic stat     see natives doc
```
script_trial_progress: `N67("SCR_TGS_0000.ssb")` then end. Trial title (`title_trial.txgtl`) is not referenced by code, [CONFIRMED by string search] so trial mode has no entry in retail.
Eventview: reads var N43(1,0) as event index, big switch of N67 file names, used by omake 0x18.
Eventview entry, FUN_000ab460 [CONFIRMED]: after cover(10): set var 1 = event id, clear save buffer, mode=1, PUSH 5 on root above scene 0x18, set event seen. When the script ends scene 5 pops and 0x18 resumes (reveal 10).

### Scene 7 carry-over [CONFIRMED]
Native 70. States: fade in(10) -> popup `引き継ぎセーブは、シナリオのクリア情報、\n仕様スクエアのオープン状況を引き継ぎます。` (one button) -> popup `引き継ぎデータをセーブしますか？` (two). Yes: take snapshot+FUN_00001d2c, cover, PUSH 0x1b (save). No: cover, POP self. Backdrop finish_bg.txgtl.

## 10. Omake (scenes 0x15-0x19) [PARTIAL]

### 10.1 Scene 0x15 menu [CONFIRMED]
Textures omake_menu_parts.txg 64x128, omake_menu_name.txg 128x128, omake_menu_img.txg 256x256. Sprite tables 0x2037d0 (+0x550, +0x6e0). 
Items: 1 イベントCG -> 0x19, 2 立ち絵 -> 0x16, 3 キャラ イベント -> 0x18, 4 おまけボイス -> 0x17, 5 TIPS -> 0x13.
UP/DOWN cycle, CIRCLE open, CROSS exit: cover, POP self, reroll voice, PUSH 3.
Enter tests each DLC with FUN_0000e934("DR01".."DR04") to include DLC gallery extras.
Each item shows NEW if some unlocked id is unseen, FUN_0009bb48 etc:
```
CG: ids 0..0x60 plus DLC ids 0x61+
pose: 0..10; event: 0..0x40; voice: 0..11; tips: 0..98
```
Header page 15.

### 10.2 Scene 0x19 CG [CONFIRMED structure]
Thumbs omake_cg_img.txg 512x512, omake_cg_img02.txg 256x256, omake_cg_parts.txg, dlc_omake_cg_img0x. Grid 5 columns, multiple pages sliding by 480px (24px/frame on select). Cell 64x? Thumb +-32. L/R page, D-pad cell, CIRCLE opens if unlocked. Selected id `(row*5+col+page offset)`, 1-based.
Fullscreen viewer FUN_000ad380, special ids use EV_035, 039, 045, 082, 083, 084_C, 084_O, 085 (variant list, `%s.cbg`). First view sets CG seen. SUSECRET.csu silhouette for locked. Header page 16.

### 10.3 Scene 0x16 pose [PARTIAL]
Left list 6+? characters omake_pose_name.txg, row step 32 scrolled, 2 pages with sprites `SU<name>_<pose>.csu` (table in FUN_000a43e4). Text x: 230 and 110 anchor. UP/DOWN repeat scroll -32/+32, LEFT/RIGHT move column, CIRCLE on unlocked entry, CROSS back SE(0x1f). Entering a char sets pose seen. Header page 19.

### 10.4 Scene 0x18 character events [PARTIAL]
Tabs per heroine omake_event_img_<msr,nnm,msk,zin,rns,oth>.txg, names omake_event_name.txg, cells 64x64 (thumb +-32), L/R page 480px, event ids 0..0x40. Selected: see section 9 eventview entry. Header page 18.

### 10.5 Scene 0x17 voice [PARTIAL]
omake_voice_parts.txg 64x128, omake_voice_ch_name.txg 256x256, rows at y=60-32k. 12 chars (0x16280 each), per-char count array at 0x209fd0. UP/DOWN repeat; CIRCLE play voice. Selecting a char sets voice seen. Header page 17.

## 11. DLC menu (scene 0x1e) [CONFIRMED structure]
Textures dlc_parts.txg, dlc_parts_title.txg, dlc_story.txg, dlc_story_name00-04.txg, dlc_img00-04.txg. Sprites 0x211af0, 0x211d20, 0x211d70, 0x211eb0, 0x211f00.
Vertical list, UP/DOWN, CIRCLE, CROSS. Checks DR01..04 with key file via FUN_0000e934. Stops at 4 dramas + store note.
Result: clears save (FUN_00001cf8), POP self; r=1..4 sets mode 3..6, POPALL, PUSH 5. CROSS: POP self, PUSH 3.
Port: DLC EDATs are not in the ISO, so show entries as unavailable.

## 12. Info (scene 0x1f) [PARTIAL]
Textures information_tab.txg, information_parts.txg, information_font_name.txg, information_font.txg, information_ch_{msr,nnm,msk,zin,rns,oth}.txg. Six tabs, header page 12. Not fully read, see open items.

## 13. Open items
- Status panel in the in-game menu (object +0x6c0, FUN_000895ac, FUN_00089ee0): uses menu_psrts; only high-level recorded.
- Omake tile coordinates, pose/voice list scrolling, CG variant viewer, info scene internals: read FUN_000ae888 group, FUN_000a5710, FUN_000b3e44, FUN_000d6da8 group.

## Appendix A. Title and logo voice tables
Each has 31 entries indexed by voiceIndex. Title table 0x229fb8:
V_G04_MSR_0040, SRT_0040c, SRT_0040d, SRT_0040e, SRT_0040f, NNM_0040a, NNM_0040b, MSK_0010a, MSK_0010c, MSK_0010d, JIN_0040a, JIN_0040c, JIN_0040e, RNS_0040, OTH_0040a, OTH_0040b, OTH_0040c, SNG_0040a, SNG_0040b, MID_0040, RIT_0040, YUK_0040, CHR_0040b, KHR_0040a, KHR_0040b, SIR_0040, SOR_0040a, SOR_0040b, KZK_0040, DIC_0040, CAT_0090.
Logo tables 0x22b2d0: MSR_0010a, SRT_0010b, SRT_0010c, SRT_0010e, SRT_0010f, NNM_0010a, NNM_0010c, MSK_0020a, MSK_0020c, MSK_0020d, JIN_0010b, JIN_0010c, JIN_0010e, RNS_0010, OTH_0010a, OTH_0010b, OTH_0010c, SNG_0010a, SNG_0010b, MID_0010, RIT_0010, YUK_0010, CHR_0010b, KHR_0010a, KHR_0010b, SIR_0010, SOR_0010a, SOR_0010b, KZK_0010, DIC_0010, CAT_0060.
0x22b34c: MSR_0020, SRT_0020b, SRT_0020d, SRT_0020e, SRT_0020f, NNM_0020a, NNM_0020c, MSK_0030a, MSK_0030c, MSK_0030d, JIN_0020a, JIN_0020d, JIN_0020e, RNS_0020, OTH_0020a, OTH_0020b, OTH_0020c, SNG_0020a, SNG_0020b, MID_0020, RIT_0020, YUK_0020, CHR_0020b, KHR_0020a, KHR_0020b, SIR_0020, SOR_0020a, SOR_0020b, KZK_0020, DIC_0020, CAT_0070.
0x22b3c8: MSR_0030, SRT_0030a, SRT_0030d, SRT_0030e, SRT_0030f, NNM_0030a, NNM_0030b, MSK_0040a, MSK_0040c, MSK_0040d, JIN_0030a, JIN_0030d, JIN_0030e, RNS_0030, OTH_0030a, OTH_0030b, OTH_0030c, SNG_0030a, SNG_0030b, MID_0030, RIT_0030, YUK_0030, CHR_0030b, KHR_0030a, KHR_0030b, SIR_0030, SOR_0030a, SOR_0030b, KZK_0030, DIC_0030, CAT_0080.
All prefixed `V_G04_`, suffix `.hca`.
