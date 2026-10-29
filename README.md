# Naruto: Rise of a Ninja for macOS (Apple Silicon)

A native port of the Xbox 360 game to Macs with Apple chips. There is no emulator:
the game's PowerPC code is recompiled to ARM64 ahead of time, and the graphics run
through the PC port's native renderer, which takes the game's Direct3D calls and
draws them with Vulkan on top of Metal (MoltenVK).

> [!IMPORTANT]
> **You need your own copy of the game.** This project contains no game files.
> It builds the game on your Mac from a disc image (`.iso`) of your Xbox 360 disc.
> Neither the game nor anything generated from it is distributed here.

**Using an AI assistant (ChatGPT, Claude, Gemini…)?** Give it
[INSTALL.md](INSTALL.md). That guide is written for AI assistants: it has every
command, how to check that each step worked, and the fix for each known error.

## What you need

- A Mac with Apple Silicon (M1, M2, M3, M4 or newer). Intel Macs are not supported.
- macOS 26 (Tahoe) or newer.
- About 8 GB of free disk space, plus the space your `.iso` takes up (about 7.3 GB).
- An internet connection for the first build, which downloads the source code it needs.
- A disc image (`.iso`) of your own **Naruto: Rise of a Ninja** Xbox 360 disc, USA or
  European release (Title ID `555307E5`).

## Installation, step by step

Most steps use **Terminal**. To open it, press `Cmd + Space`, type `Terminal` and
press Enter. To run a command, paste it into the Terminal window and press Enter.

### 1. Install Xcode

