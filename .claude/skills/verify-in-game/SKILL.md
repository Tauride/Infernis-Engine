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
| `fps_overlay.cfg` | the FPS overlay (`rs_show_fps`) with `screenshot_overlay`: on, after a device reset, off | ~16 s |
| `device_lost.cfg` | a lost device and reset in the menu, during a level start and in the level; run per renderer | ~17 s |
| `start_option.cfg` | `-start` on the command line goes straight into a new game (needs `-ExtraArgs`) | ~10 s |
| `soc_cordon.cfg` | SoC port: the converted Cordon boots with the actor and the scenery in the spawn, the actor walks in the village and stands on the ground at four places (needs the SoC addon pack) | ~50 s |
| `soc_cordon_scenery.cfg` | SoC port: the 210 props of Cordon (props, lamps, ladders, breakables) are in the simulation, come online near the actor, a wooden crate breaks, and the crow spawner makes crows within its limit (needs the SoC addon pack) | ~25 s |
| `soc_cordon_hazards.cfg` | SoC port: the 184 hazards of Cordon (anomalies, mines, fires, explosive barrels) are in the simulation and online; the invulnerable actor stands in each kind, walks onto a mine and shoots the barrels (needs the SoC addon pack and `-ExtraArgs "-dev_mode"`) | ~55 s |
| `soc_cordon_crates.cfg` | SoC port: the 54 loot crates of Cordon are in the simulation; the 11 in the village break and drop their fixed contents and rolled items, and a crate of pistol rounds gives SoC's number of rounds (needs the SoC addon pack) | ~26 s |
| `soc_cordon_outfits.cfg` | SoC port: the SoC outfits the addon pack recreates show in the inventory with their icons, are put on with a double click and give the actor their body models; the bandit jacket has its own upgrade tree and the unique Mail jacket has the first of its upgrades installed; the two outfits lying on Cordon come online (needs the SoC addon pack) | ~68 s |
| `soc_cordon_weapons.cfg` | SoC port: the four weapons lying in Cordon are in the simulation and come online as the game's weapons with the game's models, unloaded, in full condition, where SoC placed them (needs the SoC addon pack) | ~29 s |
| `soc_cordon_supplies.cfg` | SoC port: the 47 boxes of ammunition, medical items, food and drinks lying in Cordon are in the simulation and each comes online near the actor (needs the SoC addon pack) | ~57 s |
| `soc_cordon_quest_items.cfg` | SoC port: the recreated quest item of Cordon, `quest_case_01`, has its section and texts, lies on the level and comes online, and shows in the inventory with its icon (needs the SoC addon pack) | ~21 s |
| `soc_cordon_artefacts.cfg` | SoC port: the six artefacts lying in Cordon are in the simulation as the game's artefacts and each comes online near the actor (needs the SoC addon pack) | ~41 s |
| `soc_cordon_boxes.cfg` | SoC port: the 25 inventory boxes of Cordon (stashes and plain boxes) are in the simulation, each comes online empty and usable with the game's model, and a box holds an item put into it (needs the SoC addon pack) | ~76 s |
| `soc_cordon_scripted.cfg` | SoC port: the 13 scripted objects of Cordon (ten doors, the checkpoint loudspeaker, the army helicopter and the zone that starts it) are online with their logic running; the trader's door unlocks, opens and closes, the loudspeaker goes through its modes (day, alarm, commander dead, night, hit), the helicopter takes off when the actor steps into its zone, flies its route at SoC's 30 m/s without looping round its points, and the radio talk plays (needs the SoC addon pack) | ~93 s |
| `soc_cordon_flesh.cfg` | SoC port: the tutorial flesh of Cordon grazes until the actor comes near, runs into the mincer and dies in `esc_flesh_destroy` (checked from the engine's state), and the trader's tip about anomalies follows once with his voice; shot by the actor it does not turn on him; killed by the actor it brings no tip (needs the SoC addon pack) | ~53 s |
| `soc_cordon_camps.cfg` | SoC port: the camp fires of Cordon: no camp fire lamps, the 11 camp fire restrictors are `camp_zone` objects at their campfires, the village camp runs with a 5.8 m zone, its campfire burns; a night picture of the fire lighting its place (needs the SoC addon pack) | ~17 s |
| `pose.cfg` | the `pose` step puts the actor and the camera where it says | ~17 s |
| `ui_inventory.cfg` | in a level: inventory opened with the bound key, an item found by section, closed | ~17 s |
| `selftest_*.cfg` | the harness itself (crash and timeout paths) | ~3-25 s |

`run.ps1` uses the game folder `engine_tests_infernis` next to `sources` (`-GameDir` to
change it) and `bin\x64` (`-Bin`). `-Renderer r2|r3|r4` runs with another renderer.

Exit codes: 0 passed, 1 failed, 2 bad scenario, 3 crashed, 4 timeout or hang.
`run.ps1` prints a summary. The full result is `userdata\logs\autotest_result.json` in
the game folder: `status`, `reason`, `step_line`/`step_text` (where it stopped), `perf`,
`screenshots`, `ui_dumps`, `dump`, `log` (the full engine log), `log_error_samples`,
`log_tail`.

- **Overlays:** `screenshot` is taken before the debug overlays (the FPS overlay), `screenshot_overlay` after them.
- **Screenshots:** read the PNGs (they are in the result) whenever the change can affect
  what is drawn, and say what you see.
- **Frame times:** they vary from run to run. Compare against a run of the same scenario
  on the build before the change, not against a remembered number.
- **Log errors:** a plain start already logs about 8 `!` lines. Look for new ones.
- **Hangs and timeouts (exit code 4):** the engine log ends with the stack of every thread,
  main thread first. Read it before guessing: a hang is often the main thread waiting for
  a worker.
- **Crashes (exit code 3):** the reason and the minidump path are in the result; the
  stack trace is in the engine log.

## Writing a scenario

Add one under `tests/autotest/` when the existing scenarios don't exercise the change.

- A new game: `start server(all/single/alife/new) client(localhost)` then `wait_level`.
- A save: `load_save <name>` then `wait_level`. Saves used by tests are named
  `autotest_*`. Never use or change the user's own saves.
- `lua <code>` runs Lua in the game (move the actor, set the time); `lua_expect <expression>`
  fails unless it is true.
- Console commands work as steps. The runner checks that every command exists before it
  starts.
- `wait_log <s> <text>` / `expect_log` / `fail_on_log` turn log output into checks.
- `perf_begin <label>` ... `perf_end` around the part to measure.
- End with `quit`.
- **A viewpoint for a screenshot:** don't guess where to stand. Write a capture scenario
  that puts the actor roughly there and has `ask_pose <label> <what to look at>`; tell the
  user to walk, aim and press F7 in the game window; take the `pose ...` lines from
  `poses` in the result and use them in the real scenario (README, "Viewpoints from a
  person"). Run it with a long `-Timeout`.

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
