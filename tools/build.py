#!/usr/bin/env python3
"""Build the private ARM64 macOS port from the user's disc, entirely locally."""
import argparse
import json
import os
from pathlib import Path
import platform
import plistlib
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PC = ROOT / 'upstream/naruto-rise-recomp'
PS5 = ROOT / 'upstream/naruto-ps5'
SDK = PC / 'sdk'
PLUME = ROOT / 'upstream/plume'
BUILD = ROOT / 'build'
APP = ROOT / 'dist/Naruto Rise of a Ninja.app'
ENV = dict(os.environ)
if Path('/Applications/Xcode.app/Contents/Developer').is_dir():
    ENV['DEVELOPER_DIR'] = '/Applications/Xcode.app/Contents/Developer'
ENV['CPM_SOURCE_CACHE'] = str(BUILD / 'dependencies')


def run(*args, cwd=ROOT):
    print('+', ' '.join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), cwd=cwd, env=ENV, check=True)


def checkout(path, source):
    if not (path / '.git').exists():
        path.parent.mkdir(parents=True, exist_ok=True)
        run('git', 'clone', '--depth', '1', source['url'], path)
    current = subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True).strip()
    if current != source['commit']:
        dirty = subprocess.check_output(['git', '-C', str(path), 'status', '--porcelain'], text=True)
        if dirty:
            raise RuntimeError(f'Preserving edits in {path}; expected revision {source["commit"]}')
        run('git', '-C', path, 'fetch', '--depth', '1', 'origin', source['commit'])
        run('git', '-C', path, 'checkout', '--detach', source['commit'])


def apply_patches(repo, patches):
    """Applies a stack of patches once; a changed stack replaces the applied one.

    The applied stack is recorded under build/applied-patches, so patches that
    touch the same lines (the PS5 and macOS ones) need no per-patch detection.
    """
    record = BUILD / 'applied-patches' / repo.relative_to(ROOT).as_posix().replace('/', '_')
    applied = sorted(record.glob('*.patch')) if record.is_dir() else []
    wanted = [path.read_bytes() for path in patches]
    if [path.read_bytes() for path in applied] == wanted:
        return
    if not applied and subprocess.check_output(['git', '-C', str(repo), 'status', '--porcelain',
                                                '--untracked-files=no'], text=True).strip():
        raise RuntimeError(f'Preserving edits in {repo}: it has changes that no recorded patch explains')
    for path in reversed(applied):
        run('git', '-C', repo, 'apply', '--reverse', path)
    if record.is_dir():
        shutil.rmtree(record)
    record.mkdir(parents=True)
    for index, path in enumerate(patches):
        run('git', '-C', repo, 'apply', path)
        shutil.copy2(path, record / f'{index:02d}-{path.name}')


def spirv_val(jobs):
    """spirv-val of the SDK's SPIRV-Tools, which the shader cache of the native renderer checks with."""
    spirv = SDK / 'thirdparty'
    run('cmake', '-S', spirv / 'spirv-tools', '-B', BUILD / 'spirv-tools', '-DCMAKE_BUILD_TYPE=Release',
        '-DCMAKE_OSX_ARCHITECTURES=arm64', f'-DSPIRV-Headers_SOURCE_DIR={spirv / "spirv-headers"}',
        '-DSPIRV_SKIP_TESTS=ON', '-DSPIRV_WERROR=OFF')
    run('cmake', '--build', BUILD / 'spirv-tools', '--target', 'spirv-val', '--parallel', jobs)
    return BUILD / 'spirv-tools/tools/spirv-val'


