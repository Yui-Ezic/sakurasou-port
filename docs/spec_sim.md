# Sakurasou PSP: game-production simulation spec

Scope: everything around the story: GameData, tables, calendar, action choice, action scene, contest ranking, slump, ending, game over, and `script_system_progress.ssb`.

Tags: **[CONFIRMED]** read directly from the decompile, the SSB bytecode or parsed data. **[ASSUMED]** inferred, not proven. Addresses are Ghidra image-base 0 (vaddr = file offset - 0xc0 in `EBOOT.BIN.dec`). `base` = `FUN_000164b8()` game object. All integers are signed 32-bit LE unless noted; all screens are 480x272, and scene coordinates use a centred origin (x -240..240, y -136..136, +y up) [CONFIRMED: ortho call `-240,240,-136,136,-1,1` in every scene draw].

Spec correctness note: **`tools/ssbpretty.py` has comparison ops 66-69 inverted and does not show stack-slot locals.** Real semantics [CONFIRMED FUN_000e8214/8264/82b4/8308]: 66 `>`, 67 `<`, 68 `>=`, 69 `<=` (with `a` pushed first, `a op b`). Op 59 is unary plus (no-op), 60 negate, 63 logical not, 56 bitwise not. Ops 20..39 are compound assigns (even=local, odd=global; pairs in order `+= -= *= /= %= <<= >>= |= &= ^=`), ops 40..47 are `++x --x x++ x--`. A value left on the stack at statement level *is* the local variable: there is no declaration opcode. All script listings below come from a stack-aware lifter and use the corrected operators.

## 0. Vocabulary

Character index (all per-character arrays, `FUN_` tables, `N*` natives) [CONFIRMED string table 0x229d24, sprite order 0x11ebd4]:

| i | name | sprite tag | stat "tot" index for route |
|---|---|---|---|
| 0 | ましろ Mashiro | msr | tot0 |
| 1 | 美咲 Misaki | msk | tot3 |
| 2 | 七海 Nanami | nnm | tot5 |
| 3 | 仁 Jin | zin | tot4 |
| 4 | 龍之介 Ryunosuke | rns | tot1 |
| 5 | 乙羽 Otoha | oth | tot2 |

Route id (`routeB`, GameData +0x386c) [CONFIRMED from F992/F23e0/turn 97]: 0 undecided, 1 Mashiro, 2 Ryunosuke, 3 Nanami, 4 Otoha, 5 Misaki, 6 Jin.

Phase (`routeA`, +0x3868): 0..8, gates tasks. Set only by the progress script (table in section 8.2).

Character work state (+0x3438): 0 normal, 1 ヤル気最高潮 (high), 2 悩み中 (slump), 3 作業不可 (sick) [CONFIRMED strings at 0x1f092c..].

The name "stat" below uses these six per-character fields (GameData +0x3498 row):
`bio` (A), `mood` (B), `health` (C), `affinity` (D), `eventCount` (E), `love` (F). The English names are mine [ASSUMED]; the semantics are [CONFIRMED].

## 1. GameData (base+0x348544, size 0x3c78) [CONFIRMED]

Init `FUN_00010c78`, copied to a backup at +0x3c78 by `FUN_000114b8` (member-wise copy of the same layout). Natural struct:

| off | type | name | init | meaning |
|---|---|---|---|---|
| 0x0000 | i32 | turn | 3 | Day index. N10 reads. N73 increments. |
| 0x0004 | i32 | rankStage | 0 | Set by N16. Read back by `FUN_00010f2c`. |
| 0x0008 | i32 | period | 0 | 0..2 within a day; N11 reads, `FUN_00010f3c` ++ in the action scene, N73 zeroes. |
| 0x000c | {i32 kind, i32 target}[3] | plan | -1,-1 | Plan for periods 0..2. Written by action choice (`FUN_00010f5c`). |
| 0x0024 | i32 | selectedTask | -1 | Task Sorata is authoring (`FUN_00010f8c/94`). |
| 0x0028 | i32[128] | slumpSeen | 0 | One per slump record (mgr uses `int*` stride 4). |
| 0x0228 | TaskRT[500] | taskRT | 0 | 0x18 bytes, see section 2.1. |
| 0x3108 | CharQueue[6] | queue | 0 | 0x88 bytes: i32 task[32]; i32 count@0x80; i32 doneIndex@0x84. |
| 0x3438 | CharState[6] | charState | 0 | 0x10 bytes: state, sickTurns, moodIcon, healthIcon. |
| 0x3498 | CharStat[6] | stat | see below | 0x18 bytes: bio, mood, health, affinity, eventCount, love. |
| 0x3528 | i32[6] | available | 1 | N35/N36. 0 = 不在. |
| 0x3540 | i32[6] | total | 0 | Six game-quality totals, N37..N42. |
| 0x3558 | char[64] | gameTitle | team0.title | Copied from team record +0x40. |
| 0x3598 | TeamRT[60] | teamRT | 0 | 0xc bytes: prevRank, score, rank. |
| 0x3868 | i32 | routeA / phase | 0 | N12/N13 |
| 0x386c | i32 | routeB / route | 0 | N14/N15 |
| 0x3870 | u32 | clearBits | 0 | N47 sets, N48 tests (bit = arg). |
| 0x3874 | u32 | badBits | 0 | N49 sets, N50 tests. |
| 0x3878 | u32[128] | flags | 0 | N43/N44, 4096 bits, `word=n>>5`, `bit=n&31`. |
| 0x3a78 | i32[128] | vars | 0 | N45/N46. |

CharStat init: bio=mood=health=affinity=0, **eventCount=1**, love=0 (the `puVar4[0xd2a]=1` row).
Startup also does: `taskRT[0].unlocked=1`, then `update(taskRT[0])`, then update all, then `checkPrereq` all.

`FUN_00011088` writes +0x3498 row[0], `FUN_000110b8`,`110f0`,`11128`,`11198` add to rows [1],[2],[3],[5], `FUN_00011160` increments row[4]. There is **no clamp** anywhere on these fields [CONFIRMED: no other writers in the natives or script].

### 1.1 Natives 0-58 (base handler vtable 0x22cbc4)

Args are in push order. `*ret` is the pushed result, all return 0 (= done) unless stated. The stat accessors take a character index.

| N | semantics |
|---|---|
| 0,1,2 | debug print (string / int / ?). No-op in release. ret 0. |
| 3 | `wait(ms)`: `timer = ms*60/1000` frames. |
| 4 | Wait: if timer==0 ret 0 (done), else `timer--` and native returns 1 (suspend) [CONFIRMED raw 0x45e0]. |
| 5 | Debug var get: **always returns 0**. |
| 6 | Debug var set: **no-op**. |
| 7 | `rand()`: LCG `s = s*0x41c64e6d + 0x3039; return (s>>16)&0x7fff`, state base+0x59f980, seeded from time at boot. |
| 8 | `randr(lo,hi)`: `rand() % (hi-lo+1) + lo` |
| 9 | game mode `base+0x59f97c` |
| 10 | turn |
| 11 | period |
| 12 / 13 | get / set routeA (phase) |
| 14 / 15 | get / set routeB (route) |
| 16 | `setRankStage(s)`: GameData.rankStage=s, then run contest `FUN_0001c314(s)` (section 5). |
| 17 | player's current rank `teamRT[0].rank` |
| 18 | player's score `teamRT[0].score` |
| 19 | taskDone(i): `taskRT[i].state > 4` as 0/1 |
| 20 | remaining queue `queue[c].count - queue[c].doneIndex` |
| 21,22 | charState[c].state, .sickTurns |
| 23..28 | stat[c].bio, mood, health, affinity, eventCount, love |
| 29,30,31,33 | add(c, v) to mood, health, affinity, love |
| 32 | eventCount[c]++ |
| 34 | set charState[c].state=0 (`FUN_00011010(c,0)`), no message |
| 35,36 | set/get available[c] |
| 37..42 | total[0..5] |
| 43,44 | set/get flags[n] |
| 45,46 | set/get vars[n] |
| 47 | `clearBits |= 1<<r` and SaveMgr.clear(r) |
| 48 | test clearBits bit r |
| 49 | `badBits |= 1<<c` and SaveMgr +0xc bit c |
| 50 | test badBits bit c |
| 51,52,53,54,55 | SaveMgr set bit at +0x4,+0x8,+0x10,+0x14,+0x18 (each bit=arg) |
| 56 | SaveMgr +0x1c bit (apartment event seen, ids 0..13; a "complete" loop of 14 has no side effect) |
| 57 | SaveMgr +0x30f4 bitset (scenario-seen id, 97 checked) |
| 58 | SaveMgr +0x3114 bitset (event gallery id) |
| 59..63 | no-ops returning 0 (raw `sw 0,(ret); jr; li v0,0` at 0x5654..0x5684) [CONFIRMED]. Presumably stripped trophies [ASSUMED]. |

SaveMgr = `base+0x3453dc` (`FUN_00019900`), persistent across playthroughs. `clear(r)` (`FUN_00019a00`): sets bit r in +0; r=1,2,3,4,5,6 also sets unlock bits in +0x3134 and `FUN_0001a5f8` lists: r1→0, r2→4, r3→1, r4→5, r5→2, r6→3. If any route is clear: unlock 7,8,9,10 (and `1a5f8` 7..11). If all six clear: unlock 6. Layout: +0x00 clear bits, +0x04..+0x1c seven u32 bitsets, +0x20 i32[0xc35] (read flags), +0x30f4/+0x3104/+0x3114/+0x3124 u32[4] sets, +0x3134/+0x3144/+0x3138.. small sets, last i32 at index 0xc59 init -1.

### 1.2 Natives 64-74 (progress handler 0x22d99c)

| N | semantics |
|---|---|
| 64 | Push job 0x0c Calendar |
| 65(mode) | `actionMode=mode`, push job 8 Action choice |
| 66 | Push job 9 Action scene |
| 67(file) | set scenario file, push job 0x0b Scenario |
| 68 | Push job 0x0e Contest |
| 69 | Push job 0x23 Ending movie |
| 70 | Push job 7 Finish |
| 71(file) | push job 0x27 (DLC/story pmf) |
| 72 | Pop the current job (queued) |
| 73 | `turn++; period=0; for c: if state==3: sickTurns = max(sickTurns-1,0)` |
| 74(c,a1,p1,a2,p2) | `bio = (int)(a1*sin(2π*turn/p1) + 0 + a2*sin(2π*turn/p2))`, truncating, floats, turn is the current value (before N73 in the loop). |

Jobs push on the scene stack: progress job yields a frame, so script execution resumes only after the pushed job pops.

