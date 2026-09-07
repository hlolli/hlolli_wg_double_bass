#!/usr/bin/env python3
"""Measure many-handle one/two-worker cost and enforce PCM agreement."""

from __future__ import annotations

import argparse
from array import array
import math
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile
import time
from typing import Dict, List, Tuple


HANDLE_COUNT = 32
RENDER_SECONDS = 2.0
MAX_PARITY_ABSOLUTE_ERROR = 1.0e-12
MAX_PARITY_RMS_ERROR = 1.0e-13


def cpu_model() -> str:
    try:
        for line in Path("/proc/cpuinfo").read_text(encoding="utf-8").splitlines():
            if line.lower().startswith("model name"):
                return line.partition(":")[2].strip()
    except OSError:
        pass
    return "unknown"


def run(
    csound: Path, module: Path, csd: Path, output: Path, workers: int,
) -> Tuple[array, float, float]:
    command = [
        str(csound),
        "--opcode-lib={}".format(module),
        "--sample-accurate",
        "--num-threads={}".format(workers),
        "--omacro:TEST_HANDLES={}".format(HANDLE_COUNT),
        "--omacro:TEST_SECONDS={:.17g}".format(RENDER_SECONDS),
        "--format=raw",
        "--format=double",
        "--nopeaks",
        "-o",
        str(output),
        "-d",
        "-m128",
        str(csd),
    ]
    child_before = os.times()
    started = time.perf_counter()
    completed = subprocess.run(
        command,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=60,
    )
    elapsed = time.perf_counter() - started
    child_after = os.times()
    cpu_seconds = (
        child_after.children_user + child_after.children_system
        - child_before.children_user - child_before.children_system
    )
    if completed.returncode != 0:
        raise RuntimeError(
            "{}-worker render failed with status {}:\n{}".format(
                workers, completed.returncode, completed.stdout.rstrip()))
    marker = "WG_COST_HANDLES {}".format(HANDLE_COUNT)
    if marker not in completed.stdout:
        raise RuntimeError("{}-worker render reported no handle count".format(workers))
    data = output.read_bytes()
    if not data or len(data) % 16 != 0:
        raise RuntimeError("{}-worker render returned invalid PCM".format(workers))
    samples = array("d")
    samples.frombytes(data)
    if sys.byteorder != "little":
        samples.byteswap()
    if len(samples) // 2 < math.floor(48000 * RENDER_SECONDS * 0.99):
        raise RuntimeError("{}-worker render returned short PCM".format(workers))
    if not all(math.isfinite(sample) for sample in samples):
        raise RuntimeError("{}-worker render returned non-finite PCM".format(workers))
    peak = max(abs(sample) for sample in samples)
    if not 1.0e-8 < peak < 1.0:
        raise RuntimeError(
            "{}-worker render was silent or clipped: {:.9g}".format(
                workers, peak))
    return samples, elapsed, cpu_seconds


def compare(reference: array, actual: array) -> Tuple[float, float]:
    if len(reference) != len(actual):
        raise RuntimeError("one/two-worker sample counts differ")
    maximum = 0.0
    square_sum = 0.0
    for wanted, found in zip(reference, actual):
        difference = abs(wanted - found)
        maximum = max(maximum, difference)
        square_sum += difference * difference
    rms = math.sqrt(square_sum / len(reference))
    if maximum > MAX_PARITY_ABSOLUTE_ERROR or rms > MAX_PARITY_RMS_ERROR:
        raise RuntimeError(
            "one/two-worker PCM differs: max={:.17g}, rms={:.17g}".format(
                maximum, rms))
    return maximum, rms


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csound", required=True, type=Path)
    parser.add_argument("--module", required=True, type=Path)
    parser.add_argument("--csd", required=True, type=Path)
    options = parser.parse_args()
    for path in (options.csound, options.module, options.csd):
        if not path.is_file():
            parser.error("missing regular file: {}".format(path))

    try:
        timings = {1: [], 2: []}  # type: Dict[int, List[Tuple[float, float]]]
        rendered = {}  # type: Dict[int, array]
        repeat_differences = {}  # type: Dict[int, Tuple[float, float]]
        with tempfile.TemporaryDirectory(prefix="hlolli-cost-") as folder:
            root = Path(folder)
            for run_index, workers in enumerate((1, 2, 2, 1), start=1):
                pcm, elapsed, cpu_seconds = run(
                    options.csound, options.module, options.csd,
                    root / "run-{}-workers-{}.f64".format(run_index, workers),
                    workers,
                )
                timings[workers].append((elapsed, cpu_seconds))
                if workers not in rendered:
                    rendered[workers] = pcm
                else:
                    repeat_differences[workers] = compare(
                        rendered[workers], pcm)
        maximum, rms = compare(rendered[1], rendered[2])
        best = {
            workers: min(measurements, key=lambda measurement: measurement[0])
            for workers, measurements in timings.items()
        }
        print(
            "release cost host={} system={} machine={} logical_cpus={} cpu={}".format(
                platform.node() or "unknown", platform.system(),
                platform.machine(), os.cpu_count() or 0, cpu_model()))
        print(
            "handles={} render_seconds={:.1f} one_worker_wall={:.6f} "
            "one_worker_cpu={:.6f} two_worker_wall={:.6f} "
            "two_worker_cpu={:.6f} pcm_max={:.17g} pcm_rms={:.17g} "
            "two_worker_repeat_max={:.17g} two_worker_repeat_rms={:.17g}".format(
                HANDLE_COUNT, RENDER_SECONDS,
                best[1][0], best[1][1], best[2][0], best[2][1], maximum, rms,
                repeat_differences[2][0], repeat_differences[2][1]))
    except (OSError, RuntimeError, subprocess.TimeoutExpired, ValueError,
            OverflowError) as error:
        print("release cost test failed: {}".format(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