1. Install **Xcode** from the [Mac App Store](https://apps.apple.com/app/xcode/id497799835).
   It is free, but large, so the download takes a while.
2. Open Xcode once. Accept the license and let it install its extra components.
   Then you can close it. You never need to use Xcode itself.

### 2. Install Homebrew and CMake

[Homebrew](https://brew.sh) installs developer tools. Paste this into Terminal (it asks
for your Mac password; typing it shows nothing on screen, which is normal):

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

When it finishes, it prints **Next steps** with two or three commands. Run them. They
make the `brew` command work. Then install CMake:

```bash
brew install cmake
```

### 3. Download this project

This creates a `naruto-rise-macos` folder in your home folder:

```bash
cd ~
```

```bash
git clone https://github.com/QRMods/naruto-rise-macos.git
```

If macOS asks you to install the "command line developer tools", accept and run the
command again. Don't put the project in iCloud Drive, or in a Desktop or Documents
folder that syncs with iCloud: the build creates thousands of files.

### 4. Add your game

Create the folder where the build looks for the disc image:

```bash
mkdir -p ~/naruto-rise-macos/"Naruto - Rise of a Ninja"
```

```bash
open ~/naruto-rise-macos
```

A Finder window opens. Copy your `.iso` into the `Naruto - Rise of a Ninja` folder.
It must be the only `.iso` in there.

### 5. Build the game

In that Finder window, double-click **Build.command**. A Terminal window opens and the
build starts. Or run it from Terminal:

```bash
cd ~/naruto-rise-macos
```

```bash
./Build.command
```

The first build takes a long time: it downloads the source code, extracts your disc,
recompiles the whole game and translates its shaders. Leave the window open until it
ends with a line like this:

```
Application: /Users/you/naruto-rise-macos/dist/Naruto Rise of a Ninja.app
```

If your `.iso` is somewhere else, give its location: type `./Build.command --iso `
(with a space at the end), drag the `.iso` file onto the Terminal window and press Enter.

After the first build, the game is extracted in `game/`. You can move the `.iso`
somewhere else if you need the space.

### 6. Play

Double-click **Play.command**, or open `dist/Naruto Rise of a Ninja.app`. To keep it
handy, drag the app from `dist/` to the Dock.

Keep the `naruto-rise-macos` folder where it is: the app finds the game and your saves
through it (if you already moved it, see [step 9 of INSTALL.md](INSTALL.md#step-9-play)).
Saves, settings, the cache and the logs are in its `user/` folder.

## Controls (keyboard and mouse)

| Controller | Keys |
| --- | --- |
| Left stick | W A S D |
| Camera (right stick) | Mouse, or numeric keypad 8 4 2 6 |
| A | Space |
| B | E or Backspace |
| X | J or left click |
| Y | K or right click |
| LB / RB | Q / R |
| LT / RT | Control / Shift |
| Left / right stick press | C / V or middle click |
| Start | Return or Esc |
| Back | Tab |
| D-pad | Arrow keys, or 1 2 3 4 |

A controller that works with SDL (Xbox, PlayStation and most others) works too. You can
change the keys in `user/narutorise.toml` (the `keybind_*` lines).

## Settings

Settings are in `user/narutorise.toml`, created by the first build. Edit it with any text
editor, such as TextEdit. Type straight quotes (`"`), not curly ones: in TextEdit, turn off
**Edit ▸ Substitutions ▸ Smart Quotes**. Changes apply the next time you start the game.

| Setting | What it does |
| --- | --- |
| `user_language` | Game language: `1` English (default), `3` German, `4` French, `5` Spanish, `6` Italian. The languages available depend on your disc. |
| `renderer` | `"native"` (default) or `"xenos"`, the emulated Xbox 360 GPU, which is slower. Try it if something looks wrong. |
| `frame_rate` | `30` (default) keeps the open world at 30 FPS like the console. Fights always run at 60. From `60` to `240`, extra frames are interpolated, but on an M4 the village only reaches about 40. |
| `msaa_samples` | Anti-aliasing: `0` is the game's own 4×; also `2`, `4` or `8`. |
| `internal_resolution_scale` | `1.5` renders at 1920×1080; `0` uses `resolution_scale`. |
| `fullscreen`, `vsync` | `true` or `false`. |
| `brightness` | `0.5` is the game's brightness. |

To see the FPS in the window title, start the game from Terminal, inside the project
folder, with
`REX_NATIVE_FPS_TITLE=1 "dist/Naruto Rise of a Ninja.app/Contents/MacOS/narutorise"`.

## Updating

```bash
cd ~/naruto-rise-macos
```

```bash
git pull
```

Then run `./Build.command` again. Your saves and settings in `user/` are kept.

If the build then stops with `Preserving edits in ...`, the update moved one of the
source projects to a new version. Delete the old copy and build again:

```bash
rm -rf upstream/naruto-rise-recomp build/applied-patches
```

## Troubleshooting

| Message | What to do |
| --- | --- |
| `This build targets an Apple Silicon Mac.` | Only Macs with Apple chips are supported. |
| `Missing tool: cmake` (or `git`) | Repeat step 2 (or install Xcode, step 1). |
| `Select your disc image with --iso /path/to/game.iso` | There is no `.iso`, or more than one, in `Naruto - Rise of a Ninja/`. Fix that, or use `--iso`. |
| `You have not agreed to the Xcode license agreements` | Open Xcode once and accept the license. |
| `CMake 3.25 or higher is required` | Run `brew upgrade cmake`. |
| `Preserving edits in ...` | See [Updating](#updating). |
| `permission denied: ./Build.command` | You downloaded a ZIP instead of using `git clone`. Run `chmod +x Build.command Play.command`. |
| A download fails | Check your internet connection and run the build again. |
| The build was interrupted while extracting the disc | Delete the `game` folder and build again. |
| The Mac gets very slow during the build | Build with fewer parallel jobs: `./Build.command --jobs 2`. |

To report a problem, open an [issue](https://github.com/QRMods/naruto-rise-macos/issues)
and attach `user/logs/narutorise.log`.

## Performance (MacBook with M4)

| Scene | Native renderer | Xenos (emulation) |
| --- | --- | --- |
| Konoha (open world) | Steady 30 FPS | 12-15 FPS |
| Fight | 59 FPS | 16-20 FPS |

### Checking for frame drops without playing

```bash
python3 tools/scenarios.py --cold
```

This plays through scenes on its own: Konoha, your saved game (with the input recorded
in `user/replays/user-route.nrinput`) and the Fight Mode stages one by one, with input
generated by the script. It measures each scene and reports a failure when a scene drops
below its target (30 FPS in the world, 60 in fights) or has a frame more than twice as
long as expected. It plays on a copy of `user/`, so your saves are not touched. `--cold`
starts without the pipeline cache, like the first time you play; `--only konoha
fight-stage-3` repeats specific scenes. To measure another route, record it while playing
with `REX_NATIVE_INPUT_RECORD=file.nrinput`.

## How it works

This repository doesn't copy the projects it builds on. `tools/build.py` clones them
into `upstream/` at the commits pinned in `sources.json`, applies the patches, extracts
the game into `game/`, generates the recompiled code, translates the game's shaders
(XenosRecomp, DXC, SPIR-V) and creates `dist/Naruto Rise of a Ninja.app`.

The macOS-specific code is in `macos/`:

- `main.cpp`, `platform.cpp`: the app's entry point and the lookup of the bundled Vulkan loader.
- `native/`: the native renderer's Win32 layer on POSIX, Mach and SDL3, the on-disk
  pipeline cache, and the guarded copy of the game's memory.
- `naruto-native-macos.patch`: the Metal surface, memory reads, pipeline skipping on
  Vulkan, and FPS and slow-frame (`[native-hitch]`) lines in the log.
- `rexglue-*.patch`: SDK changes (SDL window, Vulkan presenter, module loading).

### New shaders without stutter

Each new shader combination is translated to Metal and compiled when it first appears,
which costs tens to hundreds of milliseconds on a Mac. On macOS:

- An object whose pipeline isn't ready yet is skipped for that moment instead of
  freezing the frame. The pipeline compiles on other threads and the object appears as
  soon as it's done. The PC port's Vulkan backend never skipped: it compiled each new
  pipeline on the thread that records frames, and every fence wait of the game queued
  behind it (200 to 900 ms stutters when new content appeared). On macOS it skips like
  the D3D12 backend does. `REX_NATIVE_PSO_ASYNC=prefetch` brings back the PC port's wait.
- Compiled pipelines are saved in `user/cache/native-vulkan-pipelines.bin` and reused in
  later sessions.

### Reading the game's memory

For each draw, the renderer reads vertices, indices, constants and fences from the
game's memory, thousands of times per frame. On PC each read is a `memcpy` under
`__try`. On macOS it used to be a kernel call (`mach_vm_read_overwrite`), about 60 times
slower, so the cost grew with how much each scene drew. Now it's a `memcpy` with a signal
handler installed ahead of the SDK's (`macos/native/guest_fault_guard.cpp`): an address
with no memory makes the copy fail, as on PC, and the SDK's write watching still gets
its faults.

### Updating the source projects

To move a source project to a new version, change its commit in `sources.json` and build
again. The PC port and the SDK carry the patches applied, so first delete
`upstream/naruto-rise-recomp` and `build/applied-patches/`. If only the patches change,
building is enough: the applied ones are reverted and the new ones applied. If a patch no
longer applies, fix it on the code in `upstream/` and regenerate the file with `git diff`.

### Known issues

- The interpolated 60 FPS mode doesn't hold in the village on an M4 (about 40).

## Credits

- [naruto-rise-recomp](https://github.com/jorjbarbosa/naruto-rise-recomp) (PC port):
  the recompilation, the application, the native renderer and its shader pipeline.
- [naruto-ps5](https://github.com/mshivam019/naruto-ps5): its SDK patch (write watching
  with 16 KB pages, Vulkan submissions, presentation pacing, save thumbnails), the ISO
  extractor and the loading thread.
- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) and [Plume](https://github.com/renderbag/plume).

## License

macOS port by [alantero](https://github.com/alantero), published by
[QRMods](https://github.com/QRMods). Copyright (C) 2026 alantero.

The code in this repository is licensed under version 3 of the GNU General Public
License ([LICENSE](LICENSE), GPL-3.0-only), the same license as the PC port its patches
apply to. Each source project keeps its own license: naruto-ps5 and Plume (MIT) and the
ReXGlue SDK (BSD-3-Clause). The built app includes their notices in
`Contents/Resources/licenses/`.

This is an unofficial fan project, not affiliated with Ubisoft, Microsoft or the owners
of Naruto. Naruto: Rise of a Ninja, its content and its trademarks belong to their
respective owners.
