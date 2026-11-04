#!/bin/sh
# Double-click launcher (or run from a terminal). Starts the game from the folder it sits in.
cd "$(dirname "$0")" || exit 1
exec ./sakurasou "$@"
