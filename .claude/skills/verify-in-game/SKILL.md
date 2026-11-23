---
name: verify-in-game
description: Check an engine or gamedata change in the running game with the autotest harness - run a scenario, read the JSON result, look at screenshots and frame times, or write a new scenario. Use after building a change that affects runtime behaviour, rendering, loading, scripts, UI or performance, or when asked to test or confirm something in the game.
---

# Verifying changes in the game

`xrEngine.exe -autotest <scenario>` plays a scenario file in the game without anyone at
the keyboard and exits with a code. The full format is in `tests/autotest/README.md`.
Read it before writing a scenario.

## Run

Build first (x64, `RelWithDebInfo`), then run from the repository root:

```
powershell -ExecutionPolicy Bypass -File tests\autotest\run.ps1 tests\autotest\menu.cfg
powershell -ExecutionPolicy Bypass -File tests\autotest\run.ps1 tests\autotest\new_game.cfg
```

| Scenario | Checks | Time |
|---|---|---|
| `menu.cfg` | the engine starts, renders the main menu, shuts down | ~8 s |
| `new_game.cfg` | a new game starts, the first level comes up, screenshot, 20 s idle perf | ~35 s |
| `ui_options.cfg` | the UI steps on the menus: options opened with the mouse, dropdown, tab, cancel, Escape | ~6 s |
| `screen_mode.cfg` | `rs_screenmode` in the options: borderless locks the resolution, accept resets the device; covers the whole display for a few seconds | ~15 s |
| `resolution_aspect.cfg` | a resolution change between 16:9 and 4:3 in the options: the menu is rebuilt, the confirm box comes back, Yes keeps and No restores | ~12 s |
| `fps_limit.cfg` | `rs_fps_limit` in the options: the list, picking an entry; main menu fps at each limit | ~25 s |
| `ui_inventory.cfg` | in a level: inventory opened with the bound key, an item found by section, closed | ~17 s |
| `selftest_*.cfg` | the harness itself (crash and timeout paths) | ~3-25 s |

`run.ps1` uses the game folder `engine_tests_infernis` next to `sources` (`-GameDir` to
change it) and `bin\x64` (`-Bin`). `-Renderer r2|r3|r4` runs with another renderer.

Exit codes: 0 passed, 1 failed, 2 bad scenario, 3 crashed, 4 timeout or hang.
`run.ps1` prints a summary. The full result is `userdata\logs\autotest_result.json` in
the game folder: `status`, `reason`, `step_line`/`step_text` (where it stopped), `perf`,
`screenshots`, `ui_dumps`, `dump`, `log` (the full engine log), `log_error_samples`,
`log_tail`.

- **Screenshots:** read the PNGs (they are in the result) whenever the change can affect
  what is drawn, and say what you see.
- **Frame times:** they vary from run to run. Compare against a run of the same scenario
  on the build before the change, not against a remembered number.
- **Log errors:** a plain start already logs about 8 `!` lines. Look for new ones.
- **Crashes (exit code 3):** the reason and the minidump path are in the result; the
  stack trace is in the engine log.

## Writing a scenario

Add one under `tests/autotest/` when the existing scenarios don't exercise the change.

- A new game: `start server(all/single/alife/new) client(localhost)` then `wait_level`.
  Never pass `-start` on the command line: the game crashes while the level starts.
- A save: `load_save <name>` then `wait_level`. Saves used by tests are named
  `autotest_*`. Never use or change the user's own saves.
- Console commands work as steps. The runner checks that every command exists before it
  starts.
- `wait_log <s> <text>` / `expect_log` / `fail_on_log` turn log output into checks.
- `perf_begin <label>` ... `perf_end` around the part to measure.
- End with `quit`.

## Driving the UI

The `ui_*` steps click, hover, drag and press keys in menus and in-game windows through a
virtual cursor; the real mouse pointer is never moved. Real clicks and keys still reach
the game while its window has the focus, so leave the window alone during a run.

1. **Look first.** Put `ui_dump <label>` where the window is open and run it.
   `userdata\logs\autotest_ui_<label>.txt` lists every visible window: `#n`, class, name,
   rect, state, `option=`, `item=`, `st=`, `text=`. It is cp1251:
   `iconv -f cp1251 -t utf-8 <file>`. Take a screenshot too.
2. **Pick stable targets**, best first: a window name (`btn_options`), an options control
   by its console command (`option:vid_mode`), an inventory item by section
   (`item:wpn_ak74u`), a text by its string table id (`st:ui_mm_video`), a class for
   unnamed containers (`class:CUIActorMenu`), a path through unnamed windows (`tab/#2`),
   and only then `x,y` in 1024x768 UI coordinates. Avoid `text:` for localized text.
3. **Check the effect** with `ui_wait <s> <target>`, `ui_expect`, `ui_expect_no`, and a
   second `ui_dump`.

- Open game windows with bound actions: `ui_key inventory`, `ui_key kESCAPE`.
- Main menu buttons select on hover and animate: `ui_move btn_x`, `wait_seconds 1`, then
  `ui_click btn_x` (see `ui_options.cfg`).

## Rules

- One game at a time. It locks the binaries, so don't build while it runs.
- The game runs windowed with its own settings (`userdata\autotest.ltx`), keeps running
  without the focus and never shows error dialogs. It still uses the GPU and opens a
  window, so tell the user before long runs.
- A passing run doesn't replace the user's own testing. Report what the scenario covered
  and what it didn't.
