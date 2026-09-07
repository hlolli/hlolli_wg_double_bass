#!/usr/bin/env python3
"""Exercise release-build rate, block, duration, control, and string gates."""

from __future__ import annotations

import argparse
from array import array
import hashlib
import math
from pathlib import Path
import subprocess
import sys
import tempfile
from typing import Dict, List, Mapping, Tuple


SAMPLE_RATES = (44100, 48000, 88200, 96000)
BLOCK_SIZES = (1, 16, 32, 64)
# The probe lands at 0.70 seconds, where the 23 Hz articulation sweep has
# returned to arco. This makes the bow-state finite check meaningful after
# every stress render while all nine modes still run repeatedly beforehand.
STRESS_SECONDS = 0.72
LONG_NOTE_SECONDS = 12.0
COMBINATION_SECONDS = 0.55

StringRecord = Tuple[float, ...]


def definitions(values: Mapping[str, object]) -> List[str]:
    return [
        "--omacro:{}={:.17g}".format(name, value)
        if isinstance(value, float)
        else "--omacro:{}={}".format(name, value)
        for name, value in values.items()
    ]


def read_pcm(path: Path, sample_rate: int, seconds: float, label: str) -> str:
    data = path.read_bytes()
    if not data or len(data) % 16 != 0:
        raise RuntimeError("{} returned invalid stereo float64 PCM".format(label))
    samples = array("d")
    samples.frombytes(data)
    if sys.byteorder != "little":
        samples.byteswap()
    minimum_frames = math.floor(sample_rate * seconds * 0.99)
    if len(samples) // 2 < minimum_frames:
        raise RuntimeError(
            "{} returned {} frames, wanted at least {}".format(
                label, len(samples) // 2, minimum_frames))
    if not all(math.isfinite(sample) for sample in samples):
        raise RuntimeError("{} returned non-finite PCM".format(label))
    peak = max(abs(sample) for sample in samples)
    active = sum(abs(sample) > 1.0e-15 for sample in samples)
    if peak <= 1.0e-8 or active < 1000:
        raise RuntimeError("{} was silent: peak {:.9g}".format(label, peak))
    if peak >= 1.0:
        raise RuntimeError("{} clipped: peak {:.9g}".format(label, peak))
    return hashlib.sha256(data).hexdigest()


def parse_string_records(output: str, label: str) -> Dict[int, StringRecord]:
    result: Dict[int, StringRecord] = {}
    marker = "WG_RELEASE_STRING "
    for line in output.splitlines():
        offset = line.find(marker)
        if offset < 0:
            continue
        words = line[offset:].split()
        if len(words) != 20:
            raise RuntimeError("{} printed malformed string facts".format(label))
        string = int(float(words[1]))
        result[string] = tuple(float(word) for word in words[2:])
    if set(result) != {1, 2, 3, 4}:
        raise RuntimeError(
            "{} printed string records {}".format(label, sorted(result)))
    return result


def parse_body_record(output: str, label: str) -> Tuple[float, ...]:
    marker = "WG_RELEASE_BODY "
    found = []
    for line in output.splitlines():
        offset = line.find(marker)
        if offset >= 0:
            words = line[offset:].split()
            if len(words) != 5:
                raise RuntimeError("{} printed malformed body facts".format(label))
            found.append(tuple(float(word) for word in words[1:]))
    if len(found) != 1:
        raise RuntimeError("{} printed {} body records".format(label, len(found)))
    return found[0]


def check_diagnostics(
    output: str, enabled_mask: int, minimum_excitations: int, label: str,
) -> None:
    records = parse_string_records(output, label)
    for string, values in records.items():
        enabled = 1 if enabled_mask & (1 << (string - 1)) else 0
        (reported_enabled, wave_samples, excitations, wave_clips, wave_finite,
         bow_failures, bow_fallbacks, bow_recoveries, bow_finite,
         gesture_recoveries, gesture_finite, exciter_recoveries,
         exciter_finite, contact_failures, physics_recoveries, physics_finite,
         strange_recoveries, strange_finite) = values
        if reported_enabled != float(enabled):
            raise RuntimeError(
                "{} string {} reported the wrong enable state".format(
                    label, string))
        if wave_clips != 0.0:
            raise RuntimeError(
                "{} string {} clipped its waveguide".format(label, string))
        if enabled:
            if wave_samples <= 0.0 or excitations < minimum_excitations:
                raise RuntimeError(
                    "{} string {} recorded only {} excitations".format(
                        label, string, excitations))
            counters = (
                bow_failures, bow_fallbacks, bow_recoveries,
                gesture_recoveries, exciter_recoveries, contact_failures,
                physics_recoveries, strange_recoveries,
            )
            if any(counter != 0.0 for counter in counters):
                raise RuntimeError(
                    "{} string {} failed or recovered: {}".format(
                        label, string, counters))
            finite = (
                wave_finite, bow_finite, gesture_finite, exciter_finite,
                physics_finite, strange_finite,
            )
            if finite != (1.0,) * len(finite):
                raise RuntimeError(
                    "{} string {} had invalid state: {}".format(
                        label, string, finite))
        elif excitations != 0.0:
            raise RuntimeError(
                "{} disabled string {} was directly excited".format(
                    label, string))

    processed_samples, resets, clips, finite = parse_body_record(output, label)
    if processed_samples <= 0.0 or resets != 0.0 or clips != 0.0 or finite != 1.0:
        raise RuntimeError(
            "{} body failed: samples={} resets={} clips={} finite={}".format(
                label, processed_samples, resets, clips, finite))


def render(
    csound: Path,
    module: Path,
    csd: Path,
    root: Path,
    values: Mapping[str, object],
    enabled_mask: int,
    minimum_excitations: int,
    label: str,
) -> str:
    output = root / (label.replace(" ", "-").replace("/", "-") + ".f64")
    command = [
        str(csound),
        "--opcode-lib={}".format(module),
        "--sample-accurate",
        "--num-threads=1",
        *definitions(values),
        "--format=raw",
        "--format=double",
        "--nopeaks",
        "-o",
        str(output),
        "-d",
        "-m128",
        str(csd),
    ]
    completed = subprocess.run(
        command,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=90,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            "{} failed with status {}:\n{}".format(
                label, completed.returncode, completed.stdout.rstrip()))
    sample_rate = int(values["TEST_SR"])
    seconds = float(values["TEST_SECONDS"])
    digest = read_pcm(output, sample_rate, seconds, label)
    check_diagnostics(
        completed.stdout, enabled_mask, minimum_excitations, label)
    return digest


def enabled_values(mask: int) -> Dict[str, int]:
    return {
        "TEST_ENABLE_{}".format(string):
            1 if mask & (1 << (string - 1)) else 0
        for string in range(1, 5)
    }


def check_rate_block_stress(
    csound: Path, module: Path, csd: Path, root: Path,
) -> None:
    for sample_rate in SAMPLE_RATES:
        for block_size in BLOCK_SIZES:
            values: Dict[str, object] = {
                "TEST_SR": sample_rate,
                "TEST_KSMPS": block_size,
                "TEST_SECONDS": STRESS_SECONDS,
                "TEST_MODE": 1,
                **enabled_values(15),
            }
            render(
                csound, module, csd, root, values, 15, 3,
                "stress-{}-{}".format(sample_rate, block_size))


def check_long_note(
    csound: Path, module: Path, csd: Path, root: Path,
) -> None:
    values: Dict[str, object] = {
        "TEST_SR": 48000,
        "TEST_KSMPS": 32,
        "TEST_SECONDS": LONG_NOTE_SECONDS,
        "TEST_MODE": 2,
        **enabled_values(15),
    }
    render(csound, module, csd, root, values, 15, 1, "long-all-strings")


def check_string_combinations(
    csound: Path, module: Path, csd: Path, root: Path,
) -> None:
    digests = set()
    for mask in range(1, 16):
        values: Dict[str, object] = {
            "TEST_SR": 48000,
            "TEST_KSMPS": 32,
            "TEST_SECONDS": COMBINATION_SECONDS,
            "TEST_MODE": 2,
            **enabled_values(mask),
        }
        digest = render(
            csound, module, csd, root, values, mask, 1,
            "combination-{:02d}".format(mask))
        if digest in digests:
            raise RuntimeError(
                "string combination {:02d} duplicated another PCM render".format(
                    mask))
        digests.add(digest)


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
        with tempfile.TemporaryDirectory(prefix="hlolli-release-stress-") as folder:
            root = Path(folder)
            check_rate_block_stress(
                options.csound, options.module, options.csd, root)
            check_long_note(options.csound, options.module, options.csd, root)
            check_string_combinations(
                options.csound, options.module, options.csd, root)
    except (OSError, RuntimeError, subprocess.TimeoutExpired, ValueError,
            OverflowError) as error:
        print("release stress test failed: {}".format(error), file=sys.stderr)
        return 1
    print(
        "release runtime matrix passed: 16 sample-rate/block stress cases, "
        "one 12-second all-string note, and all 15 non-empty string combinations")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
