#!/usr/bin/env python3
"""Build the native VBSP host and VVIS/VRAD DLLs used by Puzzle Maker."""
import argparse
import os
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jobs', type=int, default=os.cpu_count() or 1)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    subprocess.run([sys.executable, 'tools/gameprep.py', '--tool', 'vbsp',
                    '--out', 'build/tools/vbsp/prep'], cwd=root, check=True)
    subprocess.run([sys.executable, 'tools/build_game.py', '--release', '--jobs', str(args.jobs),
                    '--program', 'vbsp_linux', '--layout', 'build/tools/vbsp/prep/layout.json',
                    '--build-dir', 'build/tools/vbsp', '--out', 'build/portal2-map-compiler'], cwd=root, check=True)


if __name__ == '__main__':
    main()
