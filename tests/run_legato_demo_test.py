#!/usr/bin/env python3
"""Keep the supported legato demo equivalent to the frozen listening fixture."""

from __future__ import annotations

import argparse
from array import array
import math
from pathlib import Path
import shlex
import sys
import tempfile

from compare_wasm_pcm import render_native


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--csound', required=True, type=Path)
    parser.add_argument('--module', required=True, type=Path)
    parser.add_argument('--csd', required=True, type=Path)
    parser.add_argument('--reference', required=True, type=Path)
    options = parser.parse_args()
    notes = [shlex.split(line) for line in options.csd.read_text().splitlines()
             if line.startswith('i "Note"')]
    if len(notes) != 268 or sum(float(note[9]) for note in notes) != 88:
        raise RuntimeError('expected 268 notes and 88 new bow attacks')
    with tempfile.TemporaryDirectory(prefix='hlolli-legato-') as folder:
        actual = Path(folder) / 'demo.f64'
        expected = Path(folder) / 'reference.f64'
        render_native(options.csound, options.module, options.csd, actual)
        render_native(options.csound, options.module, options.reference, expected)
        raw = actual.read_bytes()
        if raw != expected.read_bytes():
            raise RuntimeError('demo changed the selected performance PCM')
        pcm = array('d')
        pcm.frombytes(raw)
        if sys.byteorder != 'little':
            pcm.byteswap()
        if len(pcm) != 9000000 or not all(math.isfinite(x) for x in pcm):
            raise RuntimeError('invalid full-performance PCM')
        peak = max(abs(x) for x in pcm)
        if not 1e-6 < peak < 1:
            raise RuntimeError('silent or clipped performance')
        for bar in range(48):
            start = int((1 + 3 * bar) * 0.625 * 48000) * 2
            end = int((4 + 3 * bar) * 0.625 * 48000) * 2
            if max(abs(x) for x in pcm[start:end]) <= 1e-8:
                raise RuntimeError('silent bar {}'.format(bar + 1))
    print('legato demo: 268 notes, 88 attacks, 48 non-silent bars; '
          '9,000,000 finite samples identical to frozen fixture; peak={}'.format(peak))


if __name__ == '__main__':
    main()
