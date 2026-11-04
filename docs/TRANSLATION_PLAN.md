# Russian translation plan

Status 2026-11-03: phases 1-4 done. All 27,431 strings are drafted (status `draft`). The AI edit pass read all 414 scenes ($14.50) and proposed changes; 879 were applied (`by=edit`), 1 skipped. The pack builds with 0 errors and 1,347 warnings (mostly glossary-stem false positives). Phase 5 (art) is done except the decorative cards listed at the end (109 images in `images_ru.tsv`). Phases 6 and 7 are next.

Decisions by Misha, 2026-11-02:
- Names: Polivanov system (Масиро Сиина, Дзин, -сэмпай).
- Review: play and flag only. There is no line-by-line human read, so the AI editing pass carries the quality.
- Release: public release planned. Ship only the Russian pack, redrawn art and tools; players supply their own ISO.
- Title logo: keep the Japanese original.

This replaces `.opencode/plans/sakurasou-ru-translation.md`. Its goals stand (Russian, full scope, AI draft plus human
review, ISO untouched, override directory). Three of its technical assumptions turned out wrong; see "What changed".

## How it works

```
ISO -> tl_extract.py -> work/tl/src/*.json          Japanese + speaker + order (not in git)
                            |
 style_ru.md, glossary  ->  tl_draft.py (claude CLI) -> translation/ru/*.jsonl   key + Russian (in git)
                                                             |
                                 tl_review.py (browser)  <-> | -> tl_build.py -> translation/build/ru.pack
                                 F8 in the game flags lines  |                            |
                                                                       sakurasou --tl translation/build
images_ru.tsv -> tl_images.py -> translation/build/assets/ (replaces art by file name)
```

Text is swapped when it is displayed. Scripts run unmodified, so the VM, branching, saves and lip sync keep seeing the
Japanese strings they compare against. Lines without a translation fall back to Japanese, so a partial pack is playable.

Files in git hold only hash keys and Russian. The Japanese is regenerated from the ISO, so the repo holds no game script.

## What changed from the earlier plan

| Earlier assumption | Finding | Decision |
|---|---|---|
| Patch and rebuild `.ssb` files | Speaker names are compared by lip sync, so changing them in scripts breaks it | Swap text at display time. No relocation code is needed |
| No font work: Cyrillic is in the font | True, but at 14 px per letter a line holds about 29 letters. Russian needs 2.1x the characters of Japanese | Proportional spacing and automatic word wrap, for Cyrillic strings only |
| No texture changes | Menu items, page headers, location plates, dates, name art and the legal notice are images | Redraw them; the override directory is kept for this |
| Key strings by text | The same Japanese line can need a different Russian gender | Dialogue keyed by message id + text |

## Pilot results

Scope: 406 strings. Opening scene, prologue, one event with TIPS links, one with thought clouds, one with a choice, all
47 names, 102 interface strings, 14 redrawn images.

Tested by running:
- Boot -> title -> New Game -> opening scene -> backlog -> menu, in Russian, headless.
- Message window, name plate, choice, thought cloud, mail window, popup, TIPS link text, title menu, page header.
- All 414 story scripts finish with and without the pack.
- Without `--tl`, frames match the `main` build except eye blinks, which are randomly seeded and also vary between two
  runs of the same build.
- C++ and Python wrap agree on all compared strings, so the review preview shows what the game shows.
- Review tool in a real browser: edit, glossary warning clears, accept, file and pack update.
- The ISO's size and date are unchanged.

Not tested: the simulation screens in Russian, and anything by a human reader. The translation quality has not been judged by a native
speaker. My read of the samples is that it is good but not final.

Measured:

| | Sonnet | Opus |
|---|---|---|
| Cost for the same 292 strings | $0.38 | $1.08 |
| Messages needing a third line (of 136) | 15 | 3 |
| Failing automatic checks | 1 | 0 |
| Character voice | flatter | clearly better (Misaki, jokes kept) |

Decision: Opus drafts. Across the pilot, 116 messages took 1 line, 131 took 2 and 3 took 3. Over-long messages are rare.

The checks caught real problems: a misspelt surname, inconsistent 生徒会長 («глава» vs «президент»), literal `\n` from the
model, cloud strings mislabelled by the extractor. Glossary coverage matters more than the model.

## Size and cost

| Text | Strings | Note |
|---|---|---|
| Story scripts | 25,135 | 493k Japanese characters, 19.7k unique dialogue lines |
| Data tables | about 1,810 unique | tips, tasks, reviews, teams |
| Interface | about 440 | 339 of them in `menus_data.h` |
| Art | about 110 files | estimate from the contact sheets |

Draft: about $0.0036 per string in the pilot, so roughly $100 for everything, as reported by the CLI at API prices. On a
subscription this counts against usage limits instead. Run time was about 1 s per string, so about 8 hours of calls
spread over several sessions. An AI editing pass costs about the same again.

## Phases

1. **Finish the tooling.** Extract data tables and `menus_data.h`. Handle built strings (dates, `X日`, `TURN n`), the help
   bar line, baked date glyphs. Add a runtime log of untranslated strings shown on screen. Add a language option in Settings.
   Done when one playthrough logs no unexplained Japanese.
