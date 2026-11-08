#!/bin/bash
# Captures the port screens for the Russian tutorial pictures into work/tut/ (tools/tl_tutorial.ctl), on a fresh
# temporary save directory. Takes a while: it plays the first week of the game headless. Then run tl_images.py.
set -e
cd "$(dirname "$0")/.."
mkdir -p work/tut
save=$(mktemp -d)
trap 'rm -rf "$save"' EXIT
python3 tools/tl_build.py >/dev/null
SAKURA_SAVE_DIR=$save SDL_VIDEODRIVER=dummy port/build/sakurasou --headless --tl translation/build --ctl tools/tl_tutorial.ctl >/dev/null
ls work/tut
