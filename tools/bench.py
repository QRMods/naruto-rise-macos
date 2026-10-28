#!/usr/bin/env python3
"""Replay recorded input into the packaged app and report the guest frame rate.

Usage: tools/bench.py [--replay FILE] [--seconds N] [--tail N] [-- --cvar=value ...]
"""
import argparse
import csv
import os
from pathlib import Path
import statistics
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / 'dist/Naruto Rise of a Ninja.app/Contents/MacOS/narutorise'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--replay', type=Path, default=ROOT / 'user/replays/bench-konoha.nrinput')
    parser.add_argument('--seconds', type=float, default=115)
    parser.add_argument('--tail', type=float, default=25, help='seconds of guest frames to summarise')
    parser.add_argument('--name', default='bench')
    parser.add_argument('--native', action='store_true', help='measure the native renderer (its swap rate)')
    parser.add_argument('cvars', nargs='*')
    args = parser.parse_args()
    out = ROOT / 'user/logs' / f'{args.name}.csv'
    out.unlink(missing_ok=True)
    env = dict(os.environ, REX_NATIVE_INPUT_REPLAY=str(args.replay), REX_NATIVE_FPS_TITLE='1')
    log = ROOT / 'user/logs/narutorise.log'
    log_start = log.stat().st_size if log.exists() else 0
    process = subprocess.Popen([BINARY, f'--perf_log_csv={out}', *args.cvars], env=env,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        time.sleep(args.seconds)
    finally:
        process.terminate()
        try:
            process.wait(10)
        except subprocess.TimeoutExpired:
            process.kill()
    if args.native:
        # Half-second measurements of the native renderer ([native-fps] in the log).
        text = log.read_bytes()[log_start:].decode('utf-8', 'replace') if log.exists() else ''
        samples = [line.split('[native-fps] ', 1)[1].split() for line in text.splitlines() if '[native-fps] ' in line]
        tail = samples[-int(args.tail * 2):]
        if not tail:
            print(f'{args.name}: no native frame measurements')
            return
        fps = statistics.mean(float(s[0]) for s in tail)
        worst = max(float(s[5]) for s in tail)
        peaks = [float(s[5]) for s in samples]
        print(f'{args.name}: {fps:.1f} fps over the last {len(tail) / 2:.1f} s (native; worst frame {worst:.1f} ms, '
              f'{len(samples)} samples); whole run: {statistics.mean(float(s[0]) for s in samples):.1f} fps, '
              f'{sum(p > 60 for p in peaks)} half-seconds with a frame over 60 ms, {sum(p > 200 for p in peaks)} '
              f'over 200 ms, worst {max(peaks):.0f} ms')
        return
    rows = list(csv.DictReader(out.open())) if out.exists() else []
    frames, elapsed = [], 0.0
    for row in reversed(rows):
        frame_us = int(row['frame_time_us'] or 0)
        if frame_us <= 0:
            continue
        frames.append(frame_us)
        elapsed += frame_us / 1e6
        if elapsed >= args.tail:
            break
    if not frames:
        print(f'{args.name}: no frames recorded')
        return
    fps = len(frames) / elapsed
    p95 = sorted(frames)[int(len(frames) * 0.95) - 1] / 1000
    print(f'{args.name}: {fps:.1f} fps over the last {elapsed:.1f} s '
          f'(median frame {statistics.median(frames) / 1000:.1f} ms, p95 {p95:.1f} ms, {len(rows)} frames total)')


if __name__ == '__main__':
    main()
