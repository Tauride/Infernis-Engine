# Autotest: checking changes in the running game

`xrEngine.exe -autotest <scenario>` runs a scenario file in the game without
anyone at the keyboard, writes a JSON result and exits with a meaningful exit
code. `run.ps1` does the launching and prints a summary:

```powershell
powershell -ExecutionPolicy Bypass -File tests\autotest\run.ps1 tests\autotest\new_game.cfg
```

`run.ps1` options: `-GameDir` (default `engine_tests_infernis`, next to the
`sources` folder), `-Bin` (default `bin\x64`), `-Timeout` (seconds, default 900),
`-ExtraArgs` (more command-line switches, e.g. `-nosound`), `-Renderer`
(`r1`, `r2a`, `r2`, `r2.5`, `r3` or `r4`: runs with a temporary copy of `autotest.ltx`
that selects that renderer, `autotest_<renderer>.ltx`, deleted afterwards).

The game folder needs the engine binaries, `fsgame.ltx`, the game data and a `user.ltx`
in its application data folder (`$app_data_root$` in `fsgame.ltx`; `userdata` below).
The harness was ported from the X-Ray 1.0 (Shadow of Chornobyl) engine fork.

## Exit codes

| Code | Status | Meaning |
|---|---|---|
| 0 | `passed` | the scenario reached `quit` (or its end) and the game shut down |
| 1 | `failed` | a check failed (`wait_level`, `wait_log`, `expect_log`, `fail_on_log`, `fail`, `screenshot`), or the game quit before the scenario finished |
| 2 | `error` | the scenario cannot be used: missing file, unknown command, bad arguments |
| 3 | `crashed` | R_ASSERT/FATAL/unhandled exception; minidump in `userdata\logs` |
| 4 | `timeout` | over `-autotest_timeout`, no frame for 180 s, or a shutdown over 60 s; `autotest_result_hang.mdmp` |

## Scenario files

One command per line; `#` and `//` start a comment line. Each step runs when
the previous one is done. A line that is not a directive below is a console
command; it must exist (checked before the scenario starts), and the next step
runs one frame later so the engine can process it.

| Directive | Effect |
|---|---|
| `wait_frames <n>` | wait n frames |
| `wait_seconds <s>` | wait s seconds of real time |
| `load_save <name>` | load a save from `userdata\savedgames`: from the main menu through `start server(<name>/single/alife/load) client(localhost)` (as the menu does), in a game through `load` |
| `wait_level [<s>]` | wait until a level is loaded, the actor exists and the load screen is gone (default limit 300 s; fails after). The load screen waits for a key unless `keypress_on_start` is 0, which `run.ps1` sets |
| `wait_log <s> <text>` | wait until a log line (since the start) contains text; fails after s seconds |
| `expect_log <text>` | fail unless a log line (since the start) contains text |
| `fail_on_log <text>` | from now on, fail as soon as a log line contains text |
| `screenshot <name>` | save the finished frame to `userdata\screenshots\autotest\<name>.png`; works windowed. Fails on a multisampled or non-8-bit back buffer (the engine creates neither) |
| `screenshot_overlay <name>` | the same, taken at the end of the frame: with the debug overlays (the FPS overlay), the UI cursor and the engine statistics |
| `perf_begin <label>` / `perf_end` | frame-time statistics between the two (avg, p50/p95/p99, max, fps) |
| `log <text>` | write `* [autotest] text` to the log |
| `lua <code>` | run the rest of the line as a chunk of Lua in the game's script engine (globals stay for later steps); fails with the Lua error. A returned value is written to the log as `* [autotest] lua: <value>` |
| `lua_expect <expression>` | fail unless the Lua expression is true (not `nil` or `false`); its value goes to the log and into the failure reason |
| `fail <text>` | fail with this reason |
| `debug_crash assert\|access_violation` | crash on purpose (tests the crash path) |
| `debug_device_lost [<n>]` | the device is lost for n frames (default 30): no frames are drawn, then the device is reset, as Direct3D 9 does it for real; waits until it is restored |
| `quit` | pass, and quit the game |

Note: the `load` console command only works in a running game; from the menu
it logs `! ALife simulator has not been started yet`. Use `load_save`.

A new game needs no save: `start server(all/single/alife/new) client(localhost)`, then
`wait_level`, is what the main menu's "New game" does (see `new_game.cfg`). The `-start`
command line option does the same without the main menu (see `start_option.cfg`).

### UI: mouse and keyboard