Natives 75-78 (handler 0x22f484, only meaningful inside the action-choice job, see section 4): 
- N75(c,s): old=state[c]; state[c]=s; then queue a notice: s==1→type0/sub0, s==3→type0/sub2, s==2→type0/sub1, else if old==1 && s==0→type0/sub3, old==3 && s==0→type0/sub4. Notice is `{i32 type,i32 sub,i32 arg=c}`, ring capacity 0x20.
- N76(c,t): sickTurns=t. N77(c,v): moodIcon=v. N78(c,v): healthIcon=v.

Natives 79-86 (handler 0x22e30c, only inside action scene):
- N79(file): start sub-scenario job 0xb via the job-launcher (`FUN_00028238`).
- N80 / N82: suspend while launcher is active, then ret 0.
- N81(char): pick and start slump scenario (section 6).
- N83 → soraAction; N84 → meetingTarget; N85 → breakTarget; N86(c) → charAction[c].

Launcher (`+0x125694`): idle (st0) → fade-in? state; on `FUN_00028238(job)`: enter state 2, fade out 10 frames + stop BGM, when done push job; state 3 returns: on resume fade BGM SK_BGM_02 back in over 10 frames, then idle [CONFIRMED FUN_00028400..285dc].

## 2. Tables

All files are in DATA.CPK, loaded at boot `FUN_000175b0`, except `task_table.tskt` (loaded by task chart). Every layout below was validated by parsing the full file in Python: file size exact, all references in range.

### 2.1 task_list.tskd [CONFIRMED]

Header: `"TSKD", i32 count=480`. Record 0x14c bytes:

| off | type | field |
|---|---|---|
| 0x000 | i32 | owner: character who executes the work (0..5) |
| 0x004 | char[64] | name (UTF-8, with `\n`) |
| 0x044 | char[64] | shortName (with `\n`, stripped when shown) |
| 0x084 | char[64] | description |
| 0x0c4 | i32 | planCost: units Sorata needs (1,2,3,6) |
| 0x0c8 | i32 | workCost: units owner needs (6..29) |
| 0x0cc | i32 | gridX (0..26) |
| 0x0d0 | i32 | gridY (0..27) |
| 0x0d4 | {i32 task,i32 dx,i32 dy}[4] | children unlocked when plan completes; list ends at task<0. dx,dy are the grid offset (verified for all 479 links). Drawn lines: (dx*104, dy*-72). |
| 0x104 | {i32 task,i32 kind}[4] | prerequisites, end at task<0. kind 0=child plan done, 1=work done. |
| 0x124 | i32[6] | gains added to total[0..5] when work finishes |
| 0x13c | i32 | workAnim: action sprite anim index for owner, values 1..5 |
| 0x140 | i32 | minPhase: required `routeA >= minPhase` (0,2..7) |
| 0x144 | u32 | routeMask: bit `routeB` must be set (1=common, 127=all routes...) |
| 0x148 | i32 | key: 1 for 54 route key tasks (βキー etc), passed to the completion toast as a bold flag |


TaskRT (GameData +0x228, 0x18 bytes):

| off | name | meaning |
|---|---|---|
| 0 | state | 0 hidden, 1 selectable, 2 planning, 3 plan done, 4 working, 5 complete |
| 4 | visible | >0 revealed by a parent plan |
| 8 | prereqOK | sticky 1 when prerequisites hold |
| 0xc | planDone | units |
| 0x10 | workDone | units |
| 0x14 | planFinished | set when state goes 2→3. Survives carry-over (section 12): this is the "仕様スクエアのオープン状況" |

`update(t)` `FUN_0001b690`, exact order, each `if` sees results of the previous:
```
if state==0 && visible>0: state=1
if state==1:
   if planCost<1: state=3
   elif planDone>0: state=2
if state==2 && planDone>=planCost:
   state=3; planFinished=1
     (then a loop scans tasks for planFinished==0; it only exits early, no side effect)
if state==3:
   if workCost<1: state=5
   elif workDone>0: state=4
if state==4 && workDone>=workCost: state=5
```
`checkPrereq(t)` `FUN_0001b564`: if prereqOK>=1 return. For each req with task>=0 (stop at first negative): kind0 needs `other.planDone>=other.planCost`, kind1 needs `other.workDone>=other.workCost`. All pass → prereqOK=1.
`revealChildren(t)` `FUN_0001b4e8`: for each child task>=0: `child.visible=1`.
`stateAtLeast3` = planComplete `FUN_0001b840`: state>2. `FUN_0001b854` = state>4.
`isSelectable(t)` (used by chart and "any selectable" test): `prereqOK>0 && minPhase<=routeA && (routeMask>>routeB)&1 && state==1`.

### 2.2 task_table.tskt [CONFIRMED]

Header 0x10: `"TSKT", i32 cols=27, i32 rows=28, u32 unknown=18753`, then `i32 cell[rows][cols]` (rows*cols*4 = 3024) with task id or -1. Every 480 tasks appears exactly once, at `(gridX,gridY)`. Lookup `FUN_00041ec0(x,y)`.

### 2.3 team_list.tmd [CONFIRMED]

Header `"TMD\0", i32 count=51`. Record 0x120:

| off | type | field |
|---|---|---|
| 0x00 | char[64] | teamName |
| 0x40 | char[64] | gameTitle |
| 0x80 | char[64] | genre |
| 0xc0 | i32[12] | scoreMax per stage |
| 0xf0 | i32[12] | scoreMin per stage |

Team 0 is the player (min=max target `10,20,30,40,60,60,200,250,300,350,400,440`), team 1 = スイコーゲームサークル マーズ・ギア (rival, 0s), rest random rivals. A team with `scoreMax[stage]==0` has dropped out for the stage.

TeamRT (GameData +0x3598, 0xc): `prevRank`, `score` (display value), `rank`.

### 2.4 team_rank.rkd [CONFIRMED]

Unit 0xd4 bytes, total `0xd4*(count+1)`. Header: `"RKD\0", i32 count=24, i32 scoreThreshold[51]` (descending, last is 0). Table k at file `0xd4*(k+1)`: `i32 stage, i32 kind, i32 value[51]`. kind 0: player rank to award; kind 1: rival's rank. Index with `k = stage*2 + kind`, stages 0..11.

### 2.5 slump.slpd [CONFIRMED]

Header `"SLPD", i32 count=120`. Record 0x5c:

| off | type | field |
|---|---|---|
| 0x00 | char[64] | scenario file |
| 0x40 | i32 | character (20 records each) |
| 0x44 | i32 | minTurn |
| 0x48 | i32 | maxTurn inclusive |
| 0x4c | i32 | workdayOnly: 1 = forbidden on days with calendar `dayOff!=0` (5 records) |
| 0x50 | u32 | period mask (bit period; values 7,3) |
| 0x54 | i32 | repeatable (24 records) |
| 0x58 | i32 | weight (7 or 3) |

`slumpSeen[index]` counts plays.

### 2.6 comment_list.cmed [CONFIRMED]

Header `"CMED", i32 count=598`. Record 0x15c:

| off | type | field |
|---|---|---|
| 0x000 | i32 | commenter id (0..36, 37 people) |
| 0x004 | char[64] | commenter text `name\nage job` |
| 0x044 | char[0x108] | comment body |
| 0x14c | i32 | sentiment 0 good, 1 bad (299 each) |
| 0x150 | i32 | stageBucket 0 any, 1..4 |
| 0x154 | i32 | tag 0 general, 1..6 total-index+1 (30 each) |
| 0x158 | i32 | route 0 general, else routeB (38 each) |

Mgr `FUN_0000bf98`: count, maxId, pointers.

### 2.7 tips_list.tipd [CONFIRMED]

Header `"TIPD", i32 count=99`. Record 0x188: `i32 index, i32 id, char title[64], char reading[64], char body[256]`. `id` lookup `FUN_0001cbb4`. Unlock is done from scenario natives (out of this spec). Display object `FUN_0001ece0`, `tips_window.txg`.

### 2.8 Turn calendar, code data 0x1e7838 [CONFIRMED]

Array of 16-byte rows indexed by turn (0..102): `i32 month, i32 day, i32 weekday (0=Sun), u8 holidayRed, u8 dayOff, u16 0`. Turn 3 = 11/12. Note dates are contiguous; turn 53 = 1/1. The two flag bytes: 1 at weekends and winter break days. Row list in the file; simple rule for rendering: read the table, do not derive.

## 3. Input and shared widgets

`FUN_000dd420` = held, `FUN_000dd430` = pressed, `FUN_000dd450` = repeat, on the pad object `base+0x2fefc0` [ASSUMED names, CONFIRMED by usage]. Masks follow PSP `sceCtrl`: SELECT 0x1, START 0x8, UP 0x10, RIGHT 0x20, DOWN 0x40, LEFT 0x80, L 0x100, R 0x200, TRIANGLE 0x1000, CIRCLE 0x2000, CROSS 0x4000, SQUARE 0x8000. `FUN_0001f9a4(base+0x2ff000,bit)` is a touch/pointer object (bit 1 held, 8 tap, 0x10/0x20 drag start/end, 0x40 swipe; +0x2ff020 pos, +0x2ff000 press pos, +0x2ff010 prev) [CONFIRMED usage; the PSP build has no touch, it reads as idle, usable for mouse on PC].

Screen fade `base+0x2ff080`: `FUN_00020fec(mode)` picks colour mode (0 black, 2 white?, 3 ? [ASSUMED]); `FUN_00020f78(n)` fade in over n frames; `FUN_00020f2c(n)` fade out; `FUN_00020fd8` fade-in done; `FUN_00020fc4` fade-out done.

Sound: SE `FUN_0001b3f8`, BGM `FUN_0001b3f0` with file name and fade 10, voice `FUN_0001b418`. SE cue ids in sim: 0x29 confirm (cursor select), 0x2a cursor move, 0x1f cancel [ASSUMED names; ids CONFIRMED]. BGM: action SK_BGM_02.adx, action choice SK_BGM_02.adx, task chart SK_BGM_03.adx, contest SK_ME_01..03.adx. **No `SE_DME_*`, quiz, card-game or combo logic exists in EBOOT or in any SSB script** (string scan of the EBOOT and all `.ssb` found none) [CONFIRMED]. `sound_se.awb` cue names were not examined. `action_parts_game.txg` and `action_parts_quick.txg` are only sprite sheets for the action status panel; there is no separate minigame.

Dialog yes/no `base+0x307040`: `FUN_000147f0(0,0,obj,text,1,hasNo,1)`, open with `FUN_000135b0`. Result bytes: `+0x3300a4` still open, `+0x3300a5` yes, `+0x3300a6` no [CONFIRMED usage in FUN_00037ef8].

