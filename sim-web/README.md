# Browser UI simulator

Runs the grinder's real LVGL screens, compiled to WebAssembly, in any browser, so
UI changes can be checked without beans or an OTA update.

Based on the browser simulator from [ceear's fork](https://github.com/ceear/smart-grind-by-weight)
(`sim-web/`, GPL-3.0 like this project), adapted to this fork's screens, nav bar and
AUTO mode.

## Run

```bash
python3 tools/grinder.py sim
```

Builds (first run takes a minute to fetch LVGL v9.5.0), serves
http://localhost:8080/smart-grind-web-sim.html and opens it. `--no-serve` builds
only, `--port N` changes the port, `--no-open` skips opening a tab.

One-time setup: the Emscripten SDK in `~/emsdk` (or wherever `$EMSDK` points):

```bash
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
~/emsdk/emsdk install latest
~/emsdk/emsdk activate latest
```

## What is real and what is simulated

Real, unmodified source from `src/`:
- Screens: home tabs (AUTO, Single, Double, Custom, Menu, Scale), grind arc and
  chart, Settings (every page), confirm, calibration, auto-tune, Learn Portafilters.
- The global nav bar (`status_indicator_controller.cpp`).
- The portafilter detector, profile controller and grind-mode helpers.
- Theme tokens, fonts and `include/lv_conf.h` (with the browser overrides in
  `sim-web/lv_conf.h`).

Simulated in `main.cpp`:
- The layer above the screens: which screen shows for each state, the nav bar's
  title and back arrow, and the event handlers. These mirror `UIManager` and the
  UI controllers but are not compiled from them.
- Hardware: grinds follow a fixed flow curve, the Scale tab adds weight while GRIND
  is held, and portafilter placement comes from the panel beside the screen.
- Preferences live in memory (`platform/preferences_idf.h`): toggles, the grind
  mode and learned portafilters persist until the page is reloaded.

Settings tools that would drive hardware (motor test, factory reset, purge logs,
the end of calibration and auto-tune) stop at a yellow SIMULATOR notice.

## Controls

- Drag left or right on the screen to change home tabs.
- AUTO: use the panel to place a single, double, new or custom-weight portafilter;
  tap the ring to grind; Lift re-arms detection. Long-press AUTO for Learn
  Portafilters.
- Tap the arc or chart while grinding to switch layouts.
- The nav bar's back arrow leaves Settings and dialogs.

## When the build breaks after a UI change

Screens are compiled straight from `src/`, so new code reaches the simulator
automatically. A build error means a screen started using something the
simulator does not provide yet:

- `menu_screen.cpp`, `autotune_screen.cpp` and the nav bar are compiled from a
  shadow copy (`build/shadow-src/`) so their ESP32-only includes land on the
  stand-ins in `shadow-stubs/`. Add the missing method to the matching stand-in.
- A screen that started including a new ESP32 header needs a stand-in in
  `shadow-stubs/` or `platform/`, or its own shadow copy in `CMakeLists.txt`.
- A new event or screen state needs a handler in `main.cpp`.
