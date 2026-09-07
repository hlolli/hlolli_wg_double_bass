#!/usr/bin/env python3
"""Inject each distinct private fault field and check finite recovery."""

from __future__ import annotations

import argparse
import math
from pathlib import Path
import subprocess
import sys
import tempfile
from typing import Tuple
import wave


FAULTS = (
    ("exciter", 1, range(1, 4)),
    ("bow", 2, (3,)),
    ("gesture", 3, range(1, 4)),
    ("physics", 4, range(1, 7)),
    ("strange", 5, range(1, 8)),
    ("body", 6, (3,)),
)


def find_marker(output: str, marker: str) -> Tuple[float, ...]:
    for line in output.splitlines():
        start = line.find(marker + " ")
        if start >= 0:
            return tuple(float(word) for word in line[start:].split()[1:])
    raise RuntimeError("missing {} marker".format(marker))


def check_wave(path: Path) -> None:
    with wave.open(str(path), "rb") as stream:
        if stream.getnchannels() != 2 or stream.getsampwidth() != 2:
            raise RuntimeError("wrong WAVE shape")
        if stream.getframerate() != 48000 or stream.getnframes() < 6600:
            raise RuntimeError("wrong WAVE rate or duration")
        pcm = stream.readframes(stream.getnframes())
    samples = memoryview(pcm).cast("h")
    peak = max(abs(sample) for sample in samples)
    if peak < 2:
        raise RuntimeError("recovery render was silent")
    if peak >= 32767:
        raise RuntimeError("recovery render clipped")


def run_case(
    csound: Path,
    module: Path,
    csd: Path,
    output: Path,
    name: str,
    kind: int,
    mode: int,
) -> None:
    completed = subprocess.run(
        [
            str(csound),
            "--opcode-lib={}".format(module),
            "--sample-accurate",
            "--num-threads=1",
            "--omacro:TEST_KIND={}".format(kind),
            "--omacro:TEST_MODE={}".format(mode),
            "-W",
            "-s",
            "--nopeaks",
            "-d",
            "-m128",
            "-o",
            str(output),
            str(csd),
        ],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=20,
    )
    label = "{} fault mode {}".format(name, mode)
    if completed.returncode != 0:
        raise RuntimeError(
            "{} failed with status {}:\n{}".format(
                label, completed.returncode, completed.stdout.rstrip()))
    fault = find_marker(completed.stdout, "WG_FAULT")
    recovery = find_marker(completed.stdout, "WG_RECOVERY")
    if fault != (float(kind), float(mode), 1.0):
        raise RuntimeError("{} was not injected".format(label))
    if len(recovery) != 4 or recovery[:2] != (float(kind), float(mode)):
        raise RuntimeError("{} printed the wrong recovery facts".format(label))
    recoveries, finite = recovery[2:]
    if not math.isfinite(recoveries) or recoveries != 1.0:
        raise RuntimeError(
            "{} used {} recoveries, wanted one".format(label, recoveries))
    if finite != 1.0:
        raise RuntimeError("{} was not finite within 240 samples".format(label))
    check_wave(output)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csound", required=True, type=Path)
    parser.add_argument("--module", required=True, type=Path)
    parser.add_argument("--csd", required=True, type=Path)
    options = parser.parse_args()
    for path in (options.csound, options.module, options.csd):
        if not path.is_file():
            parser.error("missing regular file: {}".format(path))

    count = 0
    try:
        with tempfile.TemporaryDirectory(prefix="hlolli-recovery-") as folder:
            root = Path(folder)
            for name, kind, modes in FAULTS:
                for mode in modes:
                    run_case(
                        options.csound,
                        options.module,
                        options.csd,
                        root / "{}-{}.wav".format(name, mode),
                        name,
                        kind,
                        mode,
                    )
                    count += 1
    except (OSError, RuntimeError, subprocess.TimeoutExpired, ValueError,
            wave.Error) as error:
        print("recovery test failed: {}".format(error), file=sys.stderr)
        return 1
    print("{} finite recovery fault modes passed within 240 samples".format(count))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