Sprite sheet getter pattern: small vfuncs in 0x1c1a5c..0x1d0810 return `(texture, quadCount, 4, vertexTable, tableEnd)`. Vertex row is 20 bytes `float u,v,x,y,z`, four per quad (triangle strip). Sheets [CONFIRMED]:

| tex | quads | table |
|---|---|---|
| action_ch_srt.txg | 16 | 0x1ec228 |
| action_ch_msr.txg | 16 | 0x1ec730 |
| action_ch_nnm.txg | 20 | 0x1ecc38 |
| action_ch_msk.txg | 16 | 0x1ed280 |
| action_ch_zin.txg | 14 | 0x1ed788 |
| action_ch_rns.txg | 14 | 0x1edbf0 |
| action_ch_oth.txg | 17 | 0x1ee058 |
| action_window.txg | 1 + 7 | 0x1eb2c4, 0x1eb374 |
| action_window text strip | 512x16 3x3 quads | 0x1eb5ac |
| action_parts.txg | 1 | 0x1eba30 |
| action_parts text | 512x32 | 0x1eba88 |
| action_parts_icon.txg | 7 | 0x1ebb64 |
| action_parts_game.txg | 1 + 6 | 0x1ebd9c, 0x1ebdf4 |
| action_parts_quick.txg | 1 | 0x1ebfdc |
| action_icon.txg | 8 | 0x1eacc8 |

## 4. Action choice (job 8) [CONFIRMED]

Scene size 0xed9c0; ctor `FUN_00036d98`; init `FUN_00038f04`; job vtable 0x22f41c. `FUN_00036d8c(mode)` sets static mode before push (N65). Mode meaning: 2 tutorial (only 仕様作成 enabled), 0 workday (3 items), 1 day off (4 items, adds お休み).

Sub-states (object pairs at +0xad0b0, `{owner,vtable}`, `enter`/`update`):

| # | enter | update | behaviour |
|---|---|---|---|
| S0 | 37c2c | 39944 | Clear notice ring. If turn==0x0e/0x1d/0x2b/0x3b/0x4e, queue notice type1 sub0/1/2/3/4. Then run `script_system_status.ssb` to completion synchronously (loop until VM done), go S1. |
| S1 | 37c34 | 37d24 | Enter: reset cards; fade mode 0, fade in 10, play SK_BGM_02 (10). Wait fade-in, then S2. |
| S2 | 37da4 | 37dac | Frame counter f: f==0 slide card 0 in, f==20 card 1, f==40 card 2. At f>=60 go S3. |
| S3 | 37e88 | 39da8 | Notice queue: while empty of active popup, take next notice and show toast (below). When ring is empty: period cursor=0, go S4. |
| S4 | 37e90 | 3a160 | Main input (below). |
| S5 | 37e98 | 37ef8 | Yes/no `これでよろしいですか？`. Yes: copy three cards into GameData.plan, S6. No or cancel: cursor=2, card 2 active, S4. |
| S6 | 381cc | 38218 | Fade mode 0, out 10, pop job. |
| S7 | 38270 | 382bc | Fade mode 2 out 10, then push job 0x10 (Menu), S8, S9, back to S4 on fade in. |
| S10..12 | | | Same pattern with mode 0, job 0x1a. |
| S13..15 | | | Same pattern, job 0x0d (task list). |
| S16..18 | | | Same pattern, job 0x1f (information). Not entered by S4 code; entry not found. |

Every sub-state calls `FUN_0003796c`: tick cards. Draw `FUN_0003947c`: background, card layouts:
`for i: tx = i*160-160`; list base y=+50; selection cards `FUN_00030754` offset (0,-22), submenus (16,-60); menu rows `y=-20 - row*32`, hit box ±60x ±16y; plan window translated (0,50), help window (0,104).

S4 input, priority top-down:
1. Not L held, TRIANGLE pressed (or swipe with dy<=-20): snapshot GameData (`FUN_00001f6c`) and S7 (menu).
2. SELECT pressed: SE 0x29, S13 (task list).
3. L held + TRIANGLE: snapshot GameData (`FUN_00001f6c`), push it into the 10-slot snapshot history (`FUN_00001e80`, slots 0xb090 bytes, shifted down), show notice `FUN_00014f20`, SE 0x29.
4. L held + SQUARE: S10, SE 0x29.
5. Else run card input on active card (`FUN_00030b34`); card's chosen `kind`/`target` go to slot display `FUN_0002fb10/2fb64`. Card signals: advance → period++ (next card focus; at 3 go S5); back (`FUN_00030b88`) → if period>=1 period-- and refocus, else re-enter the first menu.

Card menu `FUN_00033bec` (state at +0x138/0x144):
Items count by mode: 3,4,3 (mode 2 disables rows 1,2,3 via `FUN_00031550(row,0)`). Row meanings kind: 0 仕様作成, 1 ミーティング, 2 お世話, 3 お休み.
UP/DOWN wrap via modular arithmetic (cursor<0 → DOWN=0, UP=count-1). CIRCLE: if cursor<0 set 0; else if row enabled: set +0x11a=1 then +0x119=1. CROSS: back path. Rows 1 and 3 then open a character list (`FUN_00032668..`), giving `target`.

Help text: mode 1 0x1f0764, others 0x1f0818 (mode 1 includes `[お休み]相手の健康大回復`).

Toast texts: notice type0: sub0 `%sがヤル気最高潮になりました`, sub1 `%sが悩み始めました`, sub2 `%sが作業不可になりました`, sub3 `%sがヤル気最高潮から戻りました`, sub4 `%sが作業不可から回復しました`, arg `name[c]`, icon index c+2. Type1 text: sub0 `α版制作期間タスク\nタスクの[LOCK]が解除されました`, 1 α後半, 2 β版, 3 β後半, 4 マスター版, icon 0.

### 4.1 Status script (script_system_status.ssb)

Run once per action choice:
```
for c in 0..5:
  sum = bio[c] + mood[c]
  h   = health[c]
  moodIcon[c]   = sum<=-50?2 : sum>=50?0 : 1
  healthIcon[c] = h<=-50?2 : h>=50?0 : 1
  if available[c]==0: continue
  s = state[c]
  if s==1: if sum<50: setState(c,0)
  elif s==3: if sickTurns[c]<=0: setState(c,0)
  s = state[c]
  if s==0 or s==1:
    if queueRemaining(c)>0:
       table P for slump: sum<=-100: always
          <=-90:1/2 <=-80:1/3 <=-70:1/3 <=-60:1/4 <=-50:1/4 <=0:1/70 ; sum>=50:1/8 ; else (1..49) none
       each probability test is `randr(1,N)<=1`, chain is if/elif on the first true threshold. Success: setState(c,2)
  s = state[c]
  if s==0 and sum>=50: setState(c,1)
  s = state[c]
  if s==0:
     h<=-100: setState(c,3), sickTurns=3
     h<=-90 : p 1/2 → 3 turns
     h<=-80 : p 1/4 → 2 turns
     h<=-70 : p 1/8 → 2 turns
     h<=-60 : p 1/16 → 1 turn
     h<=-50 : p 1/32 → 1 turn
```
`setState` is N75 with its notice side effect. `N22` check uses `<=0`. The trailing F2a5a is debug range checks only: drop.

## 5. Action scene (job 9) [CONFIRMED]

Scene size 0x165f00, ctor `FUN_00024e64`, init `FUN_000287d0`. VM on `script_system_action.ssb` at +0x1256c0 (handler at +0x125eec). One job run handles all 3 periods.

Objects: bg +0x11b0d8 (`action_bg.txgtl` / `action_bg02.txgtl` per FUN_24540(mode)); sorata sprite +0x11e870; heroes +0x11e8ec.. ordered in table +0x11ebd4 `[msr,msk,nnm,zin,rns,oth]`; status panel +0x11ebec; toast +0x11f5e0; shutter +0x11f900.

States (+0x125648 chain, enter/update/draw):

| # | enter | update | draw | |
|---|---|---|---|---|
| A1 | 27644 | 2770c | - | Fade mode 0, fade in 10, if BGM not playing start SK_BGM_02. On done: if period<3 A3 else A2. |
| A2 | 277e8 | 2784c | - | Fade out 10, stop BGM(10), then pop job. |
| A3 | 278a0 | 279c8 | - | Shutter text: period 0/1/2 anim 4/5/6 (`FUN_00018eec`), play shutter open `FUN_00018ed0`, `FUN_00018d14(,1)`. If period==2, `FUN_00024540(bg,1)` (evening bg). Counter 30 frames → A4. |
| A4 | 27a34 | 28bc8 | - | If plan[period].kind==0: if selectedTask<0 or its plan is complete, push job 10 (task chart, mode 1) via launcher. Update waits launcher idle then decides actions (5.1) → A5. |
| A5 | 28fc8 | 27b24 | 27c14 | Set sprites and panel (5.2). Update ticks sprites and panel; when panel finished → A6. Draw: if soraAction!=8 draw sorata; draw six heroes; draw shadow/icons; panel at (0,-140). |
| A6 | 27ec4 | 27eec | - | Reset VM, run per frame until finish → A7. |
| A7 | 27f98 | 2a0dc | 27fd0 | Reset log ring, `FUN_0002654c` (5.3). Update: toast idle, ring empty → A8; else pop next and show toast. |
| A8 | 28014 | 2804c | - | `FUN_00018e80` close shutter, counter ≥30 and shutter state 2 or 4: period++; period<3 → A3 else A2. |

`FUN_00027254`/`25ec0` also persist state for suspend: `soraAction,target,target2,codes` plus job-launcher state at save offsets 0xc00.. [CONFIRMED].

### 5.1 Action decision `FUN_00028bc8`

```
for c in 0..5:
  if available[c]==0: code=10
  else switch state[c]:
    0: code = queueRemaining(c)>0 ? 2 : 0
    1: code = queueRemaining(c)>0 ? 4 : 0
    2: code = 6
    3: code = 8
meetingTarget=-1; breakTarget=-1
plan=plan[period]
kind 0: soraAction = selectedTask>=0 ? 1 : 0
kind 1: meetingTarget=plan.target; t=code[target]:
    0→soraAction=2,code=1 ; 2→3,code=3 ; 4→4,code=5 ; 6→5,code=7 ; else soraAction=6
kind 2: soraAction=7
kind 3: breakTarget=target; if code==10: soraAction=9 else soraAction=8,code=9
other: soraAction=0
```
soraAction names from the script: 0 NONE, 1 SPEC, 2 MEETING, 3 WORK_MEETING, 4 DOUBLE_MEETING, 5 SLUMP_MEETING, 6 NO_MEETING, 7 APARTMENT, 8 BREAK, 9 NO_BREAK.
A char with code 8 who is target of a meeting gets soraAction 6 with no change [CONFIRMED]. Character kind values outside 0..3 → 0.

