# Status and differences from the PSP version

Tested means run in this project. Nothing has been compared frame-by-frame with the original, because no working PPSSPP reference was available (see RE_NOTES). Timing and layout come from reading the game's code.

## Verified by running
- Boot chain, logos, opening movie decode, title, option screen (headless screenshots). A real Wayland window with the GPU renderer ran 500 frames of boot with no errors. Sound was never listened to.
- 429 of 430 scenario scripts complete through the real ADV at skip speed. The exception, SCR_TGS_0000, is a trial script with no route to it in the retail game and needs a model that is not on the disc.
- All six routes reach their ending, ending movie, clear flag and finish screen via bot runs, using 245 scenario scenes per route. These runs used boosted stats and scripted task completion, so they prove flow, not balance. An unboosted bot game-overs at turn 13 and a stat-boosted run without task completion game-overs at turn 42, as designed; no human or skilled unboosted playthrough has been done.
- Save, load in game and from the title, quick load, backlog, gallery launch of events (menu module tests).
- HCA, ADX, ATRAC3+ and H.264 decoding: HCA matches ffmpeg within rounding at inverted polarity (inaudible), ADX matches in shape with a 0.1% level offset.

## Written but never run
- 0x11, 0x12, DLC scenes 0x24-0x27 (no DLC data exists on the disc).
- CG fullscreen viewer and pan, save page turn, overwrite popup.
- Mouse hover/click/wheel, real gamepads. Keyboard paths were tested through injected button masks only.
- Calendar left/right scroll, countdown card, job 7 appearance.
- Real voice-driven lip sync (only forced-voice tests ran).
- Windows: no Windows build was attempted.
- Full-speed play by a person, A/V sync of the movies by eye and ear.

## Known differences
- PSP savedata dialogs and on-screen keyboard are replaced by the in-game slot screen and native text entry. The game-title box is a plain panel rather than the original art.
- DLC is shown as unavailable: DR01-04 data is not on the disc. Install option is inert.
- Sound channels use linear per-sample volume ramps rather than frame-stepped ones.
- Assets load synchronously, so the original's load waits do not occur.
- In-message TIPS viewer is a plain panel with the real text, not the original layout.
- Calendar note-lift transition (style 3) falls back to a fade; page header decoration may be incomplete.
- Task list (job 0xd) layout was not recoverable and is an approximation; contest ranking tweens and parts of chart layout are too.
- Pose gallery shows number digits rather than outfit labels; info screen omits the deadline box; button glints and omake petals are not drawn.
- About 26 `ASSUMED` comments in port/src mark remaining guesses. Spec open items: docs/spec_sim.md section 14, docs/spec_flow_menus.md section 13, docs/spec_adv.md section 16.
- Saves are not interchangeable with PSP saves: the outer layout matches, but the job-state blob uses its own encoding.
- Cards' row-to-action mapping in the action scene is [ASSUMED]; a wrong mapping would change gameplay, so this is the first thing to check against the original.

## Russian mode
Only with `--tl`. Cyrillic text uses proportional spacing and word wrap instead of the original fixed 14 px cell, and up to 3 lines at a 15 px pitch. Long Russian title-menu labels use wider sprite cells. Japanese mode is unchanged. Status: `docs/TRANSLATION_PLAN.md`.

## Russian pack (`--tl translation/build`)
- All 27,425 strings are AI-drafted and AI-edited; no human has read them. Status of the pack is in `docs/TRANSLATION_PLAN.md`. The human play-through and review (phase 6) was skipped by decision, so expect errors.
- 429 of 430 scripts complete with the pack, same as without; the exception is the same debug script `SCR_TGS_0000` (2026-11-04).
- A button-sweep of 27 scenes (`tools/tl_coverage.py --run`, 900 frames each) shows no Japanese string without a translation. This is a sweep, not a playthrough: strings in rare states could still be missed.
- 109 images are redrawn (see "Phase 5 result" in the plan). Not redrawn: title logo, company logos, the 69 in-story concept cards and game screenshots, mock-up thumbnails inside menus.
- Not rerun in Russian: the six full-route bot runs.
