#!/usr/bin/env python3
"""Check the physical pizzicato initial condition and linear force control."""

from __future__ import annotations

import argparse
import json
import math
import subprocess
import sys
import tempfile
import wave
from pathlib import Path
from typing import Dict, List, Mapping, Sequence, Tuple


SAMPLE_RATE = 48000
FUNDAMENTAL = 41.20344461410875


def run_csound(
    csound: Path,
    module: Path,
    csd: Path,
    definitions: Mapping[str, object],
    output: Path,
) -> subprocess.CompletedProcess[str]:
    command = [
        str(csound),
        "--opcode-lib={}".format(module),
        "--sample-accurate",
        "--num-threads=1",
        *[
            "--omacro:{}={}".format(name, value)
            for name, value in definitions.items()
        ],
        "-W",
        "-3",
        "--nopeaks",
        "-d",
        "-m128",
        "-o",
        str(output),
        str(csd),
    ]
    return subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=45,
        check=False,
    )


def decode_pcm24(sample: bytes) -> float:
    value = sample[0] | sample[1] << 8 | sample[2] << 16
    if value & 0x800000:
        value -= 1 << 24
    return value / float(1 << 23)


def read_mono(path: Path) -> Tuple[bytes, List[float]]:
    raw = path.read_bytes()
    with wave.open(str(path), "rb") as audio:
        facts = (
            audio.getframerate(),
            audio.getnchannels(),
            audio.getsampwidth(),
        )
        if facts != (SAMPLE_RATE, 2, 3):
            raise ValueError("unexpected pizzicato WAV format: {!r}".format(facts))
        frames = audio.readframes(audio.getnframes())
    samples = []
    for offset in range(0, len(frames), 6):
        left = decode_pcm24(frames[offset:offset + 3])
        right = decode_pcm24(frames[offset + 3:offset + 6])
        samples.append(0.5 * (left + right))
    return raw, samples


def harmonic_levels(
    samples: Sequence[float],
    start: float = 0.02,
    stop: float = 0.07,
    count: int = 8,
) -> List[float]:
    first = int(start * SAMPLE_RATE)
    last = min(len(samples), int(stop * SAMPLE_RATE))
    window = samples[first:last]
    result = []
    for harmonic in range(1, count + 1):
        frequency = harmonic * FUNDAMENTAL
        real = 0.0
        imaginary = 0.0
        window_sum = 0.0
        for index, sample in enumerate(window):
            weight = 0.5 - 0.5 * math.cos(
                2.0 * math.pi * index / max(1, len(window) - 1))
            angle = 2.0 * math.pi * frequency * (first + index) / SAMPLE_RATE
            real += weight * sample * math.cos(angle)
            imaginary -= weight * sample * math.sin(angle)
            window_sum += weight
        amplitude = 2.0 * math.hypot(real, imaginary) / max(window_sum, 1.0)
        result.append(20.0 * math.log10(max(amplitude, 1.0e-15)))
    return result