### 5.2 Sprite and panel setup `FUN_00028fc8`

Sorata position table: `pos[6]`= (60,58),(-34,58),(154,58),(154,-27),(60,-27),(-112,-27), sorata home (-34,-27), and away variants anim `4..7` at (-168,-27)x3, (-128,72). Hero slot positions are `pos[c]`. Second table in draw (60,90),(-34,90),(154,90),(154,5),(60,5),(-112,5).

| soraAction | sorata pos | anim | panel row 0 |
|---|---|---|---|
| 0 | home | 8 | `作業なし`, icon 0 |
| 1 | home | 0 | `%s 仕様作成中 %s.%s/%s.%s日` icon 7, sub rand%3 |
| 2,3,5 | pos[target] | 2 | none |
| 4 | pos[target] | 3 | none |
| 6 | home | 8 | `ミーティングなし` icon 0 |
| 7 | random r=rand&3 pos | 4+r | `さくら荘の仕事中` icon 8 sub r |
| 8 | pos[breakTarget] | 8 | none |
| 9 | home | 8 | `お休みなし` icon 0 |

Hero rows, c-th panel row (id c+1), anim and text:

| code | anim | text | icon |
|---|---|---|---|
| 0 | 0x10 | 作業なし | 0 |
| 1 | 0xc | ミーティング中 | 2 (flag 0) |
| 2 | task.workAnim | `%s 作業中 %s.%s/%s.%s日` | 6 |
| 3 | 0xc | `%s ミーティング中 ...` | 2 |
| 4 | workAnim+6 | `%s 倍速作業中 ...` | 6 |
| 5 | 0xd | `%s 倍速ミーティング中 ...` | 2 |
| 6 | 0x11 | 悩み中 | 3 |
| 7 | 0xc | ミーティング中 | 2 |
| 8 | 0xf | 作業不可 | 4 |
| 9 | 0xe | お休み | 1 |
| 10 | 0x12 | 不在 | 5 |

Number format: `n` units → `n/3 . (n%3)*3` shown via strings `"0".."10"`, 3 units = 1.0 day. Shown value uses `done+1` (spec, work) or `done+2` (double), name is `shortName` with newlines removed.

Panel `+0x11ebec`: ring of up to 7 rows, shown 3 at a time, row 32 px, origin (-208,56), scrolled by -(n-1)*32. Each row: up to 2 icons at x 0,32 then text (`FUN_0002b004`: font cell 14, 7 px base x, 18 px line). Timing: 90 frames per row, then scroll for 30 frames (`dy=-32*t/30`). Each row also triggers a voice when shown (`FUN_0002ab98`):
- kind 6: table 0x228f84 `[char*6 + workAnim]`
- kind 7: 0x22902c `[char*3 + sub]`
- kind 8: 0x229080 `[char*4 + sub]`
- else 0x228e88 `[char*9 + kind]`
where char index is row id (0 sorata, 1..6 hero). Tables hold `V_G04_*.hca` file names.

### 5.3 Script stage (script_system_action.ssb) [CONFIRMED]

```
period=N11
action=N83
if action==5: slumpConversation(N84): N81(c); wait; N34(c)  # clear state to 0
if action==7:
   t=turn
   (t,period)->event: see table; run file via N79+wait; N56(id)
if action==7: for c in 0..5: mood+=10, health+=20, affinity+=5, love+=10
for c in 0..5:
   switch charAction[c]:
    0: mood-=3
    1,3,5: mood+=60, health+=20, affinity+=10, love+=30, and if c==0 love[2]-=5, if c==2 love[0]-=5
    2: mood-=3, health-=4, affinity+=1
    4: mood-=5, health-=6, affinity+=1
    6: nothing
    7: mood+=60
    8: health+=5
    9: mood+=20, health+=70, affinity+=20, love+=20, plus the same love rival rule
    10: nothing
```
Apartment events:

| turn | period | file | N56 id |
|---|---|---|---|
| 7 | 1 | SCN_BMSR_1Y_1112x_010 | 0 |
| 17 | 0 | SCN_BNNM_1Y_1112x_010 | 1 |
| 21 | 2 | SCN_BMYH_1Y_1112x_010 | 2 |
| 32 | 0 | SCN_BMSK_1Y_1112x_010 | 3 |
| 40 | 1 | SCN_BMSR_1Y_1112x_020 | 4 |
| 47 | 0 | SCN_BOTH_1Y_1112x_010 | 5 |
| 56 | 0 | SCN_BNNM_1Y_1112x_020 | 6 |
| 60 | 1 | SCN_BMSK_1Y_1112x_020 | 7 |
| 63 | 0 | SCN_BCHI_1Y_1112x_010 | 8 |
| 66 | 0 | SCN_BRTA_1Y_1112x_010 | 9 |
| 73 | 2 | SCN_BNNM_1Y_1112x_030 | 10 |
| 81 | 1 | SCN_BMSR_1Y_1112x_030 | 11 |
| 83 | 0 | SCN_BOTH_1Y_1112x_020 | 12 |
| 88 | 1 | SCN_BRTA_1Y_1112x_020 | 13 |

### 5.4 Task resolution `FUN_0002654c` [CONFIRMED]

```
before = bitset(prereqOK)
if soraAction==1:
   t=selectedTask; t.planDone++; update(t)
   if planComplete(t): revealChildren(t)
   for all: update; for all: checkPrereq
   if planComplete(t): log(type0, t)
after=bitset(prereqOK); diff(before,after)   # newly prereqOK tasks: log(type2,task)
for c in 0..5:
   before=bitset
   switch charAction[c]: 2,3: workUnit(c)  4,5: workUnit(c) twice
   after=bitset; diff
if soraAction==1 && planComplete(selectedTask):
   queue[t.owner].task[count++] = t
```
`workUnit(c)` `FUN_00026324`: if doneIndex<count: t=queue.task[doneIndex]; t.workDone++; update(t); for all update; for all checkPrereq; if t.state>4: doneIndex++, total[i]+=gains[i], log(type1,t). Log ring capacity 0x40.

Toasts in A7, one at a time; `%s\n仕様が完成しました` (type0), `%s\n作業が完了しました` (type1, icon task.owner+2, flag bits i where gain[i]>3, 0x10?), `%s\n他のタスクによる[LOCK]が解除されました` (type2). `name` is the task name with newlines removed; the last arg is `key!=0`. Toast `FUN_0002bdb4(text, icon, statBits, 3, key)` draws for fixed time.

## 6. Slump conversation `FUN_00026bb4` [CONFIRMED]

N81(c), with `dayOff` from calendar row of current turn, `p=period`:
```
A=[],B=[]
for each record r (index i):
   if r.char==c and r.minTurn<=turn<=r.maxTurn:
      if r.workdayOnly!=0 and dayOff!=0: continue
      if (r.periodMask>>p)&1:
          if seen[i]<1: A.append(i)
          if r.repeatable: B.append(i)
pool = A if len(A)>0 else B
total=sum(weight); k=rand()%total
walk: k-=weight, stop at first k<0
seen[pick]++
start scenario r.file via launcher (job 0xb)
```
Note: division by zero if both pools are empty. Use guard [ASSUMED intent].

## 7. Contest (job 0x0e, FUN_0001c314, FUN_00079194) [CONFIRMED]

### 7.1 Score
`score` (shared by `FUN_0001c314`, comment logic and script F277e): sort `total[0..5]` descending.
`score = s0 + (int)(s1*0.3) + Σ_{i=2..5}(int)(s[i]*0.1)` (doubles truncated; constants at 0x1ea120). Script F277e uses integers: `l1 + l2*3/10 + Σ(rest/10)`, ties handled by order but equal in value; rounding differs by sum only slightly: keep both forms where each is used [CONFIRMED].

### 7.2 Ranking `FUN_0001c314(stage)`
RNG: separate LCG seeded from real-time clock low dword (`FUN_000e8830` sceRtc), so results vary.
```
for t: teamRT[t].prevRank = teamRT[t].rank
myRank,rivalRank: first i in 0..50 with threshold[i]<=score → value of table(stage,kind)[i]
if none matched (myRank==0): use [50]
B[0..59]=0; I[t]=t
for t in 0..n-1:
   mx=team[t].max[stage], mn=team[t].min[stage]
   if mx!=0 and t!=0:
       r=mx-mn; B[t]=mn+(r? rng%r : 0)
sort (B,I) desc, stable-by-swap selection sort
n=51
# put rival team (id 1) at rank rivalRank
for i in 0..n-1:
  if rivalRank in (1,2):
     if i==0: D[0]=1; C[0]=B[0]+10
     else: D[i]=I[i-1]; C[i]=B[i-1]; if I[i-1]==1: D[i]=0
  else:
     g=rivalRank-1
     if i==g: D[i]=1; C[i]=B[i] (+10 if i==0)
     elif i<g: D[i]=I[i]; C[i]=B[i]
     else: D[i]=I[i-1]; C[i]=B[i-1]; if I[i-1]==1: D[i]=0
cap = stage<4?50 : stage<9?40 : 30
g=myRank-1
for i in 0..n-1:
  if i==g:
     I[i]=0
     if i==0: B[i]= (score<C[i])? C[i]+10 : score
     elif C[i-1]<score or score<C[i]: r=C[i-1]-C[i]; B[i]=C[i]+(r? rng%r:0)
     else B[i]=score
  elif i<g: I[i]=D[i]; B[i]=C[i]
  else:
     if i>cap: I[i]=i; B[i]=0
     else I[i]=D[i-1]; B[i]=C[i-1]
for i: teamRT[I[i]].rank=i+1; teamRT[I[i]].score=B[i]*B[i]/2
```
`rng%r` here is the RNG on the local generator; an extra rng draw in the `C[i-1]!=x` path is discarded but must be consumed [CONFIRMED]. Note i>g branch tests `cap<i`.

### 7.3 Comments `FUN_00079fd8`
```
bucket = stage<=3?1 : stage<9?2 : stage<11?3 : 4
tot=sorted desc with origin index, tags=idx+1
best=tags[0], second=tags[1], worst=tags[5], secondWorst=tags[4]
bad = score < team[0].max[stage]
for each comment idx:
   if route!=0 and c.route==route and c.sentiment==bad: PR.add
   elif c.route==0 and c.bucket in (0,bucket):
       tag=c.tag
       if tag==best and sentiment==0: P1
       elif tag==worst and sentiment==1: P2
       elif (!bad and tag==second and sentiment==0) or (bad and tag==secondWorst and sentiment==1): P3
       elif tag==0: sentiment==0? P4 : P5
```
Fill slots (commenter unique across slots, retry until unique):
1. PR: one random if non-empty.
2. P1: 2 (1 when bad). 3. P2: 2 (1 when good). 4. P3: 1.
5. P4: 4; if bad 2, and if bad and PR non-empty 1.
6. P5: 4; if good 2, and if good and PR non-empty 1.
Re-seed RNG, shuffle into 10 display slots by random unique placement. Displayed view shows up to 10 comment rows, count `*(+0x792c8)=10`.

