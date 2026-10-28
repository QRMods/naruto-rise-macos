#!/usr/bin/env python3
"""Plays scenes of the game unattended and reports the frame rate of each.

Each scene is driven by input the script generates (the fight stages, through
Fight Mode) or by a recording (user/replays), replayed by input_replay.cpp; no
one plays. The game runs on a copy of user/ (build/scenario-user), so the saves
are never touched. The native renderer logs its rate every half second
([native-fps]); a scene fails when its rate drops under the target or a frame
takes more than twice the target's frame time.

Usage: tools/scenarios.py [--only NAME ...] [--stages N] [--cold] [-- --cvar=value ...]
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / 'dist/Naruto Rise of a Ninja.app/Contents/MacOS/narutorise'
LOG = ROOT / 'user/logs/narutorise.log'
SANDBOX = ROOT / 'build/scenario-user'
DEVICE = 1296976640  # The keyboard and mouse device of the recordings.
BUTTONS = {'up': 0x1, 'down': 0x2, 'left': 0x4, 'right': 0x8, 'start': 0x10, 'back': 0x20,
           'a': 0x1000, 'b': 0x2000, 'x': 0x4000, 'y': 0x8000}
SAMPLE = re.compile(r'^\[[0-9-]+ ([0-9:.]+)\].*\[native-fps\] ([0-9.]+) fps ([0-9.]+) ms max ([0-9.]+)')
# The slow frames the renderer explains (d3d/frame.cpp NoteFrameTime): the
# frame's time and the slot that took the most of it.
HITCH = re.compile(r'\[native-hitch\] ([0-9]+) ms \(period [0-9]+ ms\): ([a-z_]+|nothing measured)')


def generated_replay(path, presses, end):
    """A replay of button presses: (seconds on the input clock, button), 300 ms each."""
    events = [(0.0, 0), (end, 0)]
    for at, button in presses:
        events += [(at, BUTTONS[button]), (at + 0.3, 0)]
    lines = ['NRINPUT 2', f'D {DEVICE} 1 Keyboard and Mouse']
    for packet, (at, buttons) in enumerate(sorted(events), start=1):
        lines.append(f'S {int(at * 1e6)} 0 {DEVICE} {packet} {buttons} 0 0 0 0 0 0')
    path.write_text('\n'.join(lines) + '\n')
    return path


def fight_replay(stage):
    """Fight Mode, versus, one player: the default characters on the stage-th stage.

    The title screen comes later on a first boot (no caches yet), so the first
    press waits for 22 s of input clock."""
    presses = [(22.0, 'start'), (25.0, 'down'), (29.0, 'a'), (36.5, 'down'), (37.6, 'a'), (44.0, 'a'),
               (53.0, 'a'), (55.7, 'a')]
    presses += [(59.0 + 1.0 * i, 'right') for i in range(stage)]
    confirm = 62.0 + 1.0 * stage
    presses.append((confirm, 'a'))
    return generated_replay(SANDBOX / f'fight-{stage}.nrinput', presses, confirm + 120), confirm


def scenes(stages):
    out = [
        # name, replay, seconds to run, seconds measured at the end, target frame rate
        ('konoha', ROOT / 'user/replays/bench-konoha.nrinput', 120, 25, 30),
        ('saved-game', ROOT / 'user/replays/user-route.nrinput', 150, 90, 30),
    ]
    for stage in range(stages):
        replay, confirm = fight_replay(stage)
        out.append((f'fight-stage-{stage}', replay, confirm + 60, 35, 60))
    return out


def sandbox(cold):
    """A copy of the saves (user/<profile ids>), with the pipeline cache kept unless cold."""
    SANDBOX.mkdir(parents=True, exist_ok=True)
    for entry in (ROOT / 'user').iterdir():
        if entry.is_dir() and entry.name not in ('logs', 'cache', 'replays'):
            shutil.rmtree(SANDBOX / entry.name, ignore_errors=True)
            shutil.copytree(entry, SANDBOX / entry.name)
    if cold:
        shutil.rmtree(SANDBOX / 'cache', ignore_errors=True)
    (SANDBOX / 'cache').mkdir(exist_ok=True)


def run(name, replay, seconds, tail, target, cvars):
    start = LOG.stat().st_size if LOG.exists() else 0
    env = dict(os.environ, REX_NATIVE_INPUT_REPLAY=str(replay), REX_NATIVE_FPS_TITLE='1')
    process = subprocess.Popen([BINARY, f'--user_data_root={SANDBOX}', f'--cache_root={SANDBOX / "cache"}',
                                *cvars], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        time.sleep(seconds)
    finally:
        process.terminate()
        try:
            process.wait(10)
        except subprocess.TimeoutExpired:
            process.kill()
    text = LOG.read_bytes()[start:].decode('utf-8', 'replace') if LOG.exists() else ''
    samples = [m.groups() for m in map(SAMPLE.match, text.splitlines()) if m]
    window = [(float(fps), float(peak)) for _, fps, _, peak in samples[-int(tail * 2):]]
    if not window:
        return dict(name=name, ok=False, note='no frames measured')
    fps = statistics.mean(f for f, _ in window)
    worst = max(p for _, p in window)
    budget = 2000.0 / target  # Twice the target's frame time.
    hitches = sum(p > budget for _, p in window)
    low = sum(f < target * 0.9 for f, _ in window)
    ok = fps >= target * 0.95 and hitches == 0
    causes = {}
    for line in text.splitlines():
        m = HITCH.search(line)
        if m:
            causes[m.group(2)] = causes.get(m.group(2), 0) + 1
    return dict(name=name, ok=ok, fps=fps, target=target, worst=worst, hitches=hitches, low=low,
                seconds=len(window) / 2, causes=causes)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--only', nargs='*', help='scene names (default: all)')
    parser.add_argument('--stages', type=int, default=8, help='fight stages to visit')
    parser.add_argument('--cold', action='store_true', help='start without the pipeline cache (as a first play)')
    parser.add_argument('cvars', nargs='*')
    args = parser.parse_args()
    subprocess.run(['pkill', '-9', '-x', 'narutorise'], capture_output=True)
    sandbox(args.cold)
    results = []
    for name, replay, seconds, tail, target in scenes(args.stages):
        if args.only and name not in args.only:
            continue
        result = run(name, replay, seconds, tail, target, args.cvars)
        results.append(result)
        if 'fps' in result:
            print(f"{'OK  ' if result['ok'] else 'FAIL'} {name:16} {result['fps']:5.1f}/{target} fps  "
                  f"worst {result['worst']:6.1f} ms  hitches {result['hitches']}  "
                  f"low half-seconds {result['low']}  ({result['seconds']:.0f} s)"
                  + (f"  slow frames by cause: {result['causes']}" if result['causes'] else ''), flush=True)
        else:
            print(f"FAIL {name:16} {result['note']}", flush=True)
    failed = [r['name'] for r in results if not r['ok']]
    print(f"\n{len(results) - len(failed)}/{len(results)} scenes at their frame rate"
          + (f"; below: {', '.join(failed)}" if failed else ''))
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
