#!/usr/bin/env python3
"""Compare actual browser-WASM PCM with a native double-precision render."""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
from typing import List


MAX_ABSOLUTE_ERROR = 1.0e-12
MAX_RMS_ERROR = 1.0e-13


def read_doubles(path: Path) -> List[float]:
    data = path.read_bytes()
    if not data or len(data) % 8 != 0:
        raise RuntimeError("invalid float64 PCM file: {}".format(path))
    return [sample[0] for sample in struct.iter_unpack("<d", data)]


def render_native(
    csound: Path, module: Path, csd: Path, output: Path,
) -> None:
    completed = subprocess.run(
        [
            str(csound),
            "--opcode-lib={}".format(module),
            "--sample-accurate",
            "--num-threads=1",
            "--omacro:TEST_CASE=1",
            "--omacro:TEST_ORDER=0",
            "--format=raw",
            "--format=double",
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
        timeout=30,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            "native PCM render failed with status {}:\n{}".format(
                completed.returncode, completed.stdout.rstrip()))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csound", required=True, type=Path)
    parser.add_argument("--module", required=True, type=Path)
    parser.add_argument("--csd", required=True, type=Path)
    parser.add_argument("--browser-pcm", required=True, type=Path)
    parser.add_argument("--browser-facts", required=True, type=Path)
    parser.add_argument("--expected-samples", type=int)
    options = parser.parse_args()
    if options.expected_samples is not None and options.expected_samples < 1:
        parser.error("--expected-samples must be positive")
    for path in (
            options.csound, options.module, options.csd,
            options.browser_pcm, options.browser_facts):
        if not path.is_file():
            parser.error("missing regular file: {}".format(path))

    try:
        facts = json.loads(options.browser_facts.read_text(encoding="utf-8"))
        if facts.get("pcm_channels") != 2 or facts.get("pcm_sample_rate") != 48000:
            raise RuntimeError("browser PCM has the wrong channel count or rate")
        with tempfile.TemporaryDirectory(prefix="hlolli-wasm-pcm-") as folder:
            native_path = Path(folder) / "native.f64"
            render_native(
                options.csound, options.module, options.csd, native_path)
            native = read_doubles(native_path)
        browser = read_doubles(options.browser_pcm)
        if len(native) != len(browser):
            raise RuntimeError(
                "PCM sample counts differ: native={} browser={}".format(
                    len(native), len(browser)))
        if (options.expected_samples is not None
                and len(native) != options.expected_samples):
            raise RuntimeError("PCM does not cover the complete expected score")
        if facts.get("pcm_samples") != len(browser):
            raise RuntimeError("browser facts contain the wrong PCM sample count")
        if any(not math.isfinite(value) for value in native + browser):
            raise RuntimeError("native or browser PCM contains a non-finite sample")
        differences = [
            abs(native_sample - browser_sample)
            for native_sample, browser_sample in zip(native, browser)
        ]
        maximum = max(differences)
        rms = math.sqrt(sum(value * value for value in differences) / len(differences))
        native_peak = max(abs(value) for value in native)
        browser_peak = max(abs(value) for value in browser)
        if native_peak < 1.0e-6 or browser_peak < 1.0e-6:
            raise RuntimeError("native or browser PCM was silent")
        if native_peak >= 1.0 or browser_peak >= 1.0:
            raise RuntimeError("native or browser PCM was not bounded")
        if maximum > MAX_ABSOLUTE_ERROR or rms > MAX_RMS_ERROR:
            raise RuntimeError(
                "PCM differs: max={:.17g} (limit {:.1e}), "
                "rms={:.17g} (limit {:.1e})".format(
                    maximum, MAX_ABSOLUTE_ERROR, rms, MAX_RMS_ERROR))
        print(
            "actual browser-WASM PCM agrees with native: samples={} "
            "max={:.17g} rms={:.17g} native_peak={:.17g} "
            "browser_peak={:.17g}".format(
                len(native), maximum, rms, native_peak, browser_peak))
    except (OSError, RuntimeError, ValueError, json.JSONDecodeError,
            subprocess.TimeoutExpired) as error:
        print("WASM PCM comparison failed: {}".format(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