### 7.4 Scene flow
Job update `FUN_0007def4`: fade in; state0 comment scroll `FUN_00079c1c` until done; state1 rank list object +0x906c0; fade out 10; stop; pop. Job 0xe ME on enter: if stage==11 and route in 5,6 → SK_ME_03; route 1..4 → SK_ME_02; else SK_ME_01 [CONFIRMED FUN_0007dc38]. Screenshot image: `stage<4 img_plan_01`, `stage<9 img_alpha_0x` by route `(1→01,3→03,5→02)`, else `img_beta_{msr,rns,nnm,oth,msk,zin}` indexed by route [CONFIRMED FUN_0007fd8c], `%s.txg`.

Comment sub-states `FUN_00079c1c`: 0 wait intro, 1 outro, 2 wait 10 frames then next comment; after each `+0x792d0`++ when sentiment 0 ("likes" counter, seed: team0.score minus prior field); 3 wait item, 4 wait 10, 5 end check, 6 done.

Ranking: rows at x spacing, row height 50: `listY = 60 - (rank-1)*50`, x for slide `= (n-rank)*50-110` etc (`FUN_0007fee0`). Rank numeral from sprites, compare with prevRank:
prevRank==0 → sprite 0x1790 NEW; prev>rank up 0x1804; prev<rank down 0x1878; same 0x18ec [CONFIRMED]. Title line uses genre `&PTR_DAT_00229e48[k]`; `k` from phase/route: `phase 3,4: route1→2, route5→3, route3→4, others 1?`, `5,6: 1→5,2→6,3→7,5→8,6→9,4→10`, `>=7: 1→11,2→12,3→13,5→14,6→15,4→16`, else 0. Genre strings:
0 RPG, 1 超スーパーRPG, 2 コマンド式王道RPG, 3 ハリウッド級RPG, 4 ボイスARPG, 5 コマンド式王道RPG, 6 オープンワールドFPS, 7 ボイスARPG, 8 ハリウッド級RPG, 9 ドラマティックRPG, 10 かんたんリズムRPG, 11 コマンド式王道RPG, 12 オープンワールドFPS, 13 完全フルボイスARPG, 14 ハリウッド級RPG, 15 ドラマティックRPG, 16 かんたんリズムRPG.
Team header `teamName`, `gameTitle=` player: `GameData.gameTitle`; DL digits count `floor(log10(score))+1`. Exact tweening of rows is [ASSUMED]-level; data offsets in `FUN_0007fee0`, `FUN_00080aa4`.

## 8. Story progression (script_system_progress.ssb) [CONFIRMED]

Debug natives N5/N6 are stubbed, so everything guarded by `N5(0)` range checks, F2e12, resume-from-debug and the F2ea7/F2f3d phase fixers is dead. The debug test `l0 = F3aca(0)` is 0. Simplified main loop:

```
init once: routeA=0 routeB=0; var[0..5]=0,100,200,300,400,500; var6=6; var7=-1; var8=9999; flag0=1
loop:
   flag1=1
   if weeklyBoost(): ...
   t=turn
   run turnBlock(t)
   N74 for six characters (below)
   N73
```
`var` indexes are `getVar`/`setVar` (N46/N45). Bio: `N74(0,-15,3,55,13) N74(1,-15,5,60,9) N74(2,10,7,-65,13) N74(3,20,3,-50,11) N74(4,-20,7,50,17) N74(5,25,5,-45,7)`.

Helper mapping for the table below:
`CAL`=F67d (N64 calendar). `CHOICE(m)`=F6aa: if holiday(turn) eventPick(0); then N65(m). `ACT`=F726: N66; then if holiday(turn) eventPick(1). `RANK`=F77c (8.3). `GAMEOVER`=F922 (N70, N72). `route?`=F1e56. Scene names omit the `SCN_` prefix and `.ssb`, `rN:` = run if `l2==N`.

holiday(t) (F1d5d): t in {8,9,14,15,16,22,23,29,30,36,37,43..58,64,65,66,71,72,78,79,85,86,92,93,94}.

