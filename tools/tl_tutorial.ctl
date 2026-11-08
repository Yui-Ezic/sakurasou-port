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
# TUT07, 11/15: the task list (SELECT); wait for its chips and badges to appear
p 0x1 90
w 90
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
# TUT16: the contest ranking, staged: our team 43rd of 50 (the list stops with it in view), the rest by team id;
# TeamRT t at GameData+0x3598+12*t: prevRank, score, rank. The contest scene is started over everything.
nohelp
poke 0x35a4 3
poke 0x35a8 26600
poke 0x35ac 1
poke 0x35b0 1
poke 0x35b4 26200
poke 0x35b8 2
poke 0x35bc 0
poke 0x35c0 25800
poke 0x35c4 3
poke 0x35c8 6
poke 0x35cc 25400
poke 0x35d0 4
poke 0x35d4 3
poke 0x35d8 25000
poke 0x35dc 5
poke 0x35e0 0
poke 0x35e4 24600
poke 0x35e8 6
poke 0x35ec 9
poke 0x35f0 24200
poke 0x35f4 7
poke 0x35f8 6
poke 0x35fc 23800
poke 0x3600 8
poke 0x3604 0
poke 0x3608 23400
poke 0x360c 9
poke 0x3610 12
poke 0x3614 23000
poke 0x3618 10
poke 0x361c 9
poke 0x3620 22600
poke 0x3624 11
poke 0x3628 0
poke 0x362c 22200
poke 0x3630 12
poke 0x3634 15
poke 0x3638 21800
poke 0x363c 13
poke 0x3640 12
poke 0x3644 21400
poke 0x3648 14
poke 0x364c 0
poke 0x3650 21000
poke 0x3654 15
poke 0x3658 18
poke 0x365c 20600
poke 0x3660 16
poke 0x3664 15
poke 0x3668 20200
poke 0x366c 17
poke 0x3670 0
poke 0x3674 19800
poke 0x3678 18
poke 0x367c 21
poke 0x3680 19400
poke 0x3684 19
poke 0x3688 18
poke 0x368c 19000
poke 0x3690 20
poke 0x3694 0
poke 0x3698 18600
poke 0x369c 21
poke 0x36a0 24
poke 0x36a4 18200
poke 0x36a8 22
poke 0x36ac 21
poke 0x36b0 17800
poke 0x36b4 23
poke 0x36b8 0
poke 0x36bc 17400
poke 0x36c0 24
poke 0x36c4 27
poke 0x36c8 17000
poke 0x36cc 25
poke 0x36d0 24
poke 0x36d4 16600
poke 0x36d8 26
poke 0x36dc 0
poke 0x36e0 16200
poke 0x36e4 27
poke 0x36e8 30
poke 0x36ec 15800
poke 0x36f0 28
poke 0x36f4 27
poke 0x36f8 15400
poke 0x36fc 29
poke 0x3700 0
poke 0x3704 15000
poke 0x3708 30
poke 0x370c 33
poke 0x3710 14600
poke 0x3714 31
poke 0x3718 30
poke 0x371c 14200
poke 0x3720 32
poke 0x3724 0
poke 0x3728 13800
poke 0x372c 33
poke 0x3730 36
poke 0x3734 13400
poke 0x3738 34
poke 0x373c 33
poke 0x3740 13000
poke 0x3744 35
poke 0x3748 0
poke 0x374c 12600
poke 0x3750 36
poke 0x3754 39
poke 0x3758 12200
poke 0x375c 37
poke 0x3760 36
poke 0x3764 11800
poke 0x3768 38
poke 0x376c 0
poke 0x3770 11400
poke 0x3774 39
poke 0x3778 42
poke 0x377c 11000
poke 0x3780 40
poke 0x3784 39
poke 0x3788 10600
poke 0x378c 41
poke 0x3790 0
poke 0x3794 10200
poke 0x3798 42
poke 0x3598 0
poke 0x359c 9800
poke 0x35a0 43
poke 0x379c 46
poke 0x37a0 9400
poke 0x37a4 44
poke 0x37a8 43
poke 0x37ac 9000
poke 0x37b0 45
poke 0x37b4 0
poke 0x37b8 8600
poke 0x37bc 46
poke 0x37c0 49
poke 0x37c4 8200
poke 0x37c8 47
poke 0x37cc 46
poke 0x37d0 7800
poke 0x37d4 48
poke 0x37d8 0
poke 0x37dc 7400
poke 0x37e0 49
poke 0x37e4 52
poke 0x37e8 7000
poke 0x37ec 50
poke 0x37f0 49
poke 0x37f4 6600
poke 0x37f8 51
push 14
w 60
p 0x2000 20
p 0x2000 20
p 0x2000 20
p 0x2000 20
p 0x2000 20
p 0x2000 20
p 0x2000 20
p 0x2000 20
p 0x2000 20
p 0x2000 20
p 0x2000 20
w 420
s work/tut/s16.bmp
