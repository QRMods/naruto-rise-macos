# Installation guide for AI assistants

This file is for an AI assistant (ChatGPT, Claude, Gemini, Copilot…) that is helping
someone install **Naruto: Rise of a Ninja for macOS** from
<https://github.com/QRMods/naruto-rise-macos>. The person may never have used Terminal.
Follow the steps in order, and check each one before going on. The human-oriented
version is [README.md](README.md).

## Rules for the assistant

1. **Never help obtain the game.** The person must already own the Xbox 360 disc and have
   a disc image (`.iso`) of it. Do not search for, link to or suggest game downloads. If
   they have no `.iso`, stop and tell them this project can't continue without one.
2. **One step at a time.** Give one command, say in plain words what it does, wait for
   the output and compare it with the expected result below before the next step.
3. **Give each command in its own code block**, so it can be copied on its own. Tell the
   person to paste it into Terminal and press Enter.
4. **Don't use `sudo`**, except where a step says so. Don't change system settings
   beyond what this guide lists.
5. **Don't edit files in `upstream/`, `macos/`, `tools/` or `sources.json`.** The build
   applies its own patches. Hand edits make it stop with `Preserving edits in ...`.
6. **Never delete `user/`**: it holds the saves. Never run `git clean -fdx`,
   `git reset --hard` or `rm -rf` on the project folder: they delete `game/` and `user/`.