The `ui_*` steps drive the menus and in-game windows like a player: they move a
cursor, hover, click, drag and press keys. The events go to the game's input
receiver as if they came from the devices, so the real mouse pointer is never
moved and the window doesn't need the focus. In autotest mode the UI cursor is
virtual: it starts in the middle of the screen and only these steps move it.
The real mouse and keyboard are still read while the game window has the focus:
don't click or type in it during a run.

A **target** is a point, `512,300` in UI coordinates (always 1024x768, whatever
the resolution), or a path of segments separated by `/`:

| Segment | Matches |
|---|---|
| `btn_options` | a window with this name |
| `option:rs_c_gamma` | an options control bound to this console command |
| `item:medkit` | an inventory/trade/loot cell holding an item of this section |
| `st:st_move_on_belt` | a window showing the translation of this string table id: the same in every language |
| `class:CUIPropertiesBox` | a window of this C++ class (for unnamed containers) |
| `text:Options` | a window with this text (case-insensitive), else one whose text contains it; takes the rest of the target |
| `#2` | the third visible child of the window before it (not first) |

The first segment matches anywhere, a `#n` segment a direct child of the previous
match, any other segment a window anywhere below it: `tab/#2` is the third tab
button, `option:rs_c_gamma/#1` the slider's thumb. When several windows match, the
topmost visible one is used; it is clicked in the middle of its visible part, and
windows scrolled out of a list don't count.

