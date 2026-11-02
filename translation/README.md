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

    python3 tools/tl_draft.py --model opus SCN_MAIN_1Y_1112__020

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

## Check the engine and tools agree

    port/build/sakurasou --tl translation/build --tl-dump /tmp/dump.tsv
    python3 tools/tl_build.py --verify /tmp/dump.tsv