def read_probes(output: str) -> Dict[int, Tuple[float, ...]]:
    probes: Dict[int, Tuple[float, ...]] = {}
    marker = "WG_PIZZICATO_EXCITER "
    for line in output.splitlines():
        position = line.find(marker)
        if position < 0:
            continue
        words = line[position:].split()
        if len(words) < 9:
            continue
        probes[int(float(words[1]))] = tuple(float(word) for word in words[2:9])
    return probes


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csound", required=True, type=Path)
    parser.add_argument("--module", required=True, type=Path)
    parser.add_argument("--csd", required=True, type=Path)
    parser.add_argument("--model", required=True, type=Path)
    arguments = parser.parse_args()
    failures: List[str] = []

    model = json.loads(arguments.model.read_text(encoding="utf-8"))
    impedances = [
        string["characteristic_impedance_kg_per_second"]
        for string in model["strings"]
    ]
    if impedances != [0.55, 0.42, 0.30, 0.22]:
        failures.append("fixed E1/A1/D2/G2 impedances are {!r}".format(impedances))
    if model.get("evidence_status") != "violin-derived":
        failures.append("physical constants prematurely changed evidence status")

    with tempfile.TemporaryDirectory(prefix="hlolli-pizzicato-") as folder:
        temporary = Path(folder)
        renders: Dict[float, Tuple[bytes, List[float], List[float]]] = {}
        for force in (0.25, 0.5, 1.0):
            output = temporary / "force-{}.wav".format(str(force).replace(".", "p"))
            completed = run_csound(
                arguments.csound,
                arguments.module,
                arguments.csd,
                {"TEST_FORCE": force},
                output,
            )
            if completed.returncode != 0 or not output.exists():
                failures.append(
                    "force {} render failed:\n{}".format(
                        force, completed.stdout.rstrip()))
                continue
            try:
                raw, samples = read_mono(output)
            except (OSError, ValueError, wave.Error) as error:
                failures.append("force {} render: {}".format(force, error))
                continue
            levels = harmonic_levels(samples)
            renders[force] = (raw, samples, levels)
            peak = max(abs(sample) for sample in samples)
            if not 0.0 < peak < 0.98:
                failures.append("force {} produced invalid peak {}".format(force, peak))
            probes = read_probes(completed.stdout)
            if 1 not in probes:
                failures.append("force {} render returned no exciter probe".format(force))
            else:
                active, last, attacks, exciter_samples, recoveries, finite, _ = probes[1]
                if active != 0.0 or last != 0.0:
                    failures.append("pizzicato release left an active point force")
                if attacks != 1.0 or exciter_samples < 4.0:
                    failures.append("pizzicato attack did not run one rounded release")
                if recoveries != 0.0 or finite != 1.0:
                    failures.append("pizzicato exciter was not finite")

        if len(renders) == 3:
            expected = 20.0 * math.log10(2.0)
            maximum_level_error = 0.0
            maximum_shape_change = 0.0
            for low, high in ((0.25, 0.5), (0.5, 1.0)):
                low_levels = renders[low][2]
                high_levels = renders[high][2]
                changes = [right - left for left, right in zip(low_levels, high_levels)]
                shape = [
                    (right - high_levels[0]) - (left - low_levels[0])
                    for left, right in zip(low_levels, high_levels)
                ]
                maximum_level_error = max(
                    maximum_level_error,
                    max(abs(change - expected) for change in changes),
                )
                maximum_shape_change = max(
                    maximum_shape_change, max(abs(change) for change in shape))
            if maximum_level_error > 0.02:
                failures.append(
                    "force doubling missed 6.0206 dB by {:.6g} dB".format(
                        maximum_level_error))
            if maximum_shape_change > 0.02:
                failures.append(
                    "force changed normalized spectrum by {:.6g} dB".format(
                        maximum_shape_change))
            fundamental_dbfs = renders[0.5][2][0]
            if fundamental_dbfs <= -80.0:
                failures.append(
                    "E1 fundamental remained below -80 dBFS at {:.6g}".format(
                        fundamental_dbfs))
            relative = [level - fundamental_dbfs for level in renders[0.5][2]]
            if any(after > before + 0.35 for before, after in zip(relative, relative[1:])):
                failures.append(
                    "rounded displacement did not produce a descending E1 spectrum: {!r}".
                    format([round(item, 3) for item in relative]))
            print(
                "pizzicato force law: max level error {:.9g} dB, "
                "max shape change {:.9g} dB".format(
                    maximum_level_error, maximum_shape_change))
            print("pizzicato E1 fundamental: {:.6f} dBFS".format(
                fundamental_dbfs))
            print(
                "pizzicato E1 harmonics relative to fundamental: {}".format(
                    " ".join("{:.3f}".format(item) for item in relative)))

            position_levels = harmonic_levels(
                renders[0.5][1], count=12)
            tenth_notch = position_levels[9] - 0.5 * (
                position_levels[8] + position_levels[10])
            if tenth_notch > -15.0:
                failures.append(
                    "position 0.10 did not notch harmonic 10: {:.6g} dB".format(
                        tenth_notch))

            position_output = temporary / "position-0p20.wav"
            position_run = run_csound(
                arguments.csound, arguments.module, arguments.csd,
                {"TEST_FORCE": 0.5, "TEST_POSITION": 0.20}, position_output)
            if position_run.returncode != 0:
                failures.append("position 0.20 render failed")
            else:
                _, position_samples = read_mono(position_output)
                position_levels = harmonic_levels(position_samples, count=12)
                fifth_notch = position_levels[4] - 0.5 * (
                    position_levels[3] + position_levels[5])
                if fifth_notch > -15.0:
                    failures.append(
                        "position 0.20 did not notch harmonic 5: {:.6g} dB".
                        format(fifth_notch))
                print(
                    "pizzicato position notches: harmonic 10 {:.3f} dB, "
                    "harmonic 5 {:.3f} dB relative to neighbors".format(
                        tenth_notch, fifth_notch))

        reference = temporary / "trigger-reference.wav"
        low_trigger = temporary / "trigger-low.wav"
        reference_run = run_csound(
            arguments.csound, arguments.module, arguments.csd,
            {"TEST_FORCE": 0.5, "TEST_TRIGGER_LEVEL": 1.0}, reference)
        low_run = run_csound(
            arguments.csound, arguments.module, arguments.csd,
            {"TEST_FORCE": 0.5, "TEST_TRIGGER_LEVEL": 0.25}, low_trigger)
        if (reference_run.returncode != 0 or low_run.returncode != 0):
            failures.append("trigger-level comparison failed to render")
        elif reference.read_bytes() != low_trigger.read_bytes():
            failures.append("kTrigger still changes pizzicato loudness")

        retrigger = temporary / "retrigger.wav"
        retrigger_run = run_csound(
            arguments.csound, arguments.module, arguments.csd,
            {"TEST_FORCE": 0.5, "TEST_RETRIGGER": 1}, retrigger)
        probes = read_probes(retrigger_run.stdout)
        if retrigger_run.returncode != 0 or 2 not in probes:
            failures.append("repeatable-displacement render or probe failed")
        else:
            active, last, attacks, _, recoveries, finite, _ = probes[2]
            if attacks != 2.0:
                failures.append("second gate did not add one displacement state")
            if active != 0.0 or last != 0.0:
                failures.append("retrigger left an active point force")
            if recoveries != 0.0 or finite != 1.0:
                failures.append("retrigger made the exciter invalid")

    if failures:
        print("physical pizzicato test failed:", file=sys.stderr)
        for failure in failures:
            print("- " + failure, file=sys.stderr)
        return 1
    print("physical pizzicato initial condition passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