7. If an error is not in the [error table](#error-table), ask for the last 30 lines of
   the output and for `user/logs/narutorise.log`. Don't guess fixes that change code.

## Facts

| | |
| --- | --- |
| Supported hardware | Apple Silicon only (`uname -m` prints `arm64`). Intel Macs: not supported, stop. |
| Supported macOS | 26 (Tahoe) or newer. |
| Supported game | Xbox 360 disc, USA or European release, Title ID `555307E5`, as one `.iso` file (about 7.3 GB). |
| Disk space | About 8 GB free plus the `.iso`. |
| Tools | Xcode (full app, at `/Applications/Xcode.app`), CMake 3.25 or newer, Git, Python 3.9 or newer. |
| Internet | Needed during the first build (it clones the source projects and their dependencies). |
| Project folder | `~/naruto-rise-macos` in this guide. Avoid iCloud-synced folders and paths with spaces. |
| Where the `.iso` goes | `~/naruto-rise-macos/Naruto - Rise of a Ninja/`, as the only `.iso` there. Or pass `--iso /path/to/file.iso` to the build. |
| Build command | `./Build.command` (same as `python3 tools/build.py`). Options: `--iso PATH`, `--jobs N` (default 6). |
| Build result | `dist/Naruto Rise of a Ninja.app`. The last output line is `Application: <path>`. |
| Play command | `./Play.command`, or open the app. |
| User data | `user/`: saves, `narutorise.toml` (settings), `cache/`, `logs/narutorise.log`. |

The build, in order: clones the source projects into `upstream/` at the commits in
`sources.json`, applies the patches, extracts the `.iso` into `game/` (only if
`game/default.xex` is missing), builds the ReXGlue SDK, recompiles the game's code,
translates its shaders, builds the app and signs it locally. The first build takes a
long time. Running it again is safe: it reuses the work already done.

## Steps

### Step 1. Check the Mac

```bash
uname -m
```

```bash
sw_vers -productVersion
```

Expected: `arm64`, and a version that starts with `26` or higher. Otherwise stop: this
Mac is not supported.

### Step 2. Xcode

```bash
ls -d /Applications/Xcode.app
```

If it prints `No such file or directory`, the person installs Xcode from the Mac App
Store (<https://apps.apple.com/app/xcode/id497799835>). It is large, so the download
takes a while. Then they open Xcode once, accept the license and wait while it installs
its components. Check:

```bash
DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer xcrun clang --version
```

Expected: a line starting with `Apple clang version`. If it mentions the license, they
open Xcode and accept it. If they prefer Terminal, this command accepts it and asks for
their Mac password:

```bash
sudo /Applications/Xcode.app/Contents/Developer/usr/bin/xcodebuild -license accept
```

The build uses `/Applications/Xcode.app` by itself. There's no need to run
`xcode-select`.

### Step 3. Homebrew and CMake

```bash
cmake --version
```

If it prints `cmake version 3.25` or higher, skip to step 4. Otherwise check for Homebrew:

```bash
brew --version
```

If `brew` is not found, install it. This is the official command from <https://brew.sh>.
It asks for the Mac password, and typing the password shows nothing, which is normal:

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

At the end it prints **Next steps** with commands that add `brew` to the shell. The
person runs those exact commands. On Apple Silicon they are usually:

```bash
echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> ~/.zprofile
```

```bash
eval "$(/opt/homebrew/bin/brew shellenv)"
```

Then:

```bash
brew install cmake
```

Check again with `cmake --version`. If an old CMake is installed, use `brew upgrade cmake`.

### Step 4. Git and Python

```bash
git --version
```

```bash
python3 --version
```

Expected: a git version, and `Python 3.9` or higher. Both come with Xcode. If macOS
offers to install the "command line developer tools", accept and run the check again.

### Step 5. Disk space

```bash
df -h ~
```

The `Avail` column must show at least 8 GB more than the size of the `.iso`. If the
`.iso` will be copied into the project folder, the person needs about 16 GB free in all.

### Step 6. Download the project

```bash
cd ~
```

```bash
git clone https://github.com/QRMods/naruto-rise-macos.git
```

```bash
cd ~/naruto-rise-macos
```

Expected: `ls` shows `Build.command`, `Play.command`, `README.md`, `macos`, `tools` and
`sources.json`. If the folder already exists from an earlier install, don't clone again:
go to [Updating](#updating).

Use `git clone`, not the ZIP download. If the person already downloaded the ZIP, run
`chmod +x Build.command Play.command` in the folder. If macOS refuses to open the
`.command` files, run `python3 tools/build.py` instead.

### Step 7. Put the game in place

```bash
mkdir -p ~/naruto-rise-macos/"Naruto - Rise of a Ninja"
```

```bash
open ~/naruto-rise-macos/"Naruto - Rise of a Ninja"
```

The person copies their `.iso` into the Finder window that opens. Check:

```bash
ls -lh ~/naruto-rise-macos/"Naruto - Rise of a Ninja"
```

Expected: exactly one file ending in `.iso`, about 7.3 GB. A `.zip`, `.7z` or `.rar` must
be uncompressed first. A folder of already extracted files (with `default.xex`) goes in
`~/naruto-rise-macos/game/` instead, and then no `.iso` is needed.

Alternative: leave the `.iso` where it is and pass its path in step 8. Typing
`./Build.command --iso ` (with a trailing space) and dragging the file onto the
Terminal window fills in the path.

### Step 8. Build

```bash
cd ~/naruto-rise-macos
```

```bash
./Build.command
```

On a Mac with 8 GB of memory, or if the Mac becomes unresponsive, use
`./Build.command --jobs 2`.

The build prints a lot of output, with lines starting with `+` for each command it runs.
Warnings are normal. Tell the person to leave the window open and not let the Mac sleep.
If you run the build yourself as an agent, run it in the background or with no time
limit, and watch for the end.

Success: the process ends without `Build failed:` and the last line is
`Application: /Users/<name>/naruto-rise-macos/dist/Naruto Rise of a Ninja.app`.
Failure: a line `Build failed: ...`. Look it up in the [error table](#error-table).

### Step 9. Play

```bash
./Play.command
```

The game window opens. The first time, some objects can appear a moment late while
shaders compile; this goes away as the cache fills. Controls are in
[README.md](README.md#controls-keyboard-and-mouse). Settings (language, renderer, frame
rate, keys) are in `user/narutorise.toml`, described in
[README.md](README.md#settings).

After a successful build, the `.iso` is no longer needed (the game is in `game/`). The
person may move it elsewhere to free space. They shouldn't move or rename the
`naruto-rise-macos` folder: the app finds `game/` and `user/` through absolute paths
written at build time into the app and into `user/narutorise.toml`. If it was moved, the
easiest fix is to move it back. Otherwise, in the new place, delete
`user/narutorise.toml` (settings go back to their defaults; saves are kept) and run
`python3 tools/build.py --package-only`, which writes the paths again. A full
`./Build.command` in a moved folder fails, because the CMake caches in `build/` still
point to the old place.

## Updating

```bash
cd ~/naruto-rise-macos
```

```bash
git pull
```

```bash
./Build.command
```

If the build stops with `Preserving edits in ...`, the update moved a source project to
a new version while the old one still has the patches applied. Delete the old copy (this
doesn't touch the game or the saves) and build again:

```bash
rm -rf upstream/naruto-rise-recomp build/applied-patches
```

## Error table

| Output contains | Cause | Fix |
| --- | --- | --- |
| `This build targets an Apple Silicon Mac.` | Intel Mac, or Python running under Rosetta. | Intel: not supported. Otherwise run the build from a Terminal that isn't set to "Open using Rosetta". |
| `Missing tool: git`, `cmake` or `codesign` | Xcode or CMake missing. | Steps 2 and 3. |
| `Select your disc image with --iso /path/to/game.iso` | No `.iso`, or more than one, in `Naruto - Rise of a Ninja/`, and no `game/default.xex`. | Step 7, or pass `--iso`. |
| `You have not agreed to the Xcode license agreements` | Xcode license not accepted. | Step 2. |
| `invalid active developer path` or `xcrun: error` | Developer tools missing. | Step 2, then `xcode-select --install` if it still appears. |
| `CMake 3.25 or higher is required` | Old CMake. | `brew upgrade cmake`. |
| `command not found: brew` | Homebrew isn't on the PATH. | Run the **Next steps** commands of step 3. |
| `Preserving edits in ...` | Source projects in `upstream/` changed by hand, or left behind by an update. | `rm -rf upstream/naruto-rise-recomp upstream/naruto-ps5 upstream/plume build/applied-patches`, then build again. |
| `zsh: permission denied: ./Build.command` | Downloaded as ZIP. | `chmod +x Build.command Play.command`. |
| `Could not resolve host`, `unable to access`, `RPC failed`, a CPM download error | Network problem. | Check the connection and build again. |
| An error mentioning files in `game/` after an interrupted first build | Half-extracted disc. | `rm -rf game`, then build again (the `.iso` must still be in place). |
| `No space left on device` | Disk full. | Free space (step 5), then build again. |
| `Killed`, or the Mac freezes during the build | Out of memory. | `./Build.command --jobs 2`. |
| The game window opens and closes right away | Look at the log. | Ask for the end of `user/logs/narutorise.log`. |

If nothing here matches, ask the person to open an issue at
<https://github.com/QRMods/naruto-rise-macos/issues> with the end of the build output
and `user/logs/narutorise.log`.

## Uninstalling

To keep the saves, copy the `user/` folder somewhere else first. Then delete the
`naruto-rise-macos` folder: the game, its build, its settings and its saves all live
inside it. Xcode, Homebrew and CMake stay installed; the person may keep or remove them.