### 8.1 Turn table
| turn | date | sequence |
|---|---|---|
| 3 | 11/12 | avail(5, 0) ; MAIN_1Y_1112__010 ; MAIN_1Y_1112__020 ; MAIN_1Y_1112__030 ; CHOICE(2) ; TUTORIAL_01 ; phase=(1) ; ACT ; MAIN_1Y_1112__040 |
| 4 | 11/13 | CAL ; MAIN_1Y_1113__010 ; CHOICE(2) ; ACT ; MAIN_1Y_1113__020 ; MAIN_1Y_1113__030 |
| 5 | 11/14 | CAL ; MAIN_1Y_1114__010 ; MAIN_1Y_1114__020 ; avail(5, 1) ; phase=(2) ; CHOICE(0) ; ACT |
| 6 | 11/15 | CAL ; LIFE_1Y_1115__010 ; TUTORIAL_02 ; CHOICE(0) ; ACT |
| 7 | 11/16 | CAL ; LIFE_1Y_1116__010 ; TUTORIAL_03 ; CHOICE(0) ; ACT |
| 8 | 11/17 | CAL ; LIFE_1Y_1117__010 ; TUTORIAL_04 ; CHOICE(1) ; ACT |
| 9 | 11/18 | CAL ; LIFE_1Y_1118__010 ; CHOICE(1) ; ACT |
| 10 | 11/19 | CAL ; LIFE_1Y_1119__010 ; route? ; r1:MAIN_1Y_1119__010 ; r3:MAIN_1Y_1119__020 ; r5:MAIN_1Y_1119__030 ; CHOICE(0) ; ACT ; TUTORIAL_05 |
| 11 | 11/20 | CAL ; LIFE_1Y_1120__010 ; CHOICE(0) ; ACT ; YMSR_1Y_1112x_010 |
| 12 | 11/21 | CAL ; LIFE_1Y_1121__010 ; MAIN_1Y_1121__010 ; CHOICE(0) ; ACT ; MAIN_1Y_1121__020 |
| 13 | 11/22 | CAL ; LIFE_1Y_1122__010 ; CHOICE(0) ; ACT ; MAIN_1Y_1122__010 ; N59() ; RANK ; F2385()? ; MAIN_1Y_1122__020 ; GAMEOVER ; MAIN_1Y_1122__030 ; route? ; r1:MAIN_1Y_1122__040 ; r3:MAIN_1Y_1122__050 ; r5:MAIN_1Y_1122__060 |
| 14 | 11/23 | phase=(3) ; CAL ; MAIN_1Y_1123__010 ; CHOICE(1) ; ACT |
| 15 | 11/24 | CAL ; LIFE_1Y_1124__010 ; LIFE_1Y_1124__020 ; CHOICE(1) ; ACT |
| 16 | 11/25 | CAL ; LIFE_1Y_1125__010 ; CHOICE(1) ; ACT ; YMSR_1Y_1112x_020 |
| 17 | 11/26 | CAL ; MAIN_1Y_1126__010 ; route? ; r1:MAIN_1Y_1126__020 ; r3:MAIN_1Y_1126__030 ; r5:MAIN_1Y_1126__040 ; TUTORIAL_07 ; CHOICE(0) ; ACT ; MAIN_1Y_1126__050 |
| 18 | 11/27 | CAL ; LIFE_1Y_1127__010 ; CHOICE(0) ; ACT ; YMSR_1Y_1112x_030 |
| 19 | 11/28 | CAL ; LIFE_1Y_1128__010 ; CHOICE(0) ; ACT |
| 20 | 11/29 | CAL ; LIFE_1Y_1129__010 ; CHOICE(0) ; ACT |
| 21 | 11/30 | CAL ; LIFE_1Y_1130__010 ; CHOICE(0) ; ACT ; YMSR_1Y_1112x_040 |
| 22 | 12/1 | CAL ; LIFE_1Y_1201__010 ; CHOICE(1) ; ACT |
| 23 | 12/2 | CAL ; LIFE_1Y_1202__010 ; CHOICE(1) ; ACT ; YMSR_1Y_1112x_050 ; RANK |
| 24 | 12/3 | CAL ; LIFE_1Y_1203__010 ; LIFE_1Y_1203__020 ; CHOICE(0) ; ACT |
| 25 | 12/4 | CAL ; LIFE_1Y_1204__010 ; CHOICE(0) ; ACT ; YMSR_1Y_1112x_060 |
| 26 | 12/5 | CAL ; LIFE_1Y_1205__010 ; CHOICE(0) ; ACT |
| 27 | 12/6 | CAL ; LIFE_1Y_1206__010 ; CHOICE(0) ; ACT ; MAIN_1Y_1206__010 |
| 28 | 12/7 | CAL ; LIFE_1Y_1207__010 ; CHOICE(0) ; ACT ; MAIN_1Y_1207__010 ; route? ; r1:MAIN_1Y_1207__020 ; r3:MAIN_1Y_1207__030 ; r5:MAIN_1Y_1207__040 ; setRouteB ; phase=(4) ; MAIN_1Y_1207__050 |
| 29 | 12/8 | CAL ; LIFE_1Y_1208__010 ; TUTORIAL_06 ; CHOICE(1) ; ACT |
| 30 | 12/9 | CAL ; LIFE_1Y_1209__010 ; CHOICE(1) ; ACT ; YMSR_1Y_1112x_070 ; RANK |
| 31 | 12/10 | CAL ; LIFE_1Y_1210__010 ; MAIN_1Y_1210__010 ; CHOICE(0) ; ACT ; MAIN_1Y_1210__020 |
| 32 | 12/11 | CAL ; LIFE_1Y_1211__010 ; CHOICE(0) ; ACT |
| 33 | 12/12 | CAL ; LIFE_1Y_1212__010 ; CHOICE(0) ; ACT ; YMSR_1Y_1112x_080 |
| 34 | 12/13 | CAL ; LIFE_1Y_1213__010 ; CHOICE(0) ; ACT ; MAIN_1Y_1213__010 |
| 35 | 12/14 | CAL ; LIFE_1Y_1214__010 ; CHOICE(0) ; ACT ; YMSR_1Y_1112x_090 |
| 36 | 12/15 | CAL ; LIFE_1Y_1215__010 ; CHOICE(1) ; ACT |
| 37 | 12/16 | CAL ; LIFE_1Y_1216__010 ; CHOICE(1) ; ACT ; YMSR_1Y_1112x_100 ; RANK |
| 38 | 12/17 | CAL ; LIFE_1Y_1217__010 ; CHOICE(0) ; ACT |
| 39 | 12/18 | CAL ; LIFE_1Y_1218__010 ; CHOICE(0) ; ACT ; MAIN_1Y_1218__010 |
| 40 | 12/19 | CAL ; LIFE_1Y_1219__010 ; CHOICE(0) ; ACT |
| 41 | 12/20 | CAL ; LIFE_1Y_1220__010 ; CHOICE(0) ; ACT ; YMSR_1Y_1112x_110 |
| 42 | 12/21 | CAL ; LIFE_1Y_1221__010 ; CHOICE(0) ; ACT ; F23e0()? ; MAIN_1Y_1221__010 ; N60() ; MAIN_1Y_1221__020 ; GAMEOVER ; RANK ; F24d9()? ; MAIN_1Y_1221__030 ; GAMEOVER ; MAIN_1Y_1221__040 |
| 43 | 12/22 | phase=(5) ; CAL ; LIFE_1Y_1222__010 ; CHOICE(1) ; ACT |
| 44 | 12/23 | CAL ; LIFE_1Y_1223__010 ; CHOICE(1) ; ACT |
| 45 | 12/24 | CAL ; MAIN_1Y_1224__010 ; MAIN_1Y_1224__020 ; CHOICE(1) ; ACT |
| 46 | 12/25 | CAL ; LIFE_1Y_1225__010 ; CHOICE(1) ; ACT ; YMSR_1Y_1112x_120 |
| 47 | 12/26 | CAL ; LIFE_1Y_1226__010 ; CHOICE(1) ; ACT ; MAIN_1Y_1226__010 |
| 48 | 12/27 | CAL ; MAIN_1Y_1227__010 ; CHOICE(1) ; ACT ; YMSR_1Y_1112x_130 |
| 49 | 12/28 | CAL ; LIFE_1Y_1228__010 ; CHOICE(1) ; ACT ; YMSR_1Y_1112x_140 |
| 50 | 12/29 | CAL ; LIFE_1Y_1229__010 ; CHOICE(1) ; ACT ; YMSR_1Y_1112x_150 |
| 51 | 12/30 | CAL ; LIFE_1Y_1230__010 ; CHOICE(1) ; ACT ; RANK |
| 52 | 12/31 | CAL ; LIFE_1Y_1231__010 ; CHOICE(1) ; ACT ; MAIN_1Y_1231__010 |
| 53 | 1/1 | CAL ; MAIN_2Y_0101__010 ; CHOICE(1) ; ACT |
| 54 | 1/2 | CAL ; LIFE_2Y_0102__010 ; CHOICE(1) ; ACT |
| 55 | 1/3 | CAL ; LIFE_2Y_0109__010 ; MAIN_2Y_0109__020 ; CHOICE(1) ; ACT ; YMSR_1Y_1112x_160 |
| 56 | 1/4 | CAL ; LIFE_2Y_0104__010 ; CHOICE(1) ; ACT |
| 57 | 1/5 | CAL ; LIFE_2Y_0105__010 ; CHOICE(1) ; ACT ; YMSR_1Y_1112x_170 |
| 58 | 1/6 | CAL ; LIFE_2Y_0106__010 ; CHOICE(1) ; ACT ; YMSR_1Y_1112x_180 ; RANK |
| 59 | 1/7 | CAL ; LIFE_2Y_0107__010 ; MAIN_2Y_0107__010 ; route? ; r1:MAIN_2Y_0107__020 ; r2:MAIN_2Y_0107__030 ; r3:MAIN_2Y_0107__040 ; r4:MAIN_2Y_0107__050 ; r5:MAIN_2Y_0107__060 ; r6:MAIN_2Y_0107__070 ; setRouteB ; phase=(6) ; CHOICE(0) ; ACT |
| 60 | 1/8 | CAL ; LIFE_2Y_0108__010 ; CHOICE(0) ; ACT ; YMSR_1Y_1112x_190 |
| 61 | 1/9 | CAL ; LIFE_2Y_0103__010 ; (Fc5(3) >= 303)? ; MAIN_2Y_0109__010 ; CHOICE(0) ; ACT ; MAIN_2Y_0109__030 |
| 62 | 1/10 | CAL ; LIFE_2Y_0110__010 ; CHOICE(0) ; ACT ; YMSR_1Y_1112x_200 |
| 63 | 1/11 | CAL ; LIFE_2Y_0111__010 ; MAIN_2Y_0111__010 ; CHOICE(0) ; ACT |
| 64 | 1/12 | CAL ; LIFE_2Y_0112__010 ; CHOICE(1) ; ACT ; YMSR_1Y_1112x_210 |
| 65 | 1/13 | CAL ; LIFE_2Y_0113__010 ; CHOICE(1) ; ACT ; YMSR_1Y_1112x_220 ; RANK |
| 66 | 1/14 | CAL ; LIFE_2Y_0114__010 ; CHOICE(1) ; ACT ; YMSR_1Y_1112x_230 |
| 67 | 1/15 | CAL ; LIFE_2Y_0115__010 ; MAIN_2Y_0115__010 ; avail(5, 0) ; CHOICE(0) ; ACT ; YMSR_1Y_1112x_240 |
| 68 | 1/16 | CAL ; MAIN_2Y_0116__010 ; MAIN_2Y_0116__020 ; CHOICE(0) ; ACT ; MAIN_2Y_0116__030 |
| 69 | 1/17 | CAL ; MAIN_2Y_0117__010 ; MAIN_2Y_0117__020 ; CHOICE(0) ; ACT |
| 70 | 1/18 | CAL ; LIFE_2Y_0118__010 ; MAIN_2Y_0118__010 ; avail(5, 1) ; CHOICE(0) ; ACT |
| 71 | 1/19 | CAL ; LIFE_2Y_0119__010 ; CHOICE(1) ; ACT |
| 72 | 1/20 | CAL ; LIFE_2Y_0120__010 ; CHOICE(1) ; ACT ; RANK |
| 73 | 1/21 | CAL ; LIFE_2Y_0121__010 ; MAIN_2Y_0107x_010 ; CHOICE(0) ; ACT ; YMSR_1Y_1112x_250 |
| 74 | 1/22 | CAL ; LIFE_2Y_0122__010 ; CHOICE(0) ; ACT ; YMSR_1Y_1112x_260 |
| 75 | 1/23 | CAL ; rB ; r1:MAIN_1Y_1224x_010 ; r3:MAIN_1Y_1224x_020 ; r5:MAIN_1Y_1224x_030 ; r4:MAIN_1Y_1224x_040 ; LIFE_2Y_0123__010 ; CHOICE(0) ; ACT ; YMSR_1Y_1112x_270 |
| 76 | 1/24 | CAL ; LIFE_2Y_0124__010 ; CHOICE(0) ; ACT ; YMSR_1Y_1112x_280 |
| 77 | 1/25 | CAL ; LIFE_2Y_0125__010 ; CHOICE(0) ; ACT ; F2521()? ; MAIN_2Y_0125__010 ; N61() ; MAIN_2Y_0125__020 ; GAMEOVER ; F26bf()? ; MAIN_2Y_0125__030 ; RANK ; GAMEOVER ; MAIN_2Y_0125__040 ; RANK |
| 78 | 1/26 | phase=(7) ; CAL ; LIFE_2Y_0126__010 ; CHOICE(1) ; ACT |
| 79 | 1/27 | CAL ; LIFE_2Y_0127__010 ; CHOICE(1) ; ACT |
| 80 | 1/28 | CAL ; LIFE_2Y_0128__010 ; MAIN_2Y_0128__010 ; CHOICE(0) ; ACT |
| 81 | 1/29 | CAL ; LIFE_2Y_0129__010 ; (statF(2) >= 350)? ; LIFE_2Y_0115__020 ; N58(21) ; F4(2) ; CHOICE(0) ; ACT ; YMSR_1Y_1112x_290 |
| 82 | 1/30 | CAL ; LIFE_2Y_0130__010 ; CHOICE(0) ; ACT ; YMSR_1Y_1112x_300 |
| 83 | 1/31 | CAL ; LIFE_2Y_0131__010 ; CHOICE(0) ; ACT ; YMSR_1Y_1112x_310 |
| 84 | 2/1 | CAL ; LIFE_2Y_0201__010 ; MAIN_2Y_0201__010 ; rB ; r3:MAIN_2Y_0201__020 ; CHOICE(0) ; ACT |
| 85 | 2/2 | CAL ; LIFE_2Y_0202__010 ; CHOICE(1) ; ACT |
| 86 | 2/3 | CAL ; LIFE_2Y_0203__010 ; CHOICE(1) ; ACT ; RANK |
| 87 | 2/4 | CAL ; LIFE_2Y_0204__010 ; MAIN_2Y_0204__010 ; CHOICE(0) ; ACT ; (Fc5(0) == 8)? ; CMSR_2Y_0128x_020 ; N58(8) ; F1bff(0, 9) |
| 88 | 2/5 | CAL ; LIFE_2Y_0205__010 ; CHOICE(0) ; ACT ; YMSR_1Y_1112x_320 |
| 89 | 2/6 | CAL ; LIFE_2Y_0206__010 ; CHOICE(0) ; ACT ; MAIN_2Y_0206__010 |
| 90 | 2/7 | CAL ; LIFE_2Y_0207__010 ; CHOICE(0) ; ACT |
| 91 | 2/8 | CAL ; CHOICE(0) ; ACT ; MAIN_2Y_0208__010 ; N62() |
| 92 | 2/9 | phase=(8) |
| 93 | 2/10 |  |
| 94 | 2/11 |  |
| 95 | 2/12 |  |
| 96 | 2/13 | CAL ; F2707()? ; MAIN_2Y_0213__010 ; RANK ; RANK ; MAIN_2Y_0213__020 ; GAMEOVER |
| 97 | 2/14 | see the turn 97 listing below |
| 98 | 2/15 |  |