Windows are searched in the dialogs that take input now: the main menu when it
is open, otherwise the game's own dialogs (actor menu, PDA, talk, ...). Run
`ui_dump` to see what is on the screen: one line per visible window with its
`#n`, class, name, `[x,y wxh]` rect, state (`disabled`, `checked`/`unchecked`,
`clipped`), `option=`, `item=`, `st=` (the text's string table id) and `text=`. The file is in the game's encoding
(cp1251); read it with `iconv -f cp1251 -t utf-8`.

| Directive | Effect |
|---|---|
| `ui_dump [<label>]` | write the visible windows to `userdata\logs\autotest_ui_<label>.txt` |
| `ui_move <target>` | move the cursor onto the target (hover) |
| `ui_click <target>` | move, wait the hover time, press and release the left button (one frame each) |
| `ui_rclick <target>`, `ui_dblclick <target>` | the same with the right button / two left clicks |
| `ui_drag <from> -> <to>` | move to `from`, press, move to `to` in 8 frames, release |
| `ui_down [left\|right\|middle]`, `ui_up [...]` | press or release a button where the cursor is |
| `ui_wheel <n>` | scroll n steps (up is positive) |
| `ui_key <key>` | press and release a key: a key name (`kESCAPE` or `ESCAPE`, `kI`, `mouse1`) or a bound action (`inventory`, `quick_save`) |
| `ui_key_down <key>`, `ui_key_up <key>` | hold or release a key (e.g. `kLSHIFT` around a click) |
| `ui_wait <s> <target>` | wait until the target is visible; fails after s seconds |
| `ui_expect <target>`, `ui_expect_no <target>` | fail unless / if the target is visible now |
| `ui_hover <s>` | the pause between moving onto a target and pressing (default 0.1 s) |

Some windows react to the hover itself and animate: the main menu buttons
select on hover and a highlight then slides over them, and only a click inside
the highlight presses the button. Move first and wait, as `ui_options.cfg` does.
Common widgets:

- **Dropdowns**: `ui_click option:vid_mode` opens the list (`option:vid_mode/class:CUIListBox`),
  `ui_click option:vid_mode/class:CUIListBox/text:1024x768` (or `/st:<id>` for localized
  values) picks a value. Entries below the list's edge are `clipped`: `ui_move` onto the
  list and `ui_wheel -<n>` first. An open list captures the mouse: a click elsewhere only
  closes it.
- **Right-click menus** (inventory, trade, loot): `ui_rclick item:<section>`, then
  `ui_click class:CUIPropertiesBox/st:<id>`.
- **Drag and drop**: `ui_drag item:<section> -> <list>`, the list being a target taken
  from a `ui_dump` of the open window.

Limits:

- The mouse steps (`ui_move`, `ui_click`, `ui_rclick`, `ui_dblclick`, `ui_drag`)
  fail when the UI cursor is hidden (in a level with no menu or dialog open):
  the events would go to the actor and turn the camera or fire.
- `ui_wait`, `ui_expect` and `ui_expect_no` take windows, not points.
- The finder doesn't check whether another window covers the target: with a
  message box over a dialog, a click lands on whatever is on top at that point.

Separate presses are at least 0.3 s apart (the UI turns two left presses within
250 ms into a double click, and menus run at thousands of frames per second).
Key names and bound actions are checked before the scenario starts.

## The result

`userdata\logs\autotest_result.json` (or `-autotest_out <path>`). It is rewritten
at every step, so after a hang or a kill it shows the last step reached.
The file is UTF-8: log lines in the game's cp1251 are converted.
Fields: `status`, `exit_code`, `phase` (`startup`, `scenario`, `shutdown`,
`done`), `reason`, `step`/`steps_total`/`step_line`/`step_text` (the current or
failing step), `elapsed_s`, `frames`, `level`, `log` (the full engine log),
`dump`, `peak_working_set_mb`, `peak_commit_mb`, `screenshots`, `ui_dumps`,
`ui_cursor` (the virtual cursor, UI coordinates), `perf`,
`log_errors` (lines starting with `!`; a plain start already logs about 8),
`log_error_samples` (the first 30), `log_tail` (the last 30 lines).

## How the game behaves in autotest mode

- `-autotest` implies `-unattended`: fatal errors never open a dialog
  (xrDebug, Windows error boxes) and never touch the clipboard; they
  log the error and stack trace, write a minidump to `userdata\logs` and exit
  with code 3.
- The window keeps rendering and the game does not pause without the focus.
- `run.ps1` passes `-ltx autotest.ltx -nointro -i`: its own settings file
  (created from `user.ltx` at 1280x720, so the user's settings stay as they are; every
  run sets `rs_screenmode screenmode_windowed`, `rs_fps_limit off` and
  `keypress_on_start 0` in it), no intro videos, no captured input.
- Only one game can run at a time; a running game also locks `bin\x64`, so
  builds fail while it runs.

## Scenarios here

| File | Checks | Needs |
|---|---|---|
| `menu.cfg` | start, main menu, shutdown (~8 s) | nothing |
| `new_game.cfg` | starts a new game as the menu does, the first level comes up, screenshot, 20 s idle perf (~35 s) | nothing |
| `ui_options.cfg` | the ui_* steps on the menus: opens the options with the mouse, picks a dropdown value, switches a tab, cancels; opens the options again and closes them with Escape (~6 s) | nothing |
| `screen_mode.cfg` | the screen mode dropdown in the video options: "Borderless window" locks the resolution, "Windowed" unlocks it, Cancel leaves the mode alone, accepting borderless resets the device to the display's size; back to a window from the console. Covers the whole display for a few seconds (~15 s) | nothing |
| `resolution_aspect.cfg` | accepting a resolution with another aspect ratio (16:9 to 4:3 and back): the main menu is rebuilt, the options come back with the "keep these settings?" box; Yes keeps the resolution, No restores the old one (~12 s) | nothing |
| `fps_limit.cfg` | the frame rate limit dropdown in the video options: the list (30, 60, half/full/twice the refresh rate without repeats, Unlimited), picking 30 with the mouse; main menu perf at 30, 60, the refresh rate and unlimited (~25 s) | nothing |
| `fps_overlay.cfg` | the FPS overlay (`rs_show_fps`): pictures with it, after a device reset, with it turned off, and a plain screenshot without it; the limit steps 60, 30, 60 for the graph (~16 s). The overlay is off by default; the scenario turns it on and leaves it off | nothing |
| `device_lost.cfg` | a lost device and the reset after it, three times: in the main menu, while a level starts, in the level; with the FPS overlay on. Run with `-Renderer r1`, `r2`, `r3`, `r4` (~17 s) | nothing |
| `start_option.cfg` | the `-start` command line option goes straight into a new game; run with `-ExtraArgs "-start server(all/single/alife/new) client(localhost)"` (~10 s) | nothing |
| `soc_cordon.cfg` | SoC port: a new game on the converted Cordon (`l01_escape`) with an actor-only spawn. The level comes up in Sidorovich's bunker; the actor is put in the village, walks from Wolf's post towards a point Wolf watches (checked: closer by 5 m, still on the ground), then at the bridge, the north and the south end. FPS overlay on, limit 288, pictures with the overlay (~50 s). Needs the SoC addon pack with the `soc` spawn in the game folder | nothing |
| `ui_inventory.cfg` | in a level: opens the inventory with the bound key, finds the starting rifle by its section, closes it with Escape (~17 s) | nothing |
| `selftest_assert.cfg`, `selftest_av.cfg` | the crash path: expect exit code 3 | nothing |
| `selftest_timeout.cfg` | the watchdog: run with `-Timeout 20`, expect exit code 4 | nothing |

Scenarios that load a save use fixtures named `autotest_*` (they are not in the
repository - copy one of your saves under that name). Never use or change the user's
own saves.
