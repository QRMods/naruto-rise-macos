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
XENOS_THIRDPARTY = PC / 'tools/xenosrecomp/thirdparty'
BUILD = ROOT / 'build'
SHADERS = BUILD / 'native-shaders'
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


def native_shaders(jobs):
    """The PC port's native renderer shaders, translated and compiled at build time.

    The same chain as its shader-cache.cmake (XenosRecomp fork, DXC, spirv-val,
    smol-v and Zstandard), with the macOS DXC of the fork's dxc-bin.
    """
    tools = BUILD / 'shader-tools'
    tools.mkdir(parents=True, exist_ok=True)
    run('cmake', '-S', PC / 'tools/xenosrecomp', '-B', BUILD / 'xenosrecomp', '-DCMAKE_BUILD_TYPE=Release',
        '-DCMAKE_OSX_ARCHITECTURES=arm64', '-DXENOS_RECOMP_DXIL=OFF')
    run('cmake', '--build', BUILD / 'xenosrecomp', '--target', 'XenosRecomp', '--parallel', jobs)
    spirv = SDK / 'thirdparty'
    run('cmake', '-S', spirv / 'spirv-tools', '-B', BUILD / 'spirv-tools', '-DCMAKE_BUILD_TYPE=Release',
        '-DCMAKE_OSX_ARCHITECTURES=arm64', f'-DSPIRV-Headers_SOURCE_DIR={spirv / "spirv-headers"}',
        '-DSPIRV_SKIP_TESTS=ON', '-DSPIRV_WERROR=OFF')
    run('cmake', '--build', BUILD / 'spirv-tools', '--target', 'spirv-val', '--parallel', jobs)
    spirv_val = BUILD / 'spirv-tools/tools/spirv-val'
    zstd = BUILD / 'xenosrecomp/thirdparty/zstd/build/cmake/lib/libzstd.a'
    native = PC / 'tools/native-renderer'
    run('clang++', '-std=c++20', '-O2', f'-I{XENOS_THIRDPARTY / "zstd/lib"}', native / 'shader-compress.cpp', zstd,
        '-o', tools / 'shader-compress')
    run('clang++', '-std=c++20', '-O2', f'-I{XENOS_THIRDPARTY / "smol-v/source"}', native / 'smolv-encode.cpp',
        XENOS_THIRDPARTY / 'smol-v/source/smolv.cpp', '-o', tools / 'smolv-encode')
    dxc = tools / 'dxc'
    dxc.write_text('#!/bin/sh\n# DXC for macOS from XenosRecomp\'s dxc-bin, with its dylib beside it.\n'
                   f'DYLD_LIBRARY_PATH="{XENOS_THIRDPARTY / "dxc-bin/lib/arm64"}" '
                   f'exec "{XENOS_THIRDPARTY / "dxc-bin/bin/arm64/dxc-macos"}" "$@"\n')
    dxc.chmod(0o755)
    # The shader store keeps each translated and compiled shader across builds.
    run(sys.executable, '-I', native / 'build-shader-cache.py', ROOT / 'game/xeshader.sdb',
        BUILD / 'xenosrecomp/XenosRecomp/XenosRecomp', PC / 'tools/xenosrecomp/XenosRecomp/shader_common.h',
        dxc, SHADERS, BUILD / 'shader-store', '--compressor', tools / 'shader-compress',
        '--spirv-val', spirv_val, '--smolv-encode', tools / 'smolv-encode', '--passes', PC / 'app/src/gpu/backend')


def manifest():
    dest = BUILD / 'codegen'
    dest.mkdir(parents=True, exist_ok=True)
    quote = lambda p: json.dumps(str(p))
    text = '[project]\nname = "narutorise"\nsdk_version = "0.10.0"\ngame_root = ' + quote(ROOT / 'game') + '\n\n'
    for section, filename, output, seeds in (
        ('[entrypoint]', 'default.xex', 'default', 'analysis-seeds.toml'),
        ('[[modules]]', 'ai2c.dll', 'ai2c', 'ai2c-seeds.toml'),
    ):
        text += section + '\n'
        if section == '[[modules]]':
            text += 'guest_path = "ai2c.dll"\n'
        text += 'file_path = ' + quote(ROOT / 'game' / filename) + '\n'
        text += f'out_directory_path = "generated/{output}"\n'
        text += 'includes = [' + quote(PC / 'app' / seeds) + ']\n\n'
    target = dest / 'narutorise_manifest.toml'
    if not target.exists() or target.read_text() != text:
        target.write_text(text)
    return target


