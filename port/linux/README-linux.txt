Sakurasou no Pet na Kanojo - native PC port (Linux, Russian translation)

SETUP
  1. Unpack this folder anywhere.
  2. Start the game: run ./Sakurasou.sh (or ./sakurasou in a terminal).
     If your file manager asks, choose "Run" / "Execute". If the file is not executable:
       chmod +x Sakurasou.sh sakurasou
  3. The first time, the game asks for your copy of the game. Drag the .iso file onto the window that opens,
     or copy the .iso into this folder. You only do this once; the location is remembered.

  Nothing from the game is included here: you need your own PSP image of the game.
  Needs a 64-bit Linux with OpenGL or Vulkan graphics and a sound server (PipeWire, PulseAudio or ALSA).

LANGUAGE
  The game starts in Russian. Settings has a "Language" row to switch to Japanese and back.
  Start with --no-tl to play the untouched Japanese version.

CONTROLS
  Confirm / advance ... Z, Enter, Space, left click, gamepad A
  Cancel .............. X, Esc, Backspace, right click, gamepad B
  Menu (in scenes) .... S / gamepad Y        Square ...... A / gamepad X
  Hide UI (hold) ...... Q, PageUp / LB       Skip (hold) . W, PageDown, Left Ctrl / RB
  Backlog ............. Tab / Back           Auto mode ... F / Start
  Fullscreen .......... F11 or Alt+Enter

SAVES
  ~/.local/share/Sakurasou/SakurasouPort/   (copy this folder to move your saves)

TRANSLATION FEEDBACK
  F8 marks the line on screen as wrong (stored in translation/feedback.tsv).
  Untranslated lines seen while playing are logged in translation/untranslated.tsv.

LICENCE
  Free software under the GNU GPL v3 or later (LICENSE.txt). Third-party notices: THIRD_PARTY.txt.
