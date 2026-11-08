# Captures for the Russian tutorial pictures (BG_TUT*.cbg), read by tools/tl_images.py.
# Run from the repo root on a fresh save directory (see tools/tl_tutorial_shots.sh):
#   port/build/sakurasou --headless --tl translation/build --ctl tools/tl_tutorial.ctl
# Commands: see the Ctl comment in port/src/main.cpp. Screens from a new game in Russian, with GameData pokes for
# the states the tutorial art shows (illness, worries, a later chart).
# boot, new game, first action choice (11/12, tutorial mode)
w 600
p 0x2000 60
p 0x2000 400
p 0x2000 150
p 0x2000 150
p 0x2000 150
p 0x2000 150
p 0x2000 150
p 0x2000 150
skip 1
p 0x2000 200
skipto 8
w 120
# TUT01: Specification in turns 1 and 2, turn 3 menu open on Specification
p 0x2000 15
p 0x2000 15
p 0x2000 15
p 0x2000 15
p 0x2000 60
rewind
w 14
s work/tut/s01.bmp
# confirm the plan (choose Specification, then Yes); the tutorial ADV runs, then the Spec Square opens in select mode
p 0x2000 40
p 0x2000 40
skipto 10
w 90
rewind
w 14
s work/tut/s02.bmp
# pick the task, run 11/12, skip to the 11/14 choice
p 0x2000 30
p 0x2000 60
skipto 8 20000
w 120
rewind
w 14
# 11/13: Specification x3 (no square: the first task is still being specified), skip to the 11/14 choice
w 200
p 0x2000 15
p 0x2000 15
p 0x2000 15
p 0x2000 15
p 0x2000 15
p 0x2000 40
p 0x2000 40
skipto 8 20000
# TUT04: 11/14, nothing chosen yet
w 260
rewind
w 14
s work/tut/s04.bmp
# 11/14: Specification x3; the square opens on the illustration task
p 0x2000 15
p 0x2000 15
p 0x2000 15
p 0x2000 15
p 0x2000 15
p 0x2000 40
p 0x2000 40
skipto 10 3000
w 90
# TUT05: cursor on the document layout task, grid (14,12): hold LEFT and UP to reach (0,0), then step there
h 0x80 150
h 0x10 150
p 0x20 6
p 0x20 6
p 0x20 6
p 0x20 6
p 0x20 6
p 0x20 6
p 0x20 6
p 0x20 6
p 0x20 6
p 0x20 6
p 0x20 6
p 0x20 6
p 0x20 6
p 0x20 6
p 0x40 6
p 0x40 6
p 0x40 6
p 0x40 6
p 0x40 6
p 0x40 6
p 0x40 6
p 0x40 6
p 0x40 6
p 0x40 6
p 0x40 6
p 0x40 6
w 40
rewind
w 14
s work/tut/s05.bmp
# TUT06: the locked brush-up task two cells left (12,12); the ticker shows its [LOCK] half
p 0x80 6
p 0x80 6
w 40
rewind
w 480
s work/tut/s06.bmp
# pick the first selectable task, run 11/14, skip to the 11/15 choice
chartfirst
w 40
p 0x2000 30
p 0x2000 60
skipto 8 20000
w 260
# 11/15: the port's task list, only for its header and help bar (TUT07 is painted over the original)
p 0x1 90
rewind
w 14
s work/tut/s07.bmp
p 0x4000 60
# Specification x3, skip to the 11/16 choice (squares on the way take their first task)
p 0x2000 15
p 0x2000 15
p 0x2000 15
p 0x2000 15
p 0x2000 15
p 0x2000 40
p 0x2000 40
skipto 8 30000
w 260
# 11/16 (TUT08..14). cs[c] = GameData+0x3438+16*c: state, sickTurns, moodIcon, healthIcon. 0 Mashiro, 1 Misaki, 2 Nanami, 4 Ryunosuke
# TUT08: Misaki and Nanami worrying
poke 0x3448 2
poke 0x3458 2
w 30
rewind
w 14
s work/tut/s08.bmp
# TUT10: Mashiro off sick for 3 days, Nanami worrying
poke 0x3438 3
poke 0x343c 3
w 4
s work/tut/s10.bmp
# TUT13: and Ryunosuke at peak motivation
poke 0x3480 0
w 4
s work/tut/s13.bmp
# TUT09: Nanami back to normal
poke 0x3458 0
poke 0x3480 1
w 4
s work/tut/s09.bmp
# TUT14: Mashiro healthy at peak motivation
poke 0x3438 0
poke 0x343c 0
poke 0x3440 0
w 4
s work/tut/s14.bmp
# menus: Mashiro normal, Misaki and Nanami worrying again
poke 0x3440 1
poke 0x3458 2
# TUT12: turn 1 menu on Meeting
p 0x2000 20
p 0x40 30
rewind
w 14
s work/tut/s12.bmp
# TUT11: on Care
p 0x40 30
rewind
w 14
s work/tut/s11.bmp
# close the menu, Specification x3 for 11/16, skip to the 11/17 choice (a day off: four items)
p 0x4000 30
p 0x2000 15
p 0x2000 15
p 0x2000 15
p 0x2000 15
p 0x2000 15
p 0x2000 15
p 0x2000 40
skipto 8 30000
w 260
# TUT15: turn 1 menu on Rest
p 0x2000 20
p 0x40 15
p 0x40 15
p 0x40 30
rewind
w 14
s work/tut/s15.bmp
# TUT17, still on 11/17: a later chart (route alpha phase), staged. TaskRT t at GameData+0x228+0x18*t: state, visible, prereqOK,
# planDone, workDone, planFinished. Done: 22, 23, 26, 27; working: 188 (4.0 of 4.6 days left), 207 (5.3 left);
# planned: 198; not started: 189, 191. routeA 4, routeB 0 so they are selectable; the chart opens on 188.
# close the Rest menu first
p 0x4000 30
poke 0x438 5
poke 0x44c 1
poke 0x450 5
poke 0x464 1
poke 0x498 5
poke 0x4ac 1
poke 0x4b0 5
poke 0x4c4 1
poke 0x13c8 4
poke 0x13d4 1
poke 0x13d8 2
poke 0x13dc 1
poke 0x1590 4
poke 0x159c 2
poke 0x15a0 4
poke 0x15a4 1
poke 0x14b8 3
poke 0x14c4 1
poke 0x14cc 1
poke 0x13e0 1
poke 0x13e8 1
poke 0x1410 1
poke 0x1418 1
poke 0x3868 4
poke 0x386c 0
poke 0x24 188
# in-game menu (TRIANGLE), first item: the spec square in view mode
p 0x1000 90
p 0x2000 90
rewind
w 480
s work/tut/s17.bmp