def package():
    binaries = BUILD / 'macos'
    contents = APP / 'Contents'
    macos = contents / 'MacOS'
    resources = contents / 'Resources'
    macos.mkdir(parents=True, exist_ok=True)
    resources.mkdir(parents=True, exist_ok=True)
    for name in ('narutorise', 'libnarutorise_ai2c.dylib', 'librexruntime.dylib', 'librexgpu-xenos.dylib'):
        shutil.copy2(binaries / name, macos / name)
    if (macos / 'vulkan').exists():
        shutil.rmtree(macos / 'vulkan')
    shutil.copytree(binaries / 'vulkan', resources / 'vulkan', dirs_exist_ok=True)
    config = (ROOT / 'macos/narutorise.toml').read_text()
    for key, path in (('game_data_root', ROOT / 'game'), ('user_data_root', ROOT / 'user'), ('cache_root', ROOT / 'user/cache')):
        config += '\n' + key + ' = ' + json.dumps(str(path)) + '\n'
    config += '\nlog_file = ' + json.dumps(str(ROOT / 'user/logs/narutorise.log')) + '\n'
    (resources / 'narutorise.toml').write_text(config)
    user_config = ROOT / 'user/narutorise.toml'
    user_config.parent.mkdir(exist_ok=True)
    (ROOT / 'user/logs').mkdir(exist_ok=True)
    if not user_config.exists():
        user_config.write_text(config)
    (macos / 'narutorise.toml').unlink(missing_ok=True)
    if (macos / 'logs').exists():
        shutil.copytree(macos / 'logs', ROOT / 'evidence/initial-app-logs', dirs_exist_ok=True)
        shutil.rmtree(macos / 'logs')
    info = {
        'CFBundleIdentifier': 'local.narutorise.native',
        'CFBundleName': 'Naruto Rise of a Ninja',
        'CFBundleDisplayName': 'Naruto Rise of a Ninja',
        'CFBundleExecutable': 'narutorise',
        'CFBundlePackageType': 'APPL',
        'CFBundleShortVersionString': '0.1.0',
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
        *(f'tools/xenosrecomp/thirdparty/{name}' for name in ('dxc-bin', 'fmt', 'smol-v', 'xxHash', 'zstd')))
    run('git', '-C', PLUME, 'submodule', 'update', '--init', '--depth', '1', '--jobs', args.jobs,
        'contrib/volk', 'contrib/Vulkan-Headers', 'contrib/VulkanMemoryAllocator')
    # The PC port's native renderer on macOS: its Vulkan surface, guest memory
    # reads without SEH, and the frame-rate line in the log.
    apply_patches(PC, [ROOT / 'macos/naruto-native-macos.patch'])
    # Same order as the PS5 port: the PC port's SDK patches, then the PS5 port's
    # (16 KB page write watches, Vulkan submission and descriptor work, frame
    # pacing, save pictures), then the macOS-specific ones.
    apply_patches(SDK, [*sorted((PC / 'patches/sdk').glob('*.patch')), PS5 / 'ps5/patches/rexglue-ps5.patch',
                        ROOT / 'macos/rexglue-macos.patch', ROOT / 'macos/rexglue-vulkan-presenter.patch',
                        ROOT / 'macos/rexglue-macos-window.patch'])
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
    run(BUILD / 'sdk-install/bin/rexglue', 'codegen', manifest())
    native_shaders(args.jobs)
    run('cmake', '-S', ROOT / 'macos', '-B', BUILD / 'macos', '-DCMAKE_BUILD_TYPE=Release',
        f'-DCMAKE_PREFIX_PATH={BUILD / "sdk-install"}', '-DNARUTORISE_NATIVE=ON')
    run('cmake', '--build', BUILD / 'macos', '--parallel', args.jobs)
    package()


if __name__ == '__main__':
    try:
        main()
    except (subprocess.CalledProcessError, RuntimeError, OSError) as error:
        print(f'Build failed: {error}', file=sys.stderr)
        sys.exit(1)
