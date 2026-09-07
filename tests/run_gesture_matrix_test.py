#!/usr/bin/env python3
"""Exercise every public gesture, harmonics, and continuous slurs."""

from __future__ import annotations

import argparse
from array import array
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
from typing import Dict, List, Mapping, Tuple


OPEN_FREQUENCIES = (
    41.20344461410875,
    55.0,
    73.41619197935188,
    97.99885899543733,
)
ARTICULATION_NAMES = (
    "arco",
    "detache",
    "martele",
    "spiccato",
    "tremolo",
    "pizzicato",
    "bartok",
    "col-legno-battuto",
    "col-legno-tratto",
)
RELEASE_KEYS = (
    "arco",
    "detache",
    "martele",
    "spiccato",
    "tremolo",
    "pizzicato_right",
    "bartok",
    "battuto",
    "tratto",
)

RecordMap = Dict[Tuple[int, int], Tuple[float, ...]]


def definitions(values: Mapping[str, object]) -> List[str]:
    return [
        "--omacro:{}={:.17g}".format(name, value)
        if isinstance(value, float)
        else "--omacro:{}={}".format(name, value)
        for name, value in values.items()
    ]


def run(
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
    samples = array("d")
    with output.open("rb") as stream:
        samples.fromfile(stream, output.stat().st_size // samples.itemsize)
    if sys.byteorder != "little":
        samples.byteswap()
    if len(samples) == 0 or len(samples) % 2 != 0:
        raise RuntimeError("{} returned invalid stereo PCM".format(label))
    if not all(math.isfinite(sample) for sample in samples):
        raise RuntimeError("{} returned non-finite PCM".format(label))
    peak = max(abs(sample) for sample in samples)
    if not 1.0e-9 < peak < 1.0:
        raise RuntimeError(
            "{} peak {:.9g} was silent or clipped".format(label, peak))
    return completed.stdout


def read_records(
    output: str, marker: str, value_count: int,
) -> RecordMap:
    result: RecordMap = {}
    prefix = marker + " "
    for line in output.splitlines():
        offset = line.find(prefix)
        if offset < 0:
            continue
        words = line[offset:].split()
        if len(words) != value_count + 3:
            raise RuntimeError("malformed {} record: {}".format(marker, line))
        tag = int(float(words[1]))
        string = int(float(words[2]))
        result[(tag, string)] = tuple(float(word) for word in words[3:])
    return result


def probe_bundle(output: str) -> Dict[str, RecordMap]:
    return {
        "gesture": read_records(output, "WG_MATRIX_GESTURE", 18),
        "exciter": read_records(output, "WG_MATRIX_EXCITER", 17),
        "bow": read_records(output, "WG_MATRIX_BOW", 8),
        "wave": read_records(output, "WG_MATRIX_WAVE", 3),
        "harmonic": read_records(output, "WG_MATRIX_HARMONIC", 5),
        "finger": read_records(output, "WG_MATRIX_FINGER", 3),
    }


def close(left: float, right: float, tolerance: float = 1.0e-10) -> bool:
    return abs(left - right) <= tolerance * max(1.0, abs(left), abs(right))


def expected_release_gain(
    model: Mapping[str, object], string: int, articulation: int,
    frequency: float, speed: float,
) -> float:
    if articulation == 0:
        return 1.0
    releases = model["release_t60_seconds"]
    strings = model["strings"]
    assert isinstance(releases, dict)
    assert isinstance(strings, list)
    string_model = strings[string - 1]
    assert isinstance(string_model, dict)
    release_key = RELEASE_KEYS[articulation]
    if articulation == 5 and speed < 0.0:
        release_key = "pizzicato_left"
    tail = float(releases[release_key])
    passive = float(string_model["loss_time_constant_seconds"])
    wanted_rate = -math.log(0.001) / tail
    added_rate = max(0.0, wanted_rate - 1.0 / passive)
    return max(0.05, min(1.0, math.exp(-added_rate / frequency)))


def expected_gesture_timing(
    model: Mapping[str, object], articulation: int, speed: float,
) -> Tuple[int, int]:
    gestures = model["gestures"]
    assert isinstance(gestures, dict)

    def samples(seconds: float) -> int:
        return max(1, math.floor(seconds * 48000.0 + 0.5))

    if articulation == 1:
        detache = gestures["detache"]
        assert isinstance(detache, dict)
        return (
            samples(float(detache["onset_seconds"])),
            samples(float(detache["stroke_seconds"])),
        )
    if articulation == 2:
        martele = gestures["martele"]
        assert isinstance(martele, dict)
        return (
            samples(float(martele["preload_seconds"])),
            samples(float(martele["stroke_seconds"])),
        )
    if articulation == 3:
        spiccato = gestures["spiccato"]
        assert isinstance(spiccato, dict)
        stroke = samples(
            float(spiccato["stroke_fast_seconds"])
            + (float(spiccato["stroke_slow_seconds"])
               - float(spiccato["stroke_fast_seconds"]))
            * (1.0 - min(1.0, abs(speed))))
        return stroke // 2, stroke
    if articulation == 4:
        tremolo = gestures["tremolo"]
        assert isinstance(tremolo, dict)
        rate = (
            float(tremolo["rate_min_hz"])
            + (float(tremolo["rate_max_hz"])
               - float(tremolo["rate_min_hz"]))
            * min(1.0, abs(speed))
        )
        return (
            samples(float(tremolo["onset_seconds"])),
            samples(0.5 / rate),
        )
    return 0, 0


def require_common_state(
    bundle: Dict[str, RecordMap], key: Tuple[int, int], label: str,
) -> None:
    if any(key not in records for records in bundle.values()):
        raise RuntimeError("{} omitted probe {}".format(label, key))
    gesture = bundle["gesture"][key]
    exciter = bundle["exciter"][key]
    bow = bundle["bow"][key]
    wave = bundle["wave"][key]
    harmonic = bundle["harmonic"][key]
    finger = bundle["finger"][key]
    if gesture[14] != 0.0 or gesture[17] != 1.0:
        raise RuntimeError("{} gesture recovered or became invalid".format(label))
    if exciter[15] != 0.0 or exciter[16] != 1.0:
        raise RuntimeError("{} exciter recovered or became invalid".format(label))
    if bow[0] != 0.0 or bow[3] != 0.0:
        raise RuntimeError("{} bow failed or recovered".format(label))
    if wave[1] != 0.0 or wave[2] != 1.0:
        raise RuntimeError("{} waveguide clipped or became invalid".format(label))
    if harmonic[3] != 0.0 or harmonic[4] != 1.0:
        raise RuntimeError("{} harmonic state rejected or became invalid".format(label))
    if finger[2] != 1.0:
        raise RuntimeError("{} finger state became invalid".format(label))


def check_single_articulation(
    model: Mapping[str, object], bundle: Dict[str, RecordMap],
    string: int, articulation: int,
) -> None:
    name = ARTICULATION_NAMES[articulation]
    label = "{} string {}".format(name, string)
    active_key = (1, string)
    release_key = (2, string)
    require_common_state(bundle, active_key, label + " active")
    require_common_state(bundle, release_key, label + " release")
    active_gesture = bundle["gesture"][active_key]
    released_gesture = bundle["gesture"][release_key]
    active_exciter = bundle["exciter"][active_key]
    active_bow = bundle["bow"][active_key]
    active_wave = bundle["wave"][active_key]

    if active_gesture[0] != float(articulation):
        raise RuntimeError("{} selected articulation {}".format(
            label, active_gesture[0]))
    if released_gesture[15] != float(articulation):
        raise RuntimeError("{} lost its release articulation".format(label))
    expected_gain = expected_release_gain(
        model, string, articulation, OPEN_FREQUENCIES[string - 1], 0.45)
    if not close(released_gesture[16], expected_gain):
        raise RuntimeError(
            "{} release gain {} != {}".format(
                label, released_gesture[16], expected_gain))
    if released_gesture[1] != 0.0:
        raise RuntimeError("{} stayed active after note-off".format(label))
    if active_wave[0] != 1.0:
        raise RuntimeError("{} did not create exactly one excitation".format(label))
    expected_onset, expected_stroke = expected_gesture_timing(
        model, articulation, 0.45)
    if (active_gesture[8], active_gesture[9]) != (
            float(expected_onset), float(expected_stroke)):
        raise RuntimeError("{} gesture timing changed".format(label))

    if articulation <= 4:
        if active_gesture[1] != 1.0:
            raise RuntimeError("{} bowed gesture was inactive".format(label))
        if not (0.0 <= active_gesture[5] <= 1.0 and
                -1.0 <= active_gesture[6] <= 1.0 and
                0.0 <= active_gesture[7] <= 1.0):
            raise RuntimeError("{} produced an unbounded bowed control".format(label))
        if active_bow[2] < 1.0 or active_bow[4] != 1.0:
            raise RuntimeError("{} produced no finite bow attack".format(label))
        if not close(active_bow[6], 0.12) or not close(
                active_bow[7], 0.12, 2.0e-7):
            raise RuntimeError("{} missed its bow position".format(label))
        if any(active_exciter[index] != 0.0 for index in range(6, 13)):
            raise RuntimeError("{} incorrectly fired a point exciter".format(label))
        if articulation == 4 and active_gesture[10] < 2.0:
            raise RuntimeError("tremolo did not advance multiple strokes")
    else:
        if active_gesture[1] != 0.0 or active_bow[5] != 0.0:
            raise RuntimeError("{} incorrectly retained bow contact".format(label))
        if not close(active_exciter[3], 0.12):
            raise RuntimeError("{} missed its exciter position".format(label))
        if active_exciter[4] <= 0.0 or active_exciter[5] <= 0.0 or \
                active_exciter[14] <= 0.0:
            raise RuntimeError("{} produced no point-exciter energy".format(label))
        expected_counter = {
            5: 6,
            6: 8,
            7: 10,
            8: 11,
        }[articulation]
        if active_exciter[expected_counter] != 1.0:
            raise RuntimeError("{} did not fire exactly once".format(label))
        if articulation == 6 and active_exciter[9] < 1.0:
            raise RuntimeError("Bartok pizzicato produced no fingerboard impact")
        if articulation == 8 and active_exciter[12] <= 0.0:
            raise RuntimeError("col legno tratto produced no contact samples")


def check_articulation_matrix(
    csound: Path, module: Path, csd: Path, model: Mapping[str, object],
    root: Path,
) -> None:
    for articulation, name in enumerate(ARTICULATION_NAMES):
        for string, frequency in enumerate(OPEN_FREQUENCIES, start=1):
            label = "{} string {}".format(name, string)
            path = root / "{}-{}.f64".format(name, string)
            output = run(csound, module, csd, {
                "TEST_CASE": 1,
                "TEST_STRING": string,
                "TEST_FREQUENCY": frequency,
                "TEST_ARTICULATION": articulation,
                "TEST_HARMONIC": 0,
            }, path, label)
            check_single_articulation(
                model, probe_bundle(output), string, articulation)


def check_harmonics(
    csound: Path, module: Path, csd: Path, root: Path,
) -> None:
    for string, fundamental in enumerate(OPEN_FREQUENCIES, start=1):
        label = "second harmonic string {}".format(string)
        output = run(csound, module, csd, {
            "TEST_CASE": 1,
            "TEST_STRING": string,
            "TEST_FREQUENCY": 2.0 * fundamental,
            "TEST_ARTICULATION": 0,
            "TEST_HARMONIC": 2,
        }, root / "harmonic-{}.f64".format(string), label)
        bundle = probe_bundle(output)
        require_common_state(bundle, (1, string), label)
        harmonic = bundle["harmonic"][(1, string)]
        if harmonic[0] != 2.0 or harmonic[1] != 2.0:
            raise RuntimeError("{} did not engage the harmonic path".format(label))


def check_slurs(
    csound: Path, module: Path, csd: Path, root: Path,
) -> None:
    for string, fundamental in enumerate(OPEN_FREQUENCIES, start=1):
        label = "continuous slur string {}".format(string)
        output = run(csound, module, csd, {
            "TEST_CASE": 2,
            "TEST_STRING": string,
            "TEST_FREQUENCY": fundamental,
            "TEST_FREQUENCY2": 1.5 * fundamental,
        }, root / "slur-{}.f64".format(string), label)
        bundle = probe_bundle(output)
        for tag in (1, 2, 3):
            require_common_state(bundle, (tag, string), label)
        early_bow = bundle["bow"][(1, string)]
        late_bow = bundle["bow"][(2, string)]
        released_gesture = bundle["gesture"][(3, string)]
        early_wave = bundle["wave"][(1, string)]
        late_wave = bundle["wave"][(2, string)]
        early_finger = bundle["finger"][(1, string)]
        late_finger = bundle["finger"][(2, string)]
        if early_bow[2] != 1.0 or late_bow[2] != 1.0:
            raise RuntimeError("{} retriggered the bow".format(label))
        if early_wave[0] != 1.0 or late_wave[0] != 1.0:
            raise RuntimeError("{} created another string excitation".format(label))
        if late_finger[0] <= 1.25 * early_finger[0]:
            raise RuntimeError("{} did not reach its higher pitch".format(label))
        if released_gesture[1] != 0.0 or released_gesture[16] != 1.0:
            raise RuntimeError("{} did not reach passive arco release".format(label))


def check_preset_sequence(
    csound: Path, module: Path, csd: Path, root: Path,
) -> None:
    label = "same-handle bowed preset sequence"
    output = run(csound, module, csd, {
        "TEST_CASE": 3,
        "TEST_STRING": 2,
        "TEST_FREQUENCY": OPEN_FREQUENCIES[1],
    }, root / "preset-sequence.f64", label)
    bundle = probe_bundle(output)
    for tag, articulation in enumerate(range(5), start=1):
        key = (tag, 2)
        require_common_state(bundle, key, label)
        gesture = bundle["gesture"][key]
        wave = bundle["wave"][key]
        bow = bundle["bow"][key]
        if gesture[0] != float(articulation):
            raise RuntimeError(
                "{} tag {} selected {}".format(label, tag, gesture[0]))
        if gesture[12] != float(articulation):
            raise RuntimeError(
                "{} tag {} counted {} preset changes".format(
                    label, tag, gesture[12]))
        if wave[0] != 1.0 or bow[2] != 1.0:
            raise RuntimeError("{} retriggered while changing preset".format(label))


def check_control_bounds(
    csound: Path, module: Path, csd: Path, model: Mapping[str, object],
    root: Path,
) -> None:
    corners = (
        ("low-reverse-bridge", 0.05, -1.0, 0.01),
        ("high-forward-fingerboard", 1.0, 1.0, 0.49),
    )
    for articulation, name in enumerate(ARTICULATION_NAMES):
        for string, frequency in enumerate(OPEN_FREQUENCIES, start=1):
            for corner, force, speed, position in corners:
                label = "{} string {} {}".format(name, string, corner)
                output = run(csound, module, csd, {
                    "TEST_CASE": 1,
                    "TEST_STRING": string,
                    "TEST_FREQUENCY": frequency,
                    "TEST_ARTICULATION": articulation,
                    "TEST_HARMONIC": 0,
                    "TEST_FORCE": force,
                    "TEST_SPEED": speed,
                    "TEST_POSITION": position,
                }, root / "bounds-{}-{}-{}.f64".format(
                    name, string, corner), label)
                bundle = probe_bundle(output)
                active_key = (1, string)
                release_key = (2, string)
                require_common_state(bundle, active_key, label)
                require_common_state(bundle, release_key, label)
                gesture = bundle["gesture"][active_key]
                released = bundle["gesture"][release_key]
                exciter = bundle["exciter"][active_key]
                bow = bundle["bow"][active_key]
                wave = bundle["wave"][active_key]
                if not close(gesture[3], force) or not close(
                        gesture[4], speed):
                    raise RuntimeError("{} changed its control target".format(label))
                if not (0.0 <= gesture[5] <= 1.0 and
                        -1.0 <= gesture[6] <= 1.0 and
                        0.0 <= gesture[7] <= 1.0):
                    raise RuntimeError("{} escaped its output bounds".format(label))
                expected_onset, expected_stroke = expected_gesture_timing(
                    model, articulation, speed)
                if (gesture[8], gesture[9]) != (
                        float(expected_onset), float(expected_stroke)):
                    raise RuntimeError("{} timing did not follow speed".format(label))
                if wave[0] != 1.0:
                    raise RuntimeError("{} did not remain one excitation".format(label))
                if released[15] != float(articulation):
                    raise RuntimeError("{} lost its release mode".format(label))
                expected_gain = expected_release_gain(
                    model, string, articulation, frequency, speed)
                if not close(released[16], expected_gain):
                    raise RuntimeError("{} release target changed".format(label))
                if articulation <= 4:
                    if gesture[1] != 1.0 or bow[4] != 1.0:
                        raise RuntimeError("{} did not run a finite bow".format(label))
                    if not close(bow[6], position) or not close(
                            bow[7], position, 2.0e-7):
                        raise RuntimeError("{} missed its bow position".format(label))
                else:
                    if gesture[1] != 0.0 or not close(exciter[3], position):
                        raise RuntimeError(
                            "{} did not keep point-exciter geometry".format(label))
                    if articulation == 5:
                        expected_normal = 0.0 if speed < 0.0 else 1.0
                        expected_left = 1.0 if speed < 0.0 else 0.0
                        if exciter[6] != expected_normal or \
                                exciter[7] != expected_left:
                            raise RuntimeError(
                                "{} selected the wrong pizzicato hand".format(label))


def check_same_handle_controls(
    csound: Path, module: Path, csd: Path, root: Path,
) -> None:
    label = "same-handle direct-control sequence"
    output = run(csound, module, csd, {
        "TEST_CASE": 4,
        "TEST_STRING": 2,
        "TEST_FREQUENCY": OPEN_FREQUENCIES[1],
    }, root / "control-sequence.f64", label)
    bundle = probe_bundle(output)
    expected = (
        (0.25, 0.25, 0.08, 0.0),
        (0.75, 0.25, 0.08, 1.0),
        (0.75, -0.65, 0.20, 2.0),
    )
    for tag, (force, speed, position, overrides) in enumerate(
            expected, start=1):
        key = (tag, 2)
        require_common_state(bundle, key, label)
        gesture = bundle["gesture"][key]
        bow = bundle["bow"][key]
        wave = bundle["wave"][key]
        if gesture[0] != 1.0 or gesture[1] != 1.0:
            raise RuntimeError("{} lost detaché state at tag {}".format(label, tag))
        if not close(gesture[3], force) or not close(gesture[4], speed):
            raise RuntimeError("{} missed controls at tag {}".format(label, tag))
        if gesture[13] != overrides:
            raise RuntimeError(
                "{} counted {} overrides at tag {}, wanted {}".format(
                    label, gesture[13], tag, overrides))
        if not close(bow[6], position) or not close(
                bow[7], position, 2.0e-7):
            raise RuntimeError("{} missed position at tag {}".format(label, tag))
        if wave[0] != 1.0 or bow[2] != 1.0:
            raise RuntimeError("{} replaced or retriggered state".format(label))
    if bundle["gesture"][(3, 2)][11] <= bundle["gesture"][(2, 2)][11]:
        raise RuntimeError("{} did not register its bow reversal".format(label))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csound", required=True, type=Path)
    parser.add_argument("--module", required=True, type=Path)
    parser.add_argument("--csd", required=True, type=Path)
    parser.add_argument("--model", required=True, type=Path)
    options = parser.parse_args()
    for path in (options.csound, options.module, options.csd, options.model):
        if not path.is_file():
            parser.error("missing regular file: {}".format(path))
    model = json.loads(options.model.read_text(encoding="utf-8"))

    try:
        with tempfile.TemporaryDirectory(prefix="hlolli-gestures-") as folder:
            root = Path(folder)
            check_articulation_matrix(
                options.csound, options.module, options.csd, model, root)
            check_harmonics(options.csound, options.module, options.csd, root)
            check_slurs(options.csound, options.module, options.csd, root)
            check_preset_sequence(
                options.csound, options.module, options.csd, root)
            check_control_bounds(
                options.csound, options.module, options.csd, model, root)
            check_same_handle_controls(
                options.csound, options.module, options.csd, root)
    except (OSError, RuntimeError, subprocess.TimeoutExpired, ValueError,
            OverflowError) as error:
        print("gesture matrix failed: {}".format(error), file=sys.stderr)
        return 1
    print(
        "double-bass gesture matrix passed: 36 articulation/string cases, "
        "four natural harmonics, four continuous slurs, same-handle bowed "
        "preset/control changes, 72 control-bound corners, and the separate "
        "chord regression")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