def manifest():
    """The PC port's codegen manifest for the local game files.

    A copy beside the PC port's (the codegen writes the module targets relative
    to the app folder), excluded from that checkout's Git status, whose game
    paths point at game/. A module whose file the disc lacks (the character DLC
    revisions, ai2c1-3.dll) is left out: the base game runs without them.
    """
    app = PC / 'app'
    quote = lambda path: json.dumps(str(path))
    value = lambda line: json.loads(line.split('=', 1)[1])
    # The manifest's sections, each from its header to the next one.
    sections = [[]]
    for line in (app / 'narutorise_manifest.toml').read_text().splitlines():
        if line.startswith('['):
            sections.append([])
        sections[-1].append(line)
    lines = []
    for section in sections:
        files = [value(line) for line in section if line.split('=', 1)[0].strip() == 'file_path']
        if section and section[0].strip() == '[[modules]]' and not (ROOT / 'game' / Path(files[0]).name).is_file():
            continue
        for line in section:
            key = line.split('=', 1)[0].strip()
            if key == 'game_root':
                line = 'game_root = ' + quote(ROOT / 'game')
            elif key == 'file_path':
                line = 'file_path = ' + quote(ROOT / 'game' / Path(value(line)).name)
            lines.append(line)
    text = '\n'.join(lines) + '\n'
    target = app / 'narutorise_manifest.macos.toml'
    exclude = PC / '.git/info/exclude'
    exclude.parent.mkdir(parents=True, exist_ok=True)
    if target.name not in (exclude.read_text() if exclude.exists() else ''):
        with exclude.open('a') as stream:
            stream.write(f'/app/{target.name}\n')
    if not target.exists() or target.read_text() != text:
        target.write_text(text)
    return target


def settings_text(base, overrides):
    """The settings of `base` with each `key = value` line of `overrides` in place of its own."""
    lines = base.splitlines()
    keys = {line.split('=', 1)[0].strip(): index for index, line in enumerate(lines)
            if '=' in line and not line.lstrip().startswith('#')}
    for line in overrides.splitlines():
        if '=' not in line or line.lstrip().startswith('#'):
            continue
        key = line.split('=', 1)[0].strip()
        if key in keys:
            lines[keys[key]] = line
        else:
            lines.append(line)
    return '\n'.join(lines) + '\n'


def system_language():
    """The Mac's first preferred language as the settings name it.

    The menus' language (launcher_language) and the game's (user_language, an
    Xbox 360 language code: 1 English, 3 German, 4 French, 5 Spanish,
    6 Italian); English where the game or the menus lack it.
    """
    try:
        listed = subprocess.check_output(['defaults', 'read', '-g', 'AppleLanguages'], text=True)
        first = listed.split('"')[1].lower()
    except (subprocess.CalledProcessError, OSError, IndexError):
        first = 'en'
    prefix = first.split('-')[0]
    menus = {'es': 'es', 'fr': 'fr', 'de': 'de', 'it': 'it', 'ru': 'ru', 'pt': 'pt_BR'}.get(prefix, 'en')
    game = {'de': 3, 'fr': 4, 'es': 5, 'it': 6}.get(prefix, 1)
    return menus, game


