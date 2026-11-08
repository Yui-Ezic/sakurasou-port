# Russian translation: how to work on it

Plan and results: `docs/TRANSLATION_PLAN.md`. Files here hold no Japanese, only hash keys and Russian.

| Path | What |
|---|---|
| `style_ru.md` | Rules for translators; sent to the AI as its instructions |
| `glossary_ru.tsv` | Names and terms; the third column is a stem the checker looks for |
| `images_ru.tsv` | Text for redrawn art |
| `ru/<scene>.jsonl` | One line per string: `k` key, `ru` text, `st` draft/ok/fix/reject, `by`, `note` |
| `build/` | Generated pack and art; not in git |

## Setup
`work/data` must exist (`tools/cpk.py`, see `docs/RE_NOTES.md`), then:

    python3 tools/tl_extract.py

## Draft a scene with AI

    python3 tools/tl_draft.py SCN_MAIN_1Y_1112__020      # default: --model auto --effort low (see route() in tl_draft.py)

Existing lines are kept unless `--force`. Usage is logged to `work/tl/usage.jsonl`.

## Review

    python3 tools/tl_review.py        # then open http://127.0.0.1:8765

Japanese, Russian and a preview in the game's font, side by side. Ctrl+Enter accepts and moves on. Every save rebuilds
the pack. Filters show unreviewed lines or lines with warnings. Needs internet for the Japanese web font unless a CJK
font is installed.

## Play it

    python3 tools/tl_build.py
    ~/tools/sakura-venv/bin/python tools/tl_images.py
    port/build/sakurasou --tl translation/build

F8 flags the line on screen and shows a confirmation at the top of the game window. Flagged lines get ⚑ in the review
tool, and accepting a line there clears its flag. F5 reloads the pack, also with a confirmation; the change appears from
the next message. Flags are stored in `translation/build/feedback.tsv`.
Rejected lines and lines with errors stay out of the pack, so the game shows the Japanese there.

## Tutorial pictures

The tutorials (SCN_TUTORIAL_01..07) show BG_TUT01..17.cbg: shrunken game screens with the Japanese UI baked in. They
are rebuilt from the port's own Russian screens:

    tools/tl_tutorial_shots.sh                         # plays the first week headless, saves work/tut/sNN.bmp
    ~/tools/sakura-venv/bin/python tools/tl_images.py  # rows 'tutorial' in images_ru.tsv -> build/assets/BG_TUT*.cbg

The steps are in `tools/tl_tutorial.ctl` (see the `--ctl` comment in `port/src/main.cpp`); states the tutorial art
stages (illness, worries, the later chart of TUT17) are poked into GameData. `tools/tl_tutorial.py` pastes each capture
into the original frame, copies the red arrows from the original and redraws the rings. TUT07 (the task list, which
the port does not draw like the original) and the drawing in TUT14 get Russian text painted over the original; their
words are in `images_ru.tsv`. Check the pictures after any change to those screens.

## Language switch and coverage log

With `--tl`, Settings has a ninth row, Language: 日本語 / Русский. The choice is saved in `translation/build/lang.txt`; Japanese
also switches the redrawn art off. While playing, every Japanese string that reaches the screen without a translation is
appended once to `translation/build/untranslated.tsv`.

    python3 tools/tl_coverage.py --run     # shows every scene for 900 frames with a button sweep, then reports
    python3 tools/tl_coverage.py           # reports on the current log (also after a normal play session)

"In the source" means the string is extracted and just not translated yet. A **GAP** is a string the extractor does not know:
built at run time or loaded from somewhere new. Gaps are bugs in the tooling; fix them in the engine (`trFmt`, a table in
`tl_extract.py`) rather than by translating around them.

Texts with a fixed layout carry a `fit` rule from the extractor (line width, line count): data tables, the help-bar ticker.
There is no automatic wrap for them, so the translator sets `\n` himself; `tl_common.check` flags lines that do not fit.

## Context for drafting (phase 2)

    python3 tools/tl_context.py terms      # candidate terms -> translation/glossary_proposal.tsv
    python3 tools/tl_context.py voices     # speech sheets   -> translation/characters_ru.md

Both are proposals. Nothing reads them until a person approves them and moves the glossary rows into `glossary_ru.tsv`.

## Check the engine and tools agree

    port/build/sakurasou --tl translation/build --tl-dump /tmp/dump.tsv
    python3 tools/tl_build.py --verify /tmp/dump.tsv