Turn 97 (2/14, finale, cannot be tabulated) [CONFIRMED]:
```
CAL; MAIN_2Y_0214__010; eventPick(2); N69 (ending movie, F874)
if love[0]>=400 or love[2]>=400:
   MAIN_2Y_0214__020
   hero=var[6]; a=b=c=d=0
   if hero==0: (love[0]<480) ? b=1 : a=1
   if hero==2: if flag[2]==0: d=1 elif love[2]<480: d=1 else c=1
   if b: MAIN_2Y_0214__030; N53(0); N58(13)
   if d: MAIN_2Y_0214__050; N53(2); N58(27); N58(28)
   if hero==-1: MAIN_2Y_0214__060
   if a:
     MAIN_2Y_0214__070
     if F2976: MAIN_2Y_0214__080; N49(0); N54(0); N58(11)
     else:     MAIN_2Y_0214__090; N55(0); N58(12)
   if c:
     MAIN_2Y_0214__100
     if F29e8: MAIN_2Y_0299x_010; N49(2); N54(2); N58(25)
     else:     MAIN_2Y_0299x_020; N55(2); N58(26)
eventPick(3)
r=routeB
if (r==1 and var0>=10) or (r==3 and var2>=209) or (r==5 and var1>=109) or (r==4 and var5>=509) or (r==2 and var4>=407) or (r==6 and var3>=307): N47(r)
F930: N70 finish, N72 pop
```
`F4e(n)` is `flag[n]==0` so `flag[2]` was set on turn 81 when `love[2]>=350`. Note that a `getFlag` of 0 for hero 2 forces the `d` branch.
F2976: `badClear0 = !N50(0); return badClear0 || teamRT[0].score<600`. F29e8 is the same with N50(2). Both: `return (no prior flag) or score<600`. So `__080` runs when the bad-end bit is not yet set or score<600; `N49` sets the bit, and because badBits survives carry-over (section 12), a later run with score>=600 reaches `__090` [CONFIRMED].

Turns 92..95: only `phase=8` on 92; no scenes, no choice (they still run N74, N73).

### 8.2 Phase and availability

| turn | action |
|---|---|
| 3 | available[5]=0 before scenes, after TUTORIAL_01 phase=1 |
| 5 | available[5]=1, phase=2 before choice |
| 14 | phase=3 |
| 28 | phase=4 and route set |
| 43 | phase=5 |
| 59 | route refined, phase=6 |
| 67 | available[5]=0 |
| 70 | available[5]=1 |
| 78 | phase=7 |
| 92 | phase=8 |

### 8.3 RANK (F77c)
```
t=turn
if t not in {13,42,77,96}: run RANK_1_BEFORE
s=var[7]
if s>=0: var[8]=teamRT[0].rank
s++; setRankStage(s); var[7]=s
N68   # contest job
if t not in {13,42,77,96}: run RANK_2_AFTER
```
Stages are consumed in call order: turn 13 s0, 23 s1, 30 s2, 37 s3, 42 s4, 51 s5, 58 s6, 65 s7, 72 s8, 77 s9, 86 s10, 96 s11. All normal playthroughs call it exactly once on those turns (turn 96 both branches once). Turn 77 calls once in either path, so 12 stages in total.

### 8.4 Pass and fail conditions

| turn | test | fail → |
|---|---|---|
| 13 | F2385 rank>50 | MAIN_1Y_1122__020; GAMEOVER |
| 42 | F23e0 keysA | MAIN_1Y_1221__020; GAMEOVER |
| 42 | after RANK F24d9 rank>40 | MAIN_1Y_1221__030; GAMEOVER; else __040 |
| 77 | F2521 keysB | MAIN_2Y_0125__020; GAMEOVER |
| 77 | F26bf score<190 | MAIN_2Y_0125__030; RANK; GAMEOVER; else __040; RANK |
| 96 | F2707 score>=520 | else RANK; MAIN_2Y_0213__020; GAMEOVER |

Note F2385 is `rank<=50 → return 0`, and F24d9 returns 1 when `rank>40`, F26bf uses `F277e()` integer score.

keysA (all six taskDone): route1 {57,65,72,81,87,95}; route3 {188,198,207,216,223,233}; route5 {329,339,347,358,367,378}.
keysB: route1 {104,110,116,123,129,135}; route2 {142,151,159,167,478,181}; route3 {243,251,259,266,274,280}; route4 {479,295,302,309,315,322}; route5 {386,394,401,409,416,424}; route6 {432,439,446,455,463,470}. Other route values: fails.

### 8.5 Route selection (F1e56) — `l2`
```
t=turn; r=routeB
if t<=28:
   best=-1
   a=tot0+tot1  if a>best: best=a,r=1
   b=tot5+tot2  if b>best: best=b,r=3
   c=tot3+tot4  if c>best: best=c,r=5
   if t==28: r=F1ff0(r)
elif t<=59:
   if r==1: if tot0<tot1: r=2
   elif r==3: if tot5<tot2: r=4
   elif r==5: if tot3<tot4: r=6
   else return -1
   if t==59: r=F2245(r)
return r
```
setRouteB only on turns 28 and 59. Turn 10, 13, 17 read it for dialog only. Tie order 1,3,5. On re-evaluation r 2,4,6 cannot appear before 59 since set is once at 28.

F1ff0(r), "is the heroine pair still viable":
```
m=var0!=0; n=var2!=200; k=var1!=100; o=var5!=500; j=var3!=300; ry=1 (bug: `if var4==400: ry=1`)
g1=m&&ry g3=n&&o g5=k&&j
if (g1&&r==1)||(g3&&r==3)||(g5&&r==5): return r
if r==1: if b>=c: (g3?3 : g5?5) else (g5?5 : g3?3)
if r==3: if a>=c: (g1?1 : g5?5) else (g5?5 : g1?1)
if r==5: if a>=b: (g1?1 : g3?3) else (g3?3 : g1?1)
# no pair: highest individual total among eligible
bestv=-1, res=-1
m: tot0→1; ry: tot1→1; n: tot5→3; o: tot2→3; k: tot3→5; j: tot4→5 (strict >, in that order)
if res!=-1 return res; return r
```
F2245(r): eligibility: `m=var0>=4, n=var2>=204, k=var1>=104, o=var5>=504, j=var3>=304, ry=var4>=404`. If r's own char eligible, return r. Else swap in pair (1↔2, 3↔4, 5↔6) if the partner is eligible. Else return r.

### 8.6 Event pick (F992(mode)) and weekly boost

Eligibility: `ok[c] = state[c] not in {2,3}` (and Otoha also `available[5]==1`). Rules for mode 0/1:
best=-1. Windows run in order. Inside a window, characters are tested in order 0,2,1,5,4,3; a character is tested only when `D[c] > best`; a hit sets best=D[c] and selected. Windows marked `first` run only if nothing selected. Each row: `E[c]==E && D[c]>=D && var[c]==var`.
Table `when`: 0 before choice, 1 after action.

| when | turns | char | E== | D>= | var== | var= | file | gal |
|---|---|---|---|---|---|---|---|---|
| 0 | 8-23  | 0 | 1 | 100 | 0 | 1 | CMSR_1Y_1112x_010 | 0 |
| 0 | 8-23  | 2 | 1 | 100 | 200 | 201 | CNNM_1Y_1112x_010 | 14 |
| 0 | 8-23  | 1 | 1 | 100 | 100 | 101 | CMSK_1Y_1112x_010 | 29 |
| 0 | 8-23  | 5 | 1 | 100 | 500 | 501 | COTH_1Y_1112x_010 | 55 |
| 0 | 8-23  | 4 | 1 | 100 | 400 | 401 | CRYU_1Y_1112x_010 | 47 |
| 0 | 22-44 first | 4 | 2 | 200 | 401 | 402 | CRYU_1Y_1126x_010 | 48 |
| 0 | 22-44 first | 4 | 3 | 300 | 402 | 403 | CRYU_1Y_1126x_020 | 49 |
| 0 | 22-44 first | 3 | 2 | 200 | 301 | 302 | CJIN_1Y_1126x_010 | 40 |
| 0 | 22-51 first | 0 | 2 | 200 | 1 | 2 | CMSR_1Y_1126x_010 | 1 |
| 0 | 22-51 first | 0 | 3 | 300 | 2 | 3 | CMSR_1Y_1126x_020 | 2 |
| 0 | 22-51 first | 2 | 2 | 200 | 201 | 202 | CNNM_1Y_1126x_010 | 15 |
| 0 | 22-51 first | 2 | 3 | 300 | 202 | 203 | CNNM_1Y_1126x_020 | 16 |
| 0 | 22-51 first | 1 | 2 | 200 | 101 | 102 | CMSK_1Y_1126x_010 | 30 |
| 0 | 22-51 first | 1 | 3 | 300 | 102 | 103 | CMSK_1Y_1126x_020 | 31 |
| 0 | 22-51 first | 5 | 2 | 200 | 501 | 502 | COTH_1Y_1126x_010 | 56 |
| 0 | 22-51 first | 5 | 3 | 300 | 502 | 503 | COTH_1Y_1126x_020 | 57 |
| 0 | 55-79  | 0 | 7 | 700 | 6 | 7 | CMSR_1Y_1224x_020 | 6 |
| 0 | 45-79 first | 0 | 4 | 400 | 3 | 4 | CMSR_1Y_1126x_030 | 3 |
| 0 | 45-79 first | 2 | 4 | 400 | 203 | 204 | CNNM_1Y_1126x_030 | 17 |
| 0 | 45-79 first | 2 | 5 | 500 | 204 | 205 | CNNM_1Y_1126x_040 | 18 |
| 0 | 45-79 first | 2 | 6 | 600 | 205 | 206 | CNNM_1Y_1224x_010 | 19 |
| 0 | 45-79 first | 2 | 7 | 700 | 206 | 207 | CNNM_1Y_1224x_020 | 20 |
| 0 | 45-79 first | 1 | 4 | 400 | 103 | 104 | CMSK_1Y_1126x_030 | 32 |
| 0 | 45-79 first | 1 | 5 | 500 | 104 | 105 | CMSK_1Y_1126x_040 | 33 |
| 0 | 45-79 first | 1 | 6 | 600 | 105 | 106 | CMSK_1Y_1224x_010 | 34 |
| 0 | 45-79 first | 1 | 7 | 700 | 106 | 107 | CMSK_1Y_1224x_020 | 35 |
| 0 | 45-79 first | 5 | 4 | 400 | 503 | 504 | COTH_1Y_1126x_030 | 58 |
| 0 | 45-79 first | 5 | 5 | 500 | 504 | 505 | COTH_1Y_1126x_040 | 59 |
| 0 | 45-79 first | 5 | 6 | 600 | 505 | 506 | COTH_1Y_1224x_010 | 60 |
| 0 | 45-79 first | 5 | 7 | 700 | 506 | 507 | COTH_1Y_1224x_020 | 61 |
| 0 | 45-79 first | 4 | 4 | 400 | 403 | 404 | CRYU_1Y_1224x_010 | 50 |
| 0 | 45-79 first | 4 | 5 | 500 | 404 | 405 | CRYU_1Y_1224x_020 | 51 |
| 0 | 45-79 first | 3 | 4 | 400 | 303 | 304 | CJIN_1Y_1224x_010 | 42 |
| 0 | 45-79 first | 3 | 5 | 500 | 304 | 305 | CJIN_1Y_1224x_020 | 43 |
| 1 | 8-23  | 3 | 1 | 100 | 300 | 301 | CJIN_1Y_1112x_010 | 39 |
| 1 | 22-44  | 3 | 3 | 300 | 302 | 303 | CJIN_1Y_1126x_020 | 41 |
| 1 | 55-79  | 0 | 5 | 500 | 4 | 5 | CMSR_1Y_1126x_040 | 4 |
| 1 | 55-79  | 0 | 6 | 600 | 5 | 6 | CMSR_1Y_1224x_010 | 5 |