def package():
    binaries = BUILD / 'app'
    contents = APP / 'Contents'
    macos = contents / 'MacOS'
    resources = contents / 'Resources'
    macos.mkdir(parents=True, exist_ok=True)
    resources.mkdir(parents=True, exist_ok=True)
    shutil.copy2(binaries / 'narutorise', macos / 'narutorise')
    for library in binaries.glob('*.dylib'):
        shutil.copy2(library, macos / library.name)
    shutil.copytree(binaries / 'vulkan', resources / 'vulkan', dirs_exist_ok=True)
    # The PC port's default settings with the macOS ones over them (keyboard and
    # mouse, a window, 30 FPS), which the game copies into a new data folder.
    menus, game = system_language()
    defaults = settings_text((PC / 'app/narutorise.toml').read_text(),
                             (ROOT / 'macos/narutorise.toml').read_text() +
                             f'\nlauncher_language = "{menus}"\nuser_language = {game}\n')
    shutil.copytree(binaries / 'defaults', resources / 'defaults', dirs_exist_ok=True)
    (resources / 'defaults/narutorise.toml').write_text(defaults)
    # The data folder is user/ of this checkout (settings, saves, logs and caches),
    # and the game is game/: a descriptor beside the executable selects the folder,
    # and the settings name the game and the folders the saves were always in.
    (resources / 'data_location.json').write_text(json.dumps({'version': 1, 'mode': 'custom', 'path': str(ROOT / 'user')}))
    # In an app bundle the game reads both from Resources (Contents/MacOS holds
    # code only); earlier packages kept them beside the executable.
    for name in ('defaults', 'data_location.json'):
        stale = macos / name
        if stale.is_symlink() or stale.is_file():
            stale.unlink()
        elif stale.is_dir():
            shutil.rmtree(stale)
    roots = {'user_data_root': ROOT / 'user', 'cache_root': ROOT / 'user/cache', 'custom_game_root': ROOT / 'game'}
    user_config = ROOT / 'user/narutorise.toml'
    (ROOT / 'user/logs').mkdir(parents=True, exist_ok=True)
    if not user_config.exists():
        user_config.write_text(defaults + ''.join(f'{key} = {json.dumps(str(path))}\n' for key, path in roots.items()))
    else:
        # Settings from an earlier version: the keys this one adds (the game's
        # folder, the menus' language, the macOS defaults it lacks) are added;
        # the others stay as they are.
        text = user_config.read_text()
        present = {line.split('=', 1)[0].strip() for line in text.splitlines()
                   if '=' in line and not line.lstrip().startswith('#')}
        wanted = [('custom_game_root', json.dumps(str(ROOT / 'game'))), ('launcher_language', json.dumps(menus))]
        wanted += [(line.split('=', 1)[0].strip(), line.split('=', 1)[1].strip())
                   for line in (ROOT / 'macos/narutorise.toml').read_text().splitlines()
                   if '=' in line and not line.lstrip().startswith('#')]
        added = ''.join(f'{key} = {value}\n' for key, value in wanted if key not in present)
        if added:
            with user_config.open('a') as stream:
                stream.write('\n' + added)
    if (macos / 'logs').exists():
        shutil.copytree(macos / 'logs', ROOT / 'evidence/initial-app-logs', dirs_exist_ok=True)
        shutil.rmtree(macos / 'logs')
    info = {
        'CFBundleIdentifier': 'local.narutorise.native',
        'CFBundleName': 'Naruto Rise of a Ninja',
        'CFBundleDisplayName': 'Naruto Rise of a Ninja',
        'CFBundleExecutable': 'narutorise',
        'CFBundlePackageType': 'APPL',
        'CFBundleShortVersionString': '0.4.0',
        'CFBundleVersion': '1',
        'NSHighResolutionCapable': True,
        'LSMinimumSystemVersion': '26.0',
    }
    with (contents / 'Info.plist').open('wb') as stream:
        plistlib.dump(info, stream)
    licenses = resources / 'licenses'
    licenses.mkdir(exist_ok=True)
    for source, name in ((PC / 'LICENSE', 'naruto-pc-GPL3.txt'), (PS5 / 'LICENSE', 'naruto-ps5-MIT.txt'), (SDK / 'LICENSE', 'rexglue-BSD.txt')):
        shutil.copy2(source, licenses / name)
    shutil.copytree(PC / 'packaging/licenses', licenses / 'third-party', dirs_exist_ok=True)
    shutil.copy2(ROOT / 'sources.json', resources / 'sources.json')
    for library in sorted(contents.rglob('*.dylib')):
        run('codesign', '--force', '--sign', '-', library)
    run('codesign', '--force', '--sign', '-', APP)
    print(f'Application: {APP}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--iso', type=Path)
    parser.add_argument('--jobs', type=int, default=6)
    parser.add_argument('--package-only', action='store_true')
    args = parser.parse_args()
    if platform.system() != 'Darwin' or platform.machine() != 'arm64':
        parser.error('This build targets an Apple Silicon Mac.')
    if args.package_only:
        package()
        return
    for tool in ('git', 'cmake', 'codesign'):
        if not shutil.which(tool):
            parser.error(f'Missing tool: {tool}')
    sources = json.loads((ROOT / 'sources.json').read_text())
    checkout(PC, sources['pc'])
    checkout(PS5, sources['ps5'])
    checkout(SDK, sources['sdk'])
    checkout(PLUME, sources['plume'])
    run('git', '-C', SDK, 'submodule', 'update', '--init', '--depth', '1', '--jobs', args.jobs)
    run('git', '-C', PC, 'submodule', 'update', '--init', '--depth', '1', '--jobs', args.jobs,
        *(f'tools/xenosrecomp/thirdparty/{name}' for name in ('dxc-bin', 'fmt', 'smol-v', 'xxHash', 'zstd')),
        'third_party/rmlui', 'third_party/freetype')
    run('git', '-C', PLUME, 'submodule', 'update', '--init', '--depth', '1', '--jobs', args.jobs,
        'contrib/volk', 'contrib/Vulkan-Headers', 'contrib/VulkanMemoryAllocator')
    # The PC port on macOS (macos/patches/pc, one commit each, written to go
    # upstream): the platform layer and build on Apple Silicon, and the skipped
    # draws while MoltenVK creates a pipeline.
    apply_patches(PC, sorted((ROOT / 'macos/patches/pc').glob('*.patch')))
    # The PC port's SDK patches, then macos/patches/sdk: the PS5 port's (16 KB
    # page write watches, Vulkan submission and descriptor work, frame pacing),
    # adapted to follow them, and the macOS-specific ones.
    apply_patches(SDK, [*sorted((PC / 'patches/sdk').glob('*.patch')),
                        *sorted((ROOT / 'macos/patches/sdk').glob('*.patch'))])
    if not (ROOT / 'game/default.xex').is_file():
        iso = args.iso
        if iso is None:
            images = list((ROOT / 'Naruto - Rise of a Ninja').glob('*.iso'))
            if len(images) != 1:
                parser.error('Select your disc image with --iso /path/to/game.iso')
            iso = images[0]
        run(sys.executable, PS5 / 'ps5/tools/extract-xiso.py', '--image', iso.resolve(), '--output', ROOT / 'game')
    run('cmake', '-S', SDK, '-B', BUILD / 'sdk', '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_OSX_ARCHITECTURES=arm64',
        f'-DCMAKE_INSTALL_PREFIX={BUILD / "sdk-install"}', f'-DCPM_SOURCE_CACHE={BUILD / "dependencies"}',
        '-DCMAKE_EXPORT_NO_PACKAGE_REGISTRY=ON', '-DREXGLUE_ENABLE_TRACY=OFF',
        '-DREXGLUE_ENABLE_FIDELITYFX=OFF', '-DREXGLUE_ENABLE_PERF_COUNTERS=ON')
    run('cmake', '--build', BUILD / 'sdk', '--target', 'install', '--parallel', args.jobs)
    # The recompiled code first: the app's CMake takes its sources when it configures.
    run(BUILD / 'sdk-install/bin/rexglue', 'codegen', manifest(), cwd=PC / 'app')
    # The PC port's build reruns the codegen with its own manifest (which names
    # the DLC modules) unless its stamp says the code is current: it is.
    (PC / 'app/generated/default/codegen.build.stamp').touch()
    validator = spirv_val(args.jobs)
    # The PC port's own CMake, as on Linux: the native renderer on Vulkan
    # (MoltenVK), its shaders translated from game/xeshader.sdb.
    run('cmake', '-S', PC / 'app', '-B', BUILD / 'app', '-DCMAKE_BUILD_TYPE=Release',
        '-DCMAKE_OSX_ARCHITECTURES=arm64', f'-DCMAKE_PREFIX_PATH={BUILD / "sdk-install"}',
        '-DNARUTORISE_NATIVE_RENDERER_BOOTSTRAP=ON', f'-DNATIVE_RENDERER_PLUME_SOURCE={PLUME}',
        f'-DNARUTORISE_GAME_DIR={ROOT / "game"}',
        f'-DNARUTORISE_SHADER_STORE={BUILD / "shader-store"}', f'-DNARUTORISE_SPIRV_VAL={validator}',
        # Sentry reports go to the PC port's own project: a macOS build sends none.
        '-DNARUTORISE_ENABLE_SENTRY=OFF')
    # The game and its recompiled guest modules (narutorise_ai2c, and a DLC's).
    modules = sorted(f'narutorise_{folder.name}' for folder in (PC / 'app/generated').iterdir()
                     if folder.name != 'default' and (folder / 'sources.cmake').is_file())
    run('cmake', '--build', BUILD / 'app', '--target', 'narutorise', *modules, '--parallel', args.jobs)
    package()


if __name__ == '__main__':
    try:
        main()
    except (subprocess.CalledProcessError, RuntimeError, OSError) as error:
        print(f'Build failed: {error}', file=sys.stderr)
        sys.exit(1)