2. **Prepare context.** AI pass over the whole corpus: full glossary, terms, per-character speech sheets, a one-paragraph
   summary per scene. Human approves the glossary before drafting. This is the cheapest place to prevent errors.
3. **Draft.** Common route first, then each heroine's route, then events and tables. Pass the previous scene's ending and
   the summaries as context. Keep scenes in order, so each route reads as one text.
4. **AI edit.** A second pass reads whole scenes for consistency, voice and overlong lines, and only proposes changes.
5. **Art.** All redraws, with a font chosen for the look of the original. Decide on the title logo.
6. **Human review.** See below.
7. **Playtest and release.** Bot run of all six routes in Russian, then a human run, then update `docs/DIFFERENCES.md`.

## Phase 1 result

- Extractor now covers 25,135 script strings, 47 names, 461 interface strings (incl. `menus_data.h`, `sim_sprites.h`) and 1,788 data-table strings
  (`_tables.json`, with a `fit` rule per field). Pilot keys are unchanged.
- Engine: untranslated-string log (`untranslated.tsv`), `trFmt` for built strings, translated task names, Russian save-slot date,
  Russian help-bar ticker split on word boundaries, Russian weekday/month/countdown card in the calendar instead of baked kanji,
  language row in Settings (saved in `lang.txt`, Japanese also turns the redrawn art off), dev option `--scene ID`.
- `tools/tl_coverage.py --run` shows 27 scenes with a button sweep: 0 gaps (every Japanese string shown is in the source).
  This is a sweep, not a human playthrough; a string that only appears in a rare state could still be missed.
- Still art, for phase 5: calendar milestone pills (calendar_parts), "TURN"/date headers if baked elsewhere, option-screen row labels.
- `--scan`: 429 scripts finish with the pack; `SCR_TGS_0000` times out with or without it (a debug script outside the retail flow).

## Human review: play and flag

Review happens by playing. Press F8 on anything wrong, then fix flagged lines in the review tool. The rest of the review
tool stays available for spot checks and for the lines the automatic checks warn about.

Because nobody reads every line, three things become mandatory:
1. The glossary and character speech sheets are approved by a person before drafting.
2. Phase 4 runs on all text, and uses a separate model pass that reads whole scenes as an editor.
3. Six routes do not all get played closely at first. Lines flagged in one route feed the style guide, and the AI re-edits
   the other routes with those corrections.

F8 and F5 are the core of review. Both show an on-screen confirmation, and Misha's real F8 presses were recorded correctly.

## Risks

- Readability: the 14 px bitmap font is fine at 3x scale but thin. A sharper font at higher resolution is possible later.
- Built strings may be missed by exact lookup. Phase 1's log finds them.
- A save stores the on-screen line, so a line saved in Russian comes back in Russian in Japanese mode. Cosmetic.
- Public release: with no full human read, errors will reach players. Consider calling the first release a beta.
- Licences: redrawn art contains original artwork, so it may need rebuilding from the player's ISO at install time, which
  `tl_images.py` already does in principle. The art font must allow embedding and redistribution.

## Phase 5 result (2026-11-04)

`images_ru.tsv` now drives 109 files; `tools/tl_images.py` has one function per kind (the TSV holds only the Russian text,
the geometry lives in the script). Pages, plates and menus were done earlier. Added in this pass, all checked in a
headless render or in-game:

- Simulation: action choice (`simchoice`, `simcard`), deadline widget (`deadline`), calendar tags (`calpills`), story date
  ribbon (`convdate`, drawn as «12.11 (пн)» by `AdvScene::drawDate`), game-quality badge and stat chips (`gamebadge`),
  spec-square state pills (`squareparts`), the absent plate (`actionwin`).
- Menus: stat chips round the status hexagon (`psrts`), character info parts and name plates (`infoparts`, `infonames`),
  save/load genre tags and headings (`saveparts`, `savewindow`), backlog choice tags (`backlog`), DLC menu (`dlc`, plus a
  tenth entry in `menubutton`), contest ranking banner (`rankfont`).
- Story: friendship-up banner (`friend`).
- Name lists of the extras (`rowlist`: menu, event, pose, voice). The voice actors' kanji names are left as they are: the
  port never draws that column.
- Legal notice (`legal`), written as a plain TXG under the `.txgtl` name (the loader goes by magic).

Engine changes were needed only where the Russian text has a different order or length: the story date and the deadline
widget. Everything else replaces pixels in place, so Japanese mode is untouched (art is off there).

Deliberately left Japanese:
- Title logo and `title_trial`, by decision. `logo_*` company logos.
- `img_plan_01` (69 concept cards of in-story games with painted titles), `img_alpha_*` and `img_beta_*` screenshots,
  `menu_img` thumbnails, and the tiny mock-up screens inside `menu_button` and `omake_menu_img`: decorative, each card
  would be hand art. If wanted, they are the next art task.
- Atlases the port never loads (`common_calendar`, `common_help_parts`, `task_name`, `task_psrts`, `note_bg`,
  `information_font`) and the CV column of `omake_voice_ch_name`.

Quality notes: small lettering in the narrowest tags (`Повседн.`, `Стабил.`) is cramped; a few stray outline pixels remain
under some captions. Judge them by playing.

Phases 6 and 7 (human play-through, bot run of all six routes in Russian, update `docs/DIFFERENCES.md`) are not started.
