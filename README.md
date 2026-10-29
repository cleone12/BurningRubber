# BurningRubber

This is the original source code for the mac shareware game "Burning Rubber", originally released in 1995.

The source code is in Pascal, Race.π is a CodeWarrior project file which was used to build it. To be able to upload this to git, Line endings have been converted to Unix style, and the resource forks of the rsrc files have been moved to the data fork. You may need to revert these changes before being able to build it on a classic Macintosh.

---

## SDL2 port

This fork adds a native SDL2 port that runs on macOS, Linux and Windows using the original, unmodified game data (`Race.rsrc`, `Levels.rsrc`). It follows the approach of the [Reckless Drivin' SDL2 port](https://github.com/DarrCoh/reckless-drivin-sdl) of the sequel. The game logic is kept as it was, and the Classic Mac OS APIs underneath it are replaced.

The original Pascal sources in the repository root are untouched. The port lives in `src/`.

### Downloading a build

Every push builds the game on GitHub Actions. Open the repository's **Actions** tab, pick the latest green **Build** run, and download from the **Artifacts** list at the bottom (you need to be signed in to GitHub):

- **BurningRubber-mac-apple-silicon** contains `BurningRubber-mac.dmg`. Open it and drag *Burning Rubber* to Applications. The app is not notarized by Apple, so the first time, right-click it and choose **Open**, then **Open** again. If macOS says it is damaged, run `xattr -cr "/Applications/Burning Rubber.app"` in Terminal.
- **BurningRubber-windows-x64** contains a `BurningRubber` folder. Unzip it and run `BurningRubber.exe`. Windows SmartScreen may warn about an unrecognised app: choose **More info**, then **Run anyway**.

Artifacts are kept for 90 days.

### Building

You need CMake 3.16+, a C11 compiler, SDL2 and SDL2_ttf.

```bash
# Debian/Ubuntu
sudo apt install cmake libsdl2-dev libsdl2-ttf-dev
# macOS
brew install cmake sdl2 sdl2_ttf pkg-config

cmake -B build
cmake --build build
./build/BurningRubber          # macOS: open build/BurningRubber.app
```

On Windows, install the libraries with vcpkg (`vcpkg install sdl2 sdl2-ttf`) and configure with `-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake`.

The build copies `Race.rsrc`, `Levels.rsrc` and `fonts/ChicagoFLF.ttf` next to the executable (or into the app bundle).

The app icon is `icons/BurningRubber.png`. After changing it, run `python3 icons/make_icons.py` (needs Pillow) to regenerate the macOS `.icns`, the Windows `.ico` and the window icon in `src/icon.h`.

### Playing

Everything works as it did in 1995: the title screen, the menu bar, the Preferences, Controls, High Scores and About dialogs, all ten levels (and beyond: after level 10 the levels repeat with less time), the police, the bonus multipliers and the high-score table.

| | |
|---|---|
| Steer, accelerate, brake | Arrow keys (rebind them under Options › Controls…) |
| Pause | ⌘P / Ctrl+P (the menu bar appears while paused) |
| End game / Quit | ⌘E / ⌘Q (Ctrl on Windows and Linux) |
| Level select (the original cheat) | Option/Alt-click *Start*, or Option/Alt + File › New Game |
| Fullscreen | Alt+Enter |

The Command key is Ctrl on Windows and Linux. Settings, controls and high scores are saved to `Burning Rubber Prefs.txt` in the per-user preferences folder (for example `~/.local/share/Jonas Echterhoff/Burning Rubber/` on Linux).

### How the port works

| Original | Port |
|---|---|
| Pascal units (`Race.p`, `Game.p`, `Car.p`, ...) | Translated one-to-one to C in `src/` (`game.c`, `car.c`, ...), keeping Pascal's 16-bit `INTEGER` arithmetic |
| Resource Manager | `resources.c`: a resource fork parser over the two `.rsrc` files |
| Color QuickDraw at 8 bits per pixel | `qd.c`: indexed pixel buffers, scaled `CopyBits`/`CopyMask`, `ScrollRect`, PICT (PackBits, DirectBits, 1-bit, transparent mode), `cicn`, `ppat`, and the device colour table |
| QuickDraw text | `text.c`: the game's own bitmap `FONT`s (Digital, KidPix), plus ChicagoFLF standing in for the system font |
| Window, Menu, Dialog Managers, Alerts | `toolbox.c`: driven by the game's `WIND`, `MENU`, `mctb`, `DLOG`, `DITL`, `ALRT`, `wctb`, `dctb` and `actb` resources |
| Event Manager, `GetKeys`, `TickCount` | `platform.c`: SDL2 events and a Mac key-code map, presenting a 640×480 screen |
| Sound Manager | `sound.c`: a three-channel mixer for the `snd ` resources |
| `WriteResource` of `SETT`, `CTLS`, `HIGH` | `prefs.c`, with the same keep-or-revert behaviour |

Details the port reproduces from the original:

- **Palette effects:** the red flash on a crash and the fade to black when a race starts edit the device colour table, using the same signed 16-bit arithmetic.
- **Incremental drawing:** each frame the playfield is scrolled and only the changed rectangles are repainted, as in `RoadDraw.p`.
- **Sprite transparency:** sprites are masked the way the original built its masks, by plotting each icon into a 1-bit port, so white pixels are transparent.
- **Frame rate:** the game runs at the original's fixed 12 frames per second (`SETT`'s speed governor of 5 ticks per frame).
- **Random numbers:** they come from a reimplementation of QuickDraw's `Random()`.

### Differences from the original

- The default keys are the arrow keys. The shipped `CTLS` resource used keypad 8/5/4/6, which most keyboards no longer have. Keys can be rebound exactly as before.
- Chicago is not part of the game files, so the menus, dialogs and the large in-game messages use [ChicagoFLF](fonts/ChicagoFLF-LICENSE.txt), Robin Casady's public-domain revival.
- Engine pitch: the engine sound's `rateCmd` value is treated as a playback rate in Hz. Its range of roughly 1000 to 9000 Hz suits the 1890 Hz engine sample.
- A few out-of-bounds reads in the original are bounded. One is spawning objects past the end of a level's object list; another is a 16th entry in the 15-slot redraw list.
- The race window is centred on a 640×480 screen. With *Blank Screen* turned off, the screen grows taller so that the titled window fits under the menu bar.
- Desk accessories, disk-insert events and the start-up hardware checks are not emulated. The *Unlimited Speed* checkbox did nothing in the original and still does nothing.

The level editor in `LEdit/` is not ported.

### Automated testing

When `BR_TEST_SCRIPT` names a file, the game reads it as a timeline of input events and screenshots (see the comment in `src/platform.c`). CI uses this to run `tests/smoke.txt` headlessly with `SDL_VIDEODRIVER=dummy`.

### Credits

- Original game and data: Jonas Echterhoff (1995–96), MIT License.
- Porting approach: [Reckless Drivin' SDL2 port](https://github.com/DarrCoh/reckless-drivin-sdl) by Darren Cohen.
- ChicagoFLF font: Robin Casady, public domain.
- Port built with [Claude Code](https://claude.ai/code).