Mode 0, turn 78..86 pass [CONFIRMED]: two passes `p=1,2`, in order 0,2,1,5,4,3, row (E,D,var) `Mashiro (8,800,7)→8 CMSR_2Y_0128x_010 gal7`, `Nanami (8,800,207)→208 CNNM_2Y_0128x_010 gal22`, `Misaki (8,800,107)→108 CMSK_2Y_0128x_010 gal36`, `Otoha (8,800,507)→508 COTH_2Y_0128x_010 gal62`, `Ryu (6,600,405)→406 CRYU_2Y_0128x_010 gal52`, `Jin (6,600,305)→306 CJIN_1Y_1112x_020 gal44`. Pass 1 requires routeB == char's route and breaks on first hit; pass 2 has no route test and continues normal best logic. Route id of each is 1,3,5,4,2,6 in that order.

Hit: `run file; N58(gal); setEventVar(c,var)`; then `limit=(c==3||c==4)?6:8; if E[c]<limit: E[c]++`. `setEventVar` F1bff checks a range (0..12,100..111,200..211,300..309,400..409,500..511) and `new==old+1` only for a debug print, always writes when in range.

Mode 2 (`F968`) by route, runs file, sets save bits, sets var, returns:
- r1: var0==9: CMSR_2Y_0214__010; N51(1); N58(9); var0=10; then if N48(1) and D0>=850: CMSR_2Y_0214__020; N52(1); N58(10); var0=11
- r3: var2==208: CNNM_2Y_0214__010; N51(3); N58(23); var2=209; then N48(3)&&D2>=850: CNNM_2Y_0214__020; N52(3); N58(24); var2=210
- r5: var1==108: CMSK_2Y_0214__010; N51(5); N58(37); var1=109
- r4: var5==508: COTH_2Y_0214__010; N51(4); N58(63); var5=509; then N48(4)&&D5>=850: COTH_2Y_0214__020; N52(4); N58(64); var5=510
- r2: var4==406: CRYU_2Y_0214__010; N51(2); N58(53); var4=407; then N48(2)&&D4>=850: CRYU_2Y_0214__020; N52(2); N58(54); var4=408
- r6: var3==306: CJIN_2Y_0214__010; N51(6); N58(45); var3=307
The "then" test is nested inside the first event's block and runs straight after it on the same turn [CONFIRMED L19a8/L1a02]. Its D value is the one sampled at function entry. N48 tests clearBits, which only a carry-over save can have set before this point (section 12).

Mode 3 (`F97d`): r5: N48(5)&&D1>=850: CMSK_2Y_0299x_010; N52(5); N58(38); var1=110. r6: N48(6)&&D3>=850: CJIN_2Y_0299x_010; N52(6); N58(46); var3=308.

Turn 87: if var0==8: CMSR_2Y_0128x_020; N58(8); var0=9.
Turn 81: if love[2]>=350: LIFE_2Y_0115__020; N58(21); flag[2]=1.
Turn 61: if var3>=303: MAIN_2Y_0109__010.

Weekly boost F3a0, at start of every turn with turn<=23 and holiday(turn) [CONFIRMED]:
```
if turn==23 and D[3]<100: D[3]=100 ; hit1=1
need = t<=8?7 : t<=9?6 : t<=14?5 : t<=15?4 : t<=16?3 : t<=22?2 : 1
under = count of D[c]<100 for c in {0,2,1,5,4}
elig[c] = D[c]<100 and state[c] not in {2,3}
if under<need: return hit1
pick first in order 0,2,1,5,4 with elig → D=100, return 1
pick first in order with D<100 → D=100, return hit1+1
return hit1
```
Return value is unused beyond debug.

## 9. Calendar (job 0x0c) [CONFIRMED]

Size 0xbc140, init `FUN_00071ccc`. Textures: calendar_parts.txg (14 quads, table 0x1f68e8, 0x50 stride), calendar_font.txg (21, 0x1f6d48), calendar_font02.txg (16, 0x1f73d8). Enter: fade mode 3, in 30 frames. Common object `FUN_0006fee0` is a textured quad with pos/scale/colour.

Layout: day strip of 103 cells, cell i at `x = scroll + i*72`, `y=-10`; cell object layout scale (60,1,1). Cursor marks `today = max(turn-1,2)`, scroll initial = `-256 - (min(today,limit)-2)*72`. Milestones: rows `{13,42,77,91,96}` flag at (cellX, cellY+40) and label (cellX, cellY-20); `{46,53,97}` at (cellX+25, cellY-40). Arrows at (-230,-10), (230,-10), alpha pulse 0x33..0xfa step 5 per frame; shown when scroll is beyond the bounded range `first..last`.

Range table (`group` from phase): phases 0-2 g0 first=3,last=13,limit=10; 3-4 g1 14,42,0x27; 5-6 g2 43,77,0x4a; 7 g3 78,91,0x58; phase 8+ in February: day<=7 g4 92,96,0x5d; day<=12 g5 92,97,0x5e; else g other 3,97,0x5e. Group>3 with `FUN_0006f1e4` hides the countdown; otherwise countdown shows `last-(turn)` days to next deadline, digits scale 1.2, colour alpha 0x35.

State machine `FUN_000717b4(allowAdvance)`, `allowAdvance = shutter state 2 or 4`:
0 wait fade in + 11 frames; if no countdown → 2, show help `○進む　スクロール　　コンテストの提出期限などが確認できます`, else 1.
1 countdown anim `FUN_000702e0` until +0x8c04==6, then → 2 and help.
2 wait allowAdvance + 11 → 3. 3 day flip `FUN_0006fba0` until done → 4. 4 wait 11, → 5.
5 interactive: while RIGHT or LEFT held scroll, CIRCLE confirm (SE) → 6 starts `FUN_0006fa04(7.2),FUN_0006f858(7.2,0)` (7.2 px per frame, 10 frames per cell).
6 `FUN_0006fd5c` until done → 7. 7 wait 11, return 1 → job: fade mode 0, out 10, hide help, pop.
Pen tool `FUN_00070684` draws the current date stamp when sub-state<4.

## 10. Task chart (job 10) [CONFIRMED]

`FUN_0003ce28(mode)` then push job 10; mode 1 select, 0 view. Size 0x2beac0. Chart object +0x2a3340. Grid cell 104x72, draw offset `(x*-104, y*72)`. Sprite data: `square_*`.

Job states: enter: fade in, mode 1 plays SK_BGM_03 then voice `V_G04_MID_0060.hca`. If `anySelectable()` false → state NoTask (dialog `現在作成できる仕様がありません`, on close `selectedTask = -(cursorTask+1)` if cursorTask>=0, then pop), else Browse.

Browse in mode 1: SELECT → push job 0xd (fade, return Browse); cursor; on activation (`+0x15a0c`): voice `V_G04_SRT_0050.hca`, Confirm dialog `選択した仕様を作成しますか？` → yes: `selectedTask=cursorTask`, fade out, stop BGM, pop; no: Browse. Mode 0: CROSS exit (SE 0x1f).

Cursor `FUN_0003c33c`: CIRCLE on selectable cell activates. d-pad with repeat moves cell, cursor tweens: each frame `pos += (target-pos)*0.5`, snaps at distance<1. Pointer: tap cell, drag scrolls, clamp x to `[(cols-1)*-104,0]`, y to `[0,(rows-1)*72]`.

Cell info panel `FUN_00040edc`, `FUN_000416ac`: `あと %s.%s/%s.%s日` or `完成`. Line drawing `FUN_0003ecd4`: for each child link: child state 0 and prereqOK → grey 0xff808080 line; state>=1 white; else hidden.

## 11. Task list (job 0x0d)

Uses task_bg/window/count/name/psrts/cursor.txg; shows, for the character queues, remaining tasks (`FUN_00075a48`, `FUN_0007610c`, `FUN_000768d8`). Read-only view [ASSUMED]; layout not reversed. Unresolved.

## 12. Finish and game over (job 7) [CONFIRMED]

Same job for clear and failure. States: fade mode 0 in 10 → dialog (OK only) `引き継ぎセーブは、シナリオのクリア情報、仕様スクエアのオープン状況を引き継ぎます。` → yes/no `引き継ぎデータをセーブしますか？` → yes: wait 4 frames, `FUN_00001f6c`, `FUN_00001d2c`, fade out 10, push job 0x1b (save), then fade in 10 → fade out 10, pop. No: fade out 10, pop. Afterwards `F8c3` N72 pops progress job, so title returns.
Background `finish_bg.txgtl` [ASSUMED: string present, loader not traced].

Carry-over [CONFIRMED]: `FUN_00001f6c` snapshots live GameData to the save image at +0x4020. `FUN_00001d2c` then rebuilds that image from the pristine backup (GameData+0x3c78), keeping only `clearBits`, `badBits` and each `taskRT[i].planFinished`. On progress start, `FUN_00023858` runs `FUN_00010c78` (init), then `FUN_00001e18` restores the image via `FUN_000117a8` if image flag +0x10 is set. So a new game from carry-over starts at init values plus those three fields. Everything else (SaveMgr) is already persistent.

## 13. Ending movie (job 0x23) [CONFIRMED]

`FUN_000d9fa4`: open `ending.pmf`, at state 2 set volume `sys/5.0`, fade in 10; on finish or START/CIRCLE fade out 10, stop, wait stopped, pop.

## 14. Unresolved

- Calendar `FUN_000702e0` (countdown card) and `FUN_00070684` pen animations: exact timing not traced.
- Contest list tween values, `FUN_0007fee0` row layout details, `FUN_00080aa4`: only key constants given.
- Task list job 0xd layout; job 0x1a purpose (no string refs); entry to S16 in action choice.
- Character target list in the card (`FUN_00032668`, `FUN_00033854`) and how card rows map to `kind`: row→kind mapping [ASSUMED] from help text order and code in 28bc8.
- Fade colour modes 0/2/3 unconfirmed.
- `FUN_0003e6c8` stores `selectedTask=-(cursorTask+1)`; soraAction becomes 0 either way, but whether the chart uses it to restore the cursor is [ASSUMED].
- Save/suspend field offsets for mid-action resume (+0xc00..).
