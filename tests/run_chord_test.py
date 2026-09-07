#!/usr/bin/env python3
"""Check synthetic chords and passive sympathy without recording evidence."""

from __future__ import annotations

import argparse
from array import array
import math
from pathlib import Path
import subprocess
import sys
import tempfile
from typing import Dict, List, Mapping, Tuple


SAMPLE_RATE_BLOCKS = ((44100, 1), (48000, 32), (96000, 64))
EXPECTED_COUPLING_MASKS = (14, 13, 11, 7)
EXPECTED_SECOND_POLARIZATION = (0.0, 0.0, 0.0, 0.6)


def definitions(values: Mapping[str, object]) -> List[str]:
    return [
        "--omacro:{}={:.17g}".format(name, value)
        if isinstance(value, float)
        else "--omacro:{}={}".format(name, value)
        for name, value in values.items()
    ]


def render(
    csound: Path,
    module: Path,
    csd: Path,
    values: Mapping[str, object],
    output: Path,
    label: str,
) -> str:
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
        timeout=30,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            "{} failed with status {}:\n{}".format(
                label, completed.returncode, completed.stdout.rstrip()))
    return completed.stdout


def read_pcm(path: Path, label: str) -> array:
    samples = array("d")
    with path.open("rb") as stream:
        samples.fromfile(stream, path.stat().st_size // samples.itemsize)
    if sys.byteorder != "little":
        samples.byteswap()
    if len(samples) == 0 or len(samples) % 2 != 0:
        raise RuntimeError("{} returned invalid stereo PCM".format(label))
    if not all(math.isfinite(sample) for sample in samples):
        raise RuntimeError("{} returned non-finite PCM".format(label))
    peak = max(abs(sample) for sample in samples)
    if not 1.0e-8 < peak < 1.0:
        raise RuntimeError(
            "{} peak {:.9g} was silent or clipped".format(label, peak))
    return samples


def records(output: str, marker: str, fields: int) -> Dict[int, Tuple[float, ...]]:
    found: Dict[int, Tuple[float, ...]] = {}
    for line in output.splitlines():
        position = line.find(marker + " ")
        if position < 0:
            continue
        parts = line[position:].split()
        if len(parts) != fields + 2:
            raise RuntimeError("malformed {} record: {}".format(marker, line))
        string = int(float(parts[1]))
        found[string] = tuple(float(value) for value in parts[2:])
    if set(found) != {1, 2, 3, 4}:
        raise RuntimeError(
            "{} returned string records {}".format(marker, sorted(found)))
    return found


def check_chord_diagnostics(output: str, label: str) -> None:
    chord = records(output, "WG_CHORD", 8)
    sympathy = records(output, "WG_SYMPATHY", 8)
    for string in range(1, 5):
        (failures, energy, peak, samples, source_mask, recoveries,
         second_polarization, finite) = chord[string]
        if failures != 0.0 or recoveries != 0.0 or finite != 1.0:
            raise RuntimeError(
                "{} string {} had a contact failure or recovery".format(
                    label, string))
        if energy <= 0.0 or peak <= 0.0 or samples <= 0.0:
            raise RuntimeError(
                "{} string {} received no chord coupling".format(
                    label, string))
        if int(source_mask) != EXPECTED_COUPLING_MASKS[string - 1]:
            raise RuntimeError(
                "{} string {} coupling mask {} != {}".format(
                    label, string, int(source_mask),
                    EXPECTED_COUPLING_MASKS[string - 1]))
        if not math.isclose(
                second_polarization,
                EXPECTED_SECOND_POLARIZATION[string - 1],
                rel_tol=0.0,
                abs_tol=1.0e-12):
            raise RuntimeError(
                "{} string {} second-polarization value changed".format(
                    label, string))
        current, applied, strange_recoveries, passive_active, passive_energy, \
            passive_peak, passive_samples, passive_finite = sympathy[string]
        if current <= 0.0 or applied <= 0.0:
            raise RuntimeError("{} did not apply sympathy".format(label))
        if strange_recoveries != 0.0 or passive_finite != 1.0:
            raise RuntimeError(
                "{} string {} passive state recovered".format(label, string))
        if passive_active != 0.0 or passive_energy != 0.0 or \
                passive_peak != 0.0 or passive_samples != 0.0:
            raise RuntimeError(
                "{} owned string {} entered passive-open mode".format(
                    label, string))


def normalized_delta(left: array, right: array) -> float:
    if len(left) != len(right):
        raise RuntimeError("comparison renders have different lengths")
    delta_energy = sum((a - b) * (a - b) for a, b in zip(left, right))
    signal_energy = max(
        sum(sample * sample for sample in left),
        sum(sample * sample for sample in right),
    )
    return math.sqrt(delta_energy / max(signal_energy, 1.0e-30))


def check_chord(
    csound: Path, module: Path, csd: Path, root: Path,
) -> None:
    for sample_rate, block_size in SAMPLE_RATE_BLOCKS:
        base = {
            "TEST_SR": sample_rate,
            "TEST_KSMPS": block_size,
            "TEST_CASE": 1,
            "TEST_BODY": 0.68,
            "TEST_SYMPATHETIC": 0.7,
            "TEST_DRY": 0.03,
            "TEST_WET": 0.45,
        }
        outputs = []
        for reverse in (0, 1):
            values = dict(base)
            values["TEST_REVERSE"] = reverse
            label = "{} Hz/{} chord order {}".format(
                sample_rate, block_size, reverse)
            path = root / "chord-{}-{}-{}.f64".format(
                sample_rate, block_size, reverse)
            diagnostics = render(csound, module, csd, values, path, label)
            outputs.append(read_pcm(path, label))
            check_chord_diagnostics(diagnostics, label)
        difference = normalized_delta(outputs[0], outputs[1])
        if difference > 1.0e-12:
            raise RuntimeError(
                "{} Hz/{} chord depended on event order: {:.9g}".format(
                    sample_rate, block_size, difference))


def check_passive_sympathy(
    csound: Path, module: Path, csd: Path, root: Path,
) -> None:
    rendered: Dict[int, array] = {}
    diagnostics: Dict[int, str] = {}
    for enabled in (0, 1):
        values = {
            "TEST_SR": 48000,
            "TEST_KSMPS": 32,
            "TEST_CASE": 2,
            "TEST_BODY": 0.0,
            "TEST_SYMPATHETIC": float(enabled),
            "TEST_DRY": 0.08,
            "TEST_WET": 0.0,
        }
        label = "passive sympathy {}".format("on" if enabled else "off")
        path = root / "sympathy-{}.f64".format(enabled)
        diagnostics[enabled] = render(
            csound, module, csd, values, path, label)
        rendered[enabled] = read_pcm(path, label)

    off = records(diagnostics[0], "WG_SYMPATHY", 8)
    on = records(diagnostics[1], "WG_SYMPATHY", 8)
    for string in (1, 3, 4):
        if off[string][3:] != (0.0, 0.0, 0.0, 0.0, 1.0):
            raise RuntimeError(
                "disabled sympathy advanced open string {}".format(string))
        current, applied, recoveries, active, energy, peak, samples, finite = \
            on[string]
        if current != 1.0 or applied != 1.0 or recoveries != 0.0 or \
                active != 1.0 or finite != 1.0:
            raise RuntimeError(
                "enabled sympathy left string {} inactive or invalid".format(
                    string))
        if energy <= 0.0 or peak <= 0.0 or samples < 24000.0:
            raise RuntimeError(
                "enabled sympathy did not excite open string {}".format(
                    string))
    if normalized_delta(rendered[0], rendered[1]) <= 1.0e-8:
        raise RuntimeError("passive sympathy made no dry string difference")


def check_unowned_tail(
    csound: Path, module: Path, csd: Path, root: Path,
) -> None:
    for sample_rate, block_size in SAMPLE_RATE_BLOCKS:
        for harmonic in (0, 2):
            label = "unowned tail {}/{} harmonic {}".format(
                sample_rate, block_size, harmonic)
            path = root / "tail-{}-{}-{}.f64".format(
                sample_rate, block_size, harmonic)
            output = render(csound, module, csd, {
                "TEST_SR": sample_rate,
                "TEST_KSMPS": block_size,
                "TEST_CASE": 3,
                "TEST_HARMONIC": harmonic,
            }, path, label)
            read_pcm(path, label)
            before = records(output, "WG_TAIL_1", 10)
            after = records(output, "WG_TAIL_2", 10)
            for string in range(1, 5):
                early, late = before[string], after[string]
                # Target fundamental, finger stop, releases, harmonic and
                # attacks must survive loss of ownership without a new command.
                if early[:5] != late[:5] or late[2] != 0 or late[4] != 1:
                    raise RuntimeError(
                        "{} string {} changed geometry/events: {} -> {}".format(
                            label, string, early[:5], late[:5]))
                if late[3] != harmonic or not 0 < late[1] < 1:
                    raise RuntimeError(
                        "{} lost its stopped/harmonic state".format(label))
                if not 0 < late[5] < early[5]:
                    raise RuntimeError(
                        "{} did not retain a decaying tail".format(label))
                if late[6:] != (0.0, 1.0, 1.0, 1.0):
                    raise RuntimeError(
                        "{} clipped or invalidated tail state".format(label))


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
        with tempfile.TemporaryDirectory(prefix="hlolli-chord-") as folder:
            root = Path(folder)
            check_chord(options.csound, options.module, options.csd, root)
            check_passive_sympathy(
                options.csound, options.module, options.csd, root)
            check_unowned_tail(
                options.csound, options.module, options.csd, root)
    except (OSError, RuntimeError, subprocess.TimeoutExpired, ValueError,
            OverflowError) as error:
        print("chord test failed: {}".format(error), file=sys.stderr)
        return 1
    print(
        "double-bass chords passed: three sample-rate/block pairs, "
        "event-order invariance, four-string coupling, body-free "
        "passive sympathy, and stopped/harmonic unowned tails")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
