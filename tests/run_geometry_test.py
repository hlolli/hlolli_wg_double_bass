#!/usr/bin/env python3
"""Check the fixed double-bass tuning, delay geometry, and playing range."""

from __future__ import annotations

import argparse
from array import array
import math
from pathlib import Path
import subprocess
import sys
import tempfile
from typing import Dict, List, Mapping, Optional, Tuple


SAMPLE_RATES = (44100, 48000, 88200, 96000)
BLOCK_SIZES = (1, 16, 32, 64)
OPEN_MIDI = (28, 33, 38, 43)
TOP_MIDI = 67
GLISS_END_MIDI = (55, 60, 65, 67)
RANGE_DIAGNOSTIC = "fundamental is outside the playable range"
HARMONIC_DSP_DIAGNOSTIC = "sounded harmonic is outside the DSP range"
PITCH_SNAP_EPSILONS = 8.0
SETTLED_POSITION_ABSOLUTE_ERROR = 1.0e-9
SETTLED_FREQUENCY_RELATIVE_ERROR = 1.0e-9
SETTLED_DELAY_ABSOLUTE_ERROR = 1.0e-9


def pitch(midi: int, a4: float = 440.0) -> float:
    """Return an independent 12-TET sounding pitch."""
    a440 = 440.0 * 2.0 ** ((midi - 69) / 12.0)
    return a4 * (a440 / 440.0)


def pitch_snap_guard(boundary: float) -> float:
    """Return the named binary64 snap radius for one pitch boundary."""
    return (PITCH_SNAP_EPSILONS * sys.float_info.epsilon *
            max(1.0, abs(boundary)))


def pitch_at_snap_edge(
    boundary: float, direction: float, inside: bool,
) -> float:
    """Return the last float inside or first float outside the snap guard."""
    toward = math.inf if direction > 0.0 else -math.inf
    radius = pitch_snap_guard(boundary)
    current = boundary
    while True:
        following = math.nextafter(current, toward)
        if abs(following - boundary) > radius:
            return current if inside else following
        current = following


def definitions(values: Mapping[str, object]) -> List[str]:
    return [
        "--omacro:{}={:.17g}".format(name, value)
        if isinstance(value, float)
        else "--omacro:{}={}".format(name, value)
        for name, value in values.items()
    ]


def csound_command(
    csound: Path,
    module: Path,
    csd: Path,
    values: Mapping[str, object],
) -> List[str]:
    return [
        str(csound),
        "--opcode-lib={}".format(module),
        "--sample-accurate",
        "--num-threads=1",
        *definitions(values),
    ]


def invoke_csound(
    csound: Path,
    module: Path,
    csd: Path,
    values: Mapping[str, object],
    output: Optional[Path] = None,
) -> subprocess.CompletedProcess[str]:
    command = csound_command(csound, module, csd, values)
    if output is None:
        command.append("-n")
    else:
        command.extend([
            "--format=raw",
            "--format=double",
            "--nopeaks",
            "-o",
            str(output),
        ])
    command.extend(["-d", "-m128", str(csd)])
    return subprocess.run(
        command,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=30,
    )


def run_csound(
    csound: Path,
    module: Path,
    csd: Path,
    values: Mapping[str, object],
    label: str,
) -> str:
    completed = invoke_csound(csound, module, csd, values)
    if completed.returncode != 0:
        raise RuntimeError(
            "{} failed with status {}:\n{}".format(
                label, completed.returncode, completed.stdout.rstrip()))
    return completed.stdout


def expect_rejection(
    csound: Path,
    module: Path,
    csd: Path,
    values: Mapping[str, object],
    label: str,
    diagnostic: str = RANGE_DIAGNOSTIC,
) -> None:
    completed = invoke_csound(csound, module, csd, values)
    if completed.returncode == 0:
        raise RuntimeError("{} was accepted".format(label))
    if diagnostic not in completed.stdout:
        raise RuntimeError(
            "{} lacked range diagnostic {!r}:\n{}".format(
                label, diagnostic, completed.stdout.rstrip()))


def render_csound(
    csound: Path,
    module: Path,
    csd: Path,
    values: Mapping[str, object],
    output: Path,
    label: str,
) -> str:
    completed = invoke_csound(csound, module, csd, values, output=output)
    if completed.returncode != 0:
        raise RuntimeError(
            "{} failed with status {}:\n{}".format(
                label, completed.returncode, completed.stdout.rstrip()))
    return completed.stdout


def read_pcm(path: Path, label: str) -> array:
    data = path.read_bytes()
    if not data or len(data) % 16 != 0:
        raise RuntimeError("{} returned malformed stereo float64 PCM".format(label))
    samples = array("d")
    samples.frombytes(data)
    if any(not math.isfinite(sample) for sample in samples):
        raise RuntimeError("{} returned non-finite PCM".format(label))
    peak = max(abs(sample) for sample in samples)
    active = sum(abs(sample) > 1.0e-15 for sample in samples)
    if peak < 1.0e-10 or active < 64:
        raise RuntimeError("{} returned silent PCM".format(label))
    if peak >= 1.0:
        raise RuntimeError(
            "{} PCM was not bounded: peak {:.17g}".format(label, peak))
    return samples


def compare_pcm(label: str, reference: array, actual: array) -> None:
    if len(reference) != len(actual):
        raise RuntimeError(
            "{} sample counts differ: {} != {}".format(
                label, len(reference), len(actual)))
    if reference != actual:
        maximum = max(
            abs(wanted - found)
            for wanted, found in zip(reference, actual))
        raise RuntimeError(
            "{} PCM differs: max={:.17g}".format(label, maximum))


def records(output: str, marker: str) -> Dict[Tuple[int, int], Tuple[float, ...]]:
    found: Dict[Tuple[int, int], Tuple[float, ...]] = {}
    prefix = marker + " "
    for line in output.splitlines():
        start = line.find(prefix)
        if start < 0:
            continue
        words = line[start:].split()[1:]
        if len(words) < 3:
            continue
        tag = int(float(words[0]))
        string = int(float(words[1]))
        found[(tag, string)] = tuple(float(word) for word in words[2:])
    return found


def close(found: float, wanted: float, tolerance: float = 2.0e-11) -> bool:
    return math.isclose(found, wanted, rel_tol=tolerance, abs_tol=tolerance)


def require_close(label: str, found: float, wanted: float) -> None:
    if not close(found, wanted):
        raise RuntimeError(
            "{} was {:.17g}, wanted {:.17g}".format(label, found, wanted))


def require_exact(label: str, found: float, wanted: float) -> None:
    if found != wanted:
        raise RuntimeError(
            "{} was {:.17g}, wanted exact {:.17g}".format(
                label, found, wanted))


def require_settled(
    label: str,
    found: float,
    wanted: float,
    absolute_error: float,
    relative_error: float = 0.0,
) -> None:
    limit = absolute_error + relative_error * abs(wanted)
    error = abs(found - wanted)
    if error > limit:
        raise RuntimeError(
            "{} was {:.17g}, wanted {:.17g}; error {:.3g} exceeds {:.3g}"
            .format(label, found, wanted, error, limit))


def one_record(
    output: str, marker: str, tag: int, string: int, size: int,
) -> Tuple[float, ...]:
    value = records(output, marker).get((tag, string))
    if value is None or len(value) != size:
        raise RuntimeError(
            "{} tag {} string {} returned no {}-field record".format(
                marker, tag, string, size))
    if any(not math.isfinite(item) for item in value):
        raise RuntimeError(
            "{} tag {} string {} returned non-finite facts".format(
                marker, tag, string))
    return value


def check_exact_pitch_record(
    output: str,
    label: str,
    tag: int,
    string: int,
    sounding_frequency: float,
    fundamental_frequency: float,
) -> None:
    state = one_record(output, "WG_STRING", tag, string, 7)
    wave = one_record(output, "WG_WAVE", tag, string, 7)
    harmonic = one_record(output, "WG_HARMONIC", tag, string, 9)
    require_exact(label + " state pitch", state[1], sounding_frequency)
    require_exact(label + " waveguide pitch", wave[3], fundamental_frequency)
    require_exact(
        label + " harmonic sounding pitch", harmonic[4], sounding_frequency)
    require_exact(
        label + " harmonic fundamental", harmonic[5],
        fundamental_frequency)


def check_geometry_record(
    output: str,
    sample_rate: int,
    string: int,
    wanted_frequency: float,
    position: float = 0.12,
    tag: int = 1,
    lowest_open: Optional[float] = None,
) -> None:
    finger = one_record(output, "WG_FINGER", tag, string, 5)
    capacity, read_delay, target_delay, target_frequency, loop_phase, _, finite = (
        one_record(output, "WG_WAVE", tag, string, 7))
    target_position, effective_position, bridge_delay, nut_delay, bow_finite = (
        one_record(output, "WG_BOW_GEOMETRY", tag, string, 5))

    require_close("string {} open frequency".format(string), finger[0],
                  wanted_frequency)
    require_close("string {} waveguide frequency".format(string),
                  target_frequency, wanted_frequency)
    require_close("string {} loop phase".format(string), loop_phase,
                  sample_rate / wanted_frequency)
    require_close("string {} finger current position".format(string),
                  finger[2], finger[1])
    require_close("string {} effective frequency".format(string),
                  finger[3], wanted_frequency)
    require_close("string {} current delay".format(string),
                  read_delay, target_delay)
    require_close("string {} bow target coordinate".format(string),
                  target_position, position)
    require_close("string {} bow split coordinate".format(string),
                  effective_position, position)
    require_close("string {} split delay".format(string),
                  bridge_delay + nut_delay, target_delay)
    require_close("string {} bridge delay ratio".format(string),
                  bridge_delay / (bridge_delay + nut_delay), position)

    if finger[4] != 1.0 or finite != 1.0 or bow_finite != 1.0:
        raise RuntimeError("string {} geometry was not finite".format(string))
    if lowest_open is None:
        lowest_open = pitch(OPEN_MIDI[0])
    wanted_capacity = math.ceil(sample_rate / lowest_open) + 8
    if capacity != wanted_capacity:
        raise RuntimeError(
            "string {} rail capacity was {}, wanted ceil({}/{:.17g})+8 = {}"
            .format(
                string, capacity, sample_rate, lowest_open, wanted_capacity))
    for name, value, low in (
            ("read delay", read_delay, 2.0),
            ("target delay", target_delay, 2.0),
            ("bridge delay", bridge_delay, 1.0),
            ("nut delay", nut_delay, 1.0)):
        if value < low or value > capacity - 2.0:
            raise RuntimeError(
                "string {} {} {} is outside [{}, {}]".format(
                    string, name, value, low, capacity - 2.0))


def geometry_values(
    sample_rate: int, block_size: int, case: int = 1, a4: float = 440.0,
) -> Dict[str, object]:
    opens = [pitch(midi, a4) for midi in OPEN_MIDI]
    return {
        "TEST_SR": sample_rate,
        "TEST_KSMPS": block_size,
        "TEST_A4": a4,
        "TEST_CASE": case,
        "TEST_FREQUENCY1": opens[0],
        "TEST_FREQUENCY2": opens[1],
        "TEST_FREQUENCY3": opens[2],
        "TEST_FREQUENCY4": opens[3],
    }


def check_e1_matrix(csound: Path, module: Path, csd: Path) -> None:
    wanted = pitch(OPEN_MIDI[0])
    for sample_rate in SAMPLE_RATES:
        for block_size in BLOCK_SIZES:
            values = geometry_values(sample_rate, block_size)
            values.update({
                "TEST_STRING": 1,
                "TEST_PROBE_STRING": 1,
                "TEST_FREQUENCY": wanted,
            })
            label = "E1 geometry at {} Hz, ksmps {}".format(
                sample_rate, block_size)
            output = run_csound(csound, module, csd, values, label)
            check_geometry_record(output, sample_rate, 1, wanted)


def check_all_opens(csound: Path, module: Path, csd: Path) -> None:
    wanted = [pitch(midi) for midi in OPEN_MIDI]
    for sample_rate in SAMPLE_RATES:
        for block_size in BLOCK_SIZES:
            values = geometry_values(sample_rate, block_size, case=2)
            label = "four open strings at {} Hz, ksmps {}".format(
                sample_rate, block_size)
            output = run_csound(csound, module, csd, values, label)
            for string, frequency in enumerate(wanted, start=1):
                check_geometry_record(
                    output, sample_rate, string, frequency, tag=string,
                    lowest_open=wanted[0])


def check_tuned_opens(csound: Path, module: Path, csd: Path) -> None:
    sample_rate = 48000
    block_size = 32
    a4 = 442.0
    wanted = [pitch(midi, a4) for midi in OPEN_MIDI]
    values = geometry_values(
        sample_rate, block_size, case=2, a4=a4)
    output = run_csound(
        csound, module, csd, values, "A4=442 four open strings")
    for string, frequency in enumerate(wanted, start=1):
        check_geometry_record(
            output, sample_rate, string, frequency, tag=string,
            lowest_open=wanted[0])


def check_reference_extreme_matrix(
    csound: Path, module: Path, csd: Path,
) -> None:
    for a4 in (380.0, 480.0):
        opens = [pitch(midi, a4) for midi in OPEN_MIDI]
        top = pitch(TOP_MIDI, a4)
        for sample_rate in SAMPLE_RATES:
            for block_size in BLOCK_SIZES:
                values = geometry_values(
                    sample_rate, block_size, case=2, a4=a4)
                for index in range(1, 5):
                    values["TEST_FREQUENCY{}".format(index)] = top
                label = "A4={} G4 geometry at {} Hz, ksmps {}".format(
                    a4, sample_rate, block_size)
                output = run_csound(csound, module, csd, values, label)
                wanted_capacity = math.ceil(sample_rate / opens[0]) + 8
                for string, open_frequency in enumerate(opens, start=1):
                    check_normal_stopped_record(
                        output, sample_rate, string, open_frequency, top,
                        tag=string)
                    wave = one_record(
                        output, "WG_WAVE", string, string, 7)
                    if wave[0] != wanted_capacity:
                        raise RuntimeError(
                            "{} string {} capacity was {}, wanted {}".format(
                                label, string, wave[0], wanted_capacity))
                    for name, value, low in (
                            ("read delay", wave[1], 2.0),
                            ("target delay", wave[2], 2.0)):
                        if value < low or value > wave[0] - 2.0:
                            raise RuntimeError(
                                "{} string {} {} {} was out of bounds".format(
                                    label, string, name, value))


def check_contact_split_corners(
    csound: Path, module: Path, csd: Path,
) -> None:
    corners = (
        (44100, 64, 480.0, 4, pitch(TOP_MIDI, 480.0),
         "44.1 kHz/A4=480/G4"),
        (96000, 1, 380.0, 1, pitch(OPEN_MIDI[0], 380.0),
         "96 kHz/A4=380/E1"),
    )
    positions = (0.01, 0.12, 0.49)
    for sample_rate, block_size, a4, string, frequency, label in corners:
        opens = [pitch(midi, a4) for midi in OPEN_MIDI]
        bridge_delays = []
        nut_delays = []
        for position in positions:
            values = geometry_values(sample_rate, block_size, a4=a4)
            values.update({
                "TEST_STRING": string,
                "TEST_PROBE_STRING": string,
                "TEST_FREQUENCY": frequency,
                "TEST_POSITION": position,
            })
            output = run_csound(
                csound, module, csd, values,
                "{} beta={}".format(label, position))
            check_normal_stopped_record(
                output, sample_rate, string, opens[string - 1], frequency,
                position=position)
            wave = one_record(output, "WG_WAVE", 1, string, 7)
            bow = one_record(output, "WG_BOW_GEOMETRY", 1, string, 5)
            wanted_capacity = math.ceil(sample_rate / opens[0]) + 8
            if wave[0] != wanted_capacity:
                raise RuntimeError(
                    "{} beta={} capacity was {}, wanted {}".format(
                        label, position, wave[0], wanted_capacity))
            if not bow[2] < bow[3]:
                raise RuntimeError(
                    "{} beta={} reversed bridge/nut split: {} >= {}".format(
                        label, position, bow[2], bow[3]))
            bridge_delays.append(bow[2])
            nut_delays.append(bow[3])
        if not (bridge_delays[0] < bridge_delays[1] < bridge_delays[2]):
            raise RuntimeError(
                "{} bridge delay did not rise with beta: {}".format(
                    label, bridge_delays))
        if not (nut_delays[0] > nut_delays[1] > nut_delays[2]):
            raise RuntimeError(
                "{} nut delay did not fall with beta: {}".format(
                    label, nut_delays))


def check_nonfinite_frequency_fallback(
    csound: Path, module: Path, csd: Path,
) -> None:
    opens = [pitch(midi) for midi in OPEN_MIDI]
    top = pitch(TOP_MIDI)

    def check_record(
        output: str, label: str, string: int, request: float, tag: int,
    ) -> None:
        open_frequency = opens[string - 1]
        order = (
            int(request)
            if request == int(request) and 2 <= int(request) <= 8
            else 0
        )
        sounding = open_frequency * (order if order else 1)
        state = one_record(output, "WG_STRING", tag, string, 7)
        require_close(label + " sounding pitch", state[1], sounding)
        if order:
            check_harmonic_record(
                output, 48000, string, open_frequency, sounding,
                order, True, top, tag=tag)
            return

        check_geometry_record(
            output, 48000, string, open_frequency, tag=tag,
            lowest_open=opens[0])
        harmonic = one_record(output, "WG_HARMONIC", tag, string, 9)
        wanted = (
            request, 0.0, 1.0 if request == 0.0 else 0.0, 0.0,
            open_frequency, open_frequency,
            0.0, 0.0, 1.0,
        )
        for name, found, expected in zip(
                ("request", "active order", "valid", "natural",
                 "sounding pitch", "fundamental", "stop", "touch",
                 "finite"), harmonic, wanted):
            require_close(label + " " + name, found, expected)

    for request, name in (
            (0.0, "normal"), (4.0, "order 4"), (9.0, "invalid order 9")):
        string = 3
        values = geometry_values(48000, 32)
        values.update({
            "TEST_STRING": string,
            "TEST_PROBE_STRING": string,
            "TEST_FREQUENCY": "log(-1)",
            "TEST_HARMONIC": request,
        })
        label = "explicit string 3 NaN {} fallback".format(name)
        output = run_csound(csound, module, csd, values, label)
        check_record(output, label, string, request, tag=1)

        automatic = geometry_values(48000, 32, case=3)
        automatic.update({
            "TEST_FREQUENCY": "log(-1)",
            "TEST_HARMONIC": request,
        })
        label = "initial automatic NaN {} E-string fallback".format(name)
        output = run_csound(csound, module, csd, automatic, label)
        selected = selected_auto_string(output, label)
        if selected != 1:
            raise RuntimeError(
                "{} chose string {}, wanted 1".format(label, selected))
        check_record(output, label, 1, request, tag=1)

        continuous = geometry_values(48000, 32, case=9)
        continuous.update({
            "TEST_FREQUENCY": opens[2] * (request if request == 4.0 else 1.0),
            "TEST_FREQUENCY2": "log(-1)",
            "TEST_HARMONIC": request,
        })
        label = "continuous automatic D-string NaN {} fallback".format(name)
        output = run_csound(csound, module, csd, continuous, label)
        selected = selected_auto_string(output, label)
        if selected != 3:
            raise RuntimeError(
                "{} chose string {}, wanted retained string 3".format(
                    label, selected))
        check_record(output, label, 3, request, tag=3)

    competing = geometry_values(48000, 32, case=10)
    competing.update({
        "TEST_FREQUENCY1": opens[1],
        "TEST_FREQUENCY2": opens[2],
        "TEST_FREQUENCY3": "log(-1)",
        "TEST_FREQUENCY4": opens[2],
    })
    label = "automatic NaN tail jump keeps D string"
    output = run_csound(csound, module, csd, competing, label)
    selected = selected_auto_string(output, label)
    if selected != 3:
        raise RuntimeError(
            "{} chose string {}, wanted 3".format(label, selected))
    check_geometry_record(
        output, 48000, 3, opens[2], tag=3, lowest_open=opens[0])


def selected_auto_string(output: str, label: str) -> int:
    states = records(output, "WG_STRING")
    owners = []
    for string in range(1, 5):
        state = states.get((string, string))
        if state is None or len(state) != 7:
            raise RuntimeError(
                "{} returned no string {} owner state".format(label, string))
        if state[5] == 1.0:
            owners.append(string)
        elif state[5] != 0.0:
            raise RuntimeError(
                "{} returned invalid string {} owner {}".format(
                    label, string, state[5]))
    if len(owners) != 1:
        raise RuntimeError(
            "{} owned strings {}, wanted exactly one".format(label, owners))
    return owners[0]


def check_auto_selection(csound: Path, module: Path, csd: Path) -> None:
    for a4 in (440.0, 442.0):
        opens = [pitch(midi, a4) for midi in OPEN_MIDI]
        cases = [(opens[0], 1)]
        for string in range(2, 5):
            boundary = opens[string - 1]
            cases.extend([
                (pitch_at_snap_edge(boundary, -1.0, False), string - 1),
                (pitch_at_snap_edge(boundary, -1.0, True), string),
                (boundary, string),
            ])
        cases.append((pitch(TOP_MIDI, a4), 4))
        for number, (frequency, wanted_string) in enumerate(cases, start=1):
            values = geometry_values(48000, 32, case=3, a4=a4)
            values.update({
                "TEST_FREQUENCY": frequency,
                "TEST_HARMONIC": 0,
            })
            label = "A4={} auto boundary {}".format(a4, number)
            output = run_csound(csound, module, csd, values, label)
            found = selected_auto_string(output, label)
            if found != wanted_string:
                raise RuntimeError(
                    "{} chose string {}, wanted {}".format(
                        label, found, wanted_string))


def check_continuous_auto_boundary_handoffs(
    csound: Path, module: Path, csd: Path,
) -> None:
    opens = [pitch(midi) for midi in OPEN_MIDI]
    for upper_string in range(2, 5):
        lower_string = upper_string - 1
        boundary = opens[upper_string - 1]
        below = pitch_at_snap_edge(boundary, -1.0, False)

        isolated_below = geometry_values(48000, 32, case=3)
        isolated_below["TEST_FREQUENCY"] = below
        below_label = "isolated auto below string {} boundary".format(
            upper_string)
        below_output = run_csound(
            csound, module, csd, isolated_below, below_label)
        found = selected_auto_string(below_output, below_label)
        if found != lower_string:
            raise RuntimeError(
                "{} chose string {}, wanted {}".format(
                    below_label, found, lower_string))

        isolated_exact = geometry_values(48000, 32, case=3)
        isolated_exact["TEST_FREQUENCY"] = boundary
        exact_label = "isolated auto at string {} boundary".format(
            upper_string)
        exact_output = run_csound(
            csound, module, csd, isolated_exact, exact_label)
        found = selected_auto_string(exact_output, exact_label)
        if found != upper_string:
            raise RuntimeError(
                "{} chose string {}, wanted {}".format(
                    exact_label, found, upper_string))

        continuous = geometry_values(48000, 32, case=8)
        continuous.update({
            "TEST_FREQUENCY": below,
            "TEST_FREQUENCY2": boundary,
        })
        continuous_label = (
            "continuous auto crossing string {} boundary".format(
                upper_string))
        continuous_output = run_csound(
            csound, module, csd, continuous, continuous_label)
        found = selected_auto_string(continuous_output, continuous_label)
        if found != lower_string:
            raise RuntimeError(
                "{} chose string {}, wanted retained string {}".format(
                    continuous_label, found, lower_string))
        state = one_record(
            continuous_output, "WG_STRING",
            lower_string, lower_string, 7)
        require_close(
            continuous_label + " current pitch", state[1], boundary)
        check_normal_stopped_record(
            continuous_output, 48000, lower_string,
            opens[lower_string - 1], boundary, tag=lower_string)


def check_normal_stopped_record(
    output: str,
    sample_rate: int,
    string: int,
    open_frequency: float,
    sounding_frequency: float,
    tag: int = 1,
    position: float = 0.12,
) -> None:
    finger = one_record(output, "WG_FINGER", tag, string, 5)
    wave = one_record(output, "WG_WAVE", tag, string, 7)
    bow = one_record(output, "WG_BOW_GEOMETRY", tag, string, 5)
    harmonic = one_record(output, "WG_HARMONIC", tag, string, 9)
    wanted_stop = 1.0 - open_frequency / sounding_frequency
    require_close("string {} stopped open".format(string), finger[0],
                  open_frequency)
    require_close("string {} stopped target".format(string), finger[1],
                  wanted_stop)
    require_close("string {} stopped wave pitch".format(string), wave[3],
                  sounding_frequency)
    require_close("string {} stopped loop phase".format(string), wave[4],
                  sample_rate / sounding_frequency)
    require_settled(
        "string {} settled finger position".format(string),
        finger[2], finger[1], SETTLED_POSITION_ABSOLUTE_ERROR)
    require_settled(
        "string {} settled effective frequency".format(string),
        finger[3], sounding_frequency, 0.0,
        SETTLED_FREQUENCY_RELATIVE_ERROR)
    require_settled(
        "string {} settled delay".format(string),
        wave[1], wave[2], SETTLED_DELAY_ABSOLUTE_ERROR)
    require_close("string {} stopped bridge coordinate".format(string),
                  bow[1], position)
    require_close("string {} stopped split delay".format(string),
                  bow[2] + bow[3], wave[2])
    require_close("string {} stopped bridge delay ratio".format(string),
                  bow[2] / (bow[2] + bow[3]), position)
    if harmonic[0:4] != (0.0, 0.0, 1.0, 0.0):
        raise RuntimeError(
            "string {} normal stop entered harmonic mode".format(string))
    if (finger[4] != 1.0 or wave[6] != 1.0 or bow[4] != 1.0 or
            harmonic[8] != 1.0):
        raise RuntimeError("string {} stopped geometry was not finite".format(string))


def check_normal_range(csound: Path, module: Path, csd: Path) -> None:
    for a4 in (440.0, 442.0):
        opens = [pitch(midi, a4) for midi in OPEN_MIDI]
        top = pitch(TOP_MIDI, a4)
        for string, open_frequency in enumerate(opens, start=1):
            values = geometry_values(48000, 32, a4=a4)
            values.update({
                "TEST_STRING": string,
                "TEST_PROBE_STRING": string,
                "TEST_FREQUENCY": top,
                "TEST_HARMONIC": 0,
            })
            label = "A4={} string {} inclusive G4".format(a4, string)
            output = run_csound(csound, module, csd, values, label)
            check_normal_stopped_record(
                output, 48000, string, open_frequency, top)

            below = dict(values)
            below["TEST_FREQUENCY"] = pitch_at_snap_edge(
                open_frequency, -1.0, False)
            expect_rejection(
                csound, module, csd, below,
                "A4={} string {} pitch below its open".format(a4, string))

            above = dict(values)
            above["TEST_FREQUENCY"] = pitch_at_snap_edge(
                top, 1.0, False)
            expect_rejection(
                csound, module, csd, above,
                "A4={} string {} pitch above G4".format(a4, string))

        for frequency, name in (
                (pitch_at_snap_edge(opens[0], -1.0, False), "below E1"),
                (pitch_at_snap_edge(top, 1.0, False), "above G4")):
            values = geometry_values(48000, 32, case=3, a4=a4)
            values["TEST_FREQUENCY"] = frequency
            expect_rejection(
                csound, module, csd, values,
                "A4={} automatic pitch {}".format(a4, name))


def check_harmonic_record(
    output: str,
    sample_rate: int,
    string: int,
    open_frequency: float,
    sounding_frequency: float,
    order: int,
    natural: bool,
    top: float,
    tag: int = 1,
) -> None:
    q = sounding_frequency / order
    harmonic = one_record(output, "WG_HARMONIC", tag, string, 9)
    finger = one_record(output, "WG_FINGER", tag, string, 5)
    wave = one_record(output, "WG_WAVE", tag, string, 7)
    wanted_stop = 0.0 if natural else 1.0 - open_frequency / q
    wanted_touch = (
        1.0 / order if natural
        else wanted_stop + (open_frequency / q) / order)
    wanted = (
        float(order), float(order), 1.0, float(natural),
        sounding_frequency, q, wanted_stop, wanted_touch, 1.0,
    )
    names = (
        "request", "active order", "valid", "natural", "sounding pitch",
        "fundamental q", "stop coordinate", "touch coordinate", "finite",
    )
    for name, found, expected in zip(names, harmonic, wanted):
        require_close(
            "string {} harmonic {} {}".format(string, order, name),
            found, expected)
    require_close("harmonic finger stop", finger[1], wanted_stop)
    require_close("harmonic waveguide q", wave[3], q)
    require_close("harmonic loop phase", wave[4], sample_rate / q)
    if finger[4] != 1.0 or wave[6] != 1.0:
        raise RuntimeError(
            "string {} harmonic {} geometry was not finite".format(
                string, order))
    capacity = wave[0]
    for name, value, low in (
            ("read delay", wave[1], 2.0),
            ("target delay", wave[2], 2.0)):
        if value < low or value > capacity - 2.0:
            raise RuntimeError(
                "string {} harmonic {} {} {} is out of bounds".format(
                    string, order, name, value))
    dsp_ceiling = min(8.0 * top, 0.20 * sample_rate)
    if sounding_frequency > dsp_ceiling * (1.0 + 1.0e-12):
        raise RuntimeError(
            "test requested harmonic {} above DSP ceiling {}".format(
                sounding_frequency, dsp_ceiling))


def harmonic_output(
    csound: Path,
    module: Path,
    csd: Path,
    a4: float,
    string: int,
    sounding: float,
    order: int,
    label: str,
) -> str:
    values = geometry_values(48000, 32, a4=a4)
    values.update({
        "TEST_STRING": string,
        "TEST_PROBE_STRING": string,
        "TEST_FREQUENCY": sounding,
        "TEST_HARMONIC": order,
    })
    return run_csound(csound, module, csd, values, label)


def check_harmonics(csound: Path, module: Path, csd: Path) -> None:
    a4 = 440.0
    opens = [pitch(midi, a4) for midi in OPEN_MIDI]
    top = pitch(TOP_MIDI, a4)
    artificial_q = opens[1]
    for order in range(2, 9):
        for string, natural_open in enumerate(opens, start=1):
            natural_sounding = order * natural_open
            label = "natural string {} harmonic {}".format(string, order)
            output = harmonic_output(
                csound, module, csd, a4, string, natural_sounding,
                order, label)
            check_harmonic_record(
                output, 48000, string, natural_open, natural_sounding,
                order, True, top)

        artificial_sounding = order * artificial_q
        label = "artificial E-string harmonic {}".format(order)
        output = harmonic_output(
            csound, module, csd, a4, 1, artificial_sounding, order, label)
        check_harmonic_record(
            output, 48000, 1, opens[0], artificial_sounding, order,
            False, top)

    ceiling_sounding = 8.0 * top
    output = harmonic_output(
        csound, module, csd, a4, 4, ceiling_sounding, 8,
        "inclusive 8*G4 harmonic ceiling")
    check_harmonic_record(
        output, 48000, 4, opens[3], ceiling_sounding, 8, False, top)

    for a4 in (440.0, 442.0):
        opens = [pitch(midi, a4) for midi in OPEN_MIDI]
        top = pitch(TOP_MIDI, a4)
        order = 5
        q = opens[2]
        sounding = order * q
        values = geometry_values(48000, 32, case=3, a4=a4)
        values.update({
            "TEST_FREQUENCY": sounding,
            "TEST_HARMONIC": order,
        })
        label = "A4={} automatic harmonic selection".format(a4)
        auto_output = run_csound(csound, module, csd, values, label)
        found = selected_auto_string(auto_output, label)
        if found != 3:
            raise RuntimeError(
                "{} chose string {}, wanted 3".format(label, found))
        check_harmonic_record(
            auto_output, 48000, 3, opens[2], sounding, order, True, top,
            tag=3,
        )

        artificial_q = opens[1]
        artificial_sounding = order * artificial_q
        tuned_output = harmonic_output(
            csound, module, csd, a4, 1, artificial_sounding, order,
            "A4={} artificial harmonic geometry".format(a4))
        check_harmonic_record(
            tuned_output, 48000, 1, opens[0], artificial_sounding,
            order, False, top)


def check_csound_pitch_snap(
    csound: Path, module: Path, csd: Path,
) -> None:
    sample_rate = 48000
    block_size = 32
    for a4 in (440.0, 442.0):
        opens = [pitch(midi, a4) for midi in OPEN_MIDI]
        top = pitch(TOP_MIDI, a4)

        explicit = geometry_values(
            sample_rate, block_size, case=2, a4=a4)
        for string, midi in enumerate(OPEN_MIDI, start=1):
            explicit["TEST_FREQUENCY{}".format(string)] = (
                "cpsmidinn({})".format(midi))
        label = "A4={} Csound explicit open expressions".format(a4)
        output = run_csound(csound, module, csd, explicit, label)
        for string, open_frequency in enumerate(opens, start=1):
            check_geometry_record(
                output, sample_rate, string, open_frequency, tag=string,
                lowest_open=opens[0])
            check_exact_pitch_record(
                output, label, string, string,
                open_frequency, open_frequency)

        explicit_top = geometry_values(
            sample_rate, block_size, case=2, a4=a4)
        for string in range(1, 5):
            explicit_top["TEST_FREQUENCY{}".format(string)] = (
                "cpsmidinn({})".format(TOP_MIDI))
        label = "A4={} Csound explicit G4 expressions".format(a4)
        output = run_csound(csound, module, csd, explicit_top, label)
        for string, open_frequency in enumerate(opens, start=1):
            check_normal_stopped_record(
                output, sample_rate, string, open_frequency, top,
                tag=string)
            check_exact_pitch_record(
                output, label, string, string, top, top)

        for wanted_string, midi in enumerate(OPEN_MIDI, start=1):
            values = geometry_values(
                sample_rate, block_size, case=3, a4=a4)
            values["TEST_FREQUENCY"] = "cpsmidinn({})".format(midi)
            label = "A4={} Csound automatic MIDI {} boundary".format(
                a4, midi)
            output = run_csound(csound, module, csd, values, label)
            selected = selected_auto_string(output, label)
            if selected != wanted_string:
                raise RuntimeError(
                    "{} chose string {}, wanted {}".format(
                        label, selected, wanted_string))
            check_geometry_record(
                output, sample_rate, wanted_string,
                opens[wanted_string - 1], tag=wanted_string,
                lowest_open=opens[0])
            check_exact_pitch_record(
                output, label, wanted_string, wanted_string,
                opens[wanted_string - 1], opens[wanted_string - 1])

        values = geometry_values(
            sample_rate, block_size, case=3, a4=a4)
        values["TEST_FREQUENCY"] = "cpsmidinn({})".format(TOP_MIDI)
        label = "A4={} Csound automatic G4 boundary".format(a4)
        output = run_csound(csound, module, csd, values, label)
        selected = selected_auto_string(output, label)
        if selected != 4:
            raise RuntimeError(
                "{} chose string {}, wanted 4".format(label, selected))
        check_normal_stopped_record(
            output, sample_rate, 4, opens[3], top, tag=4)
        check_exact_pitch_record(output, label, 4, 4, top, top)

        for order in range(2, 9):
            for string, (midi, open_frequency) in enumerate(
                    zip(OPEN_MIDI, opens), start=1):
                sounded = order * open_frequency
                values = geometry_values(sample_rate, block_size, a4=a4)
                values.update({
                    "TEST_STRING": string,
                    "TEST_PROBE_STRING": string,
                    "TEST_FREQUENCY": "{}*cpsmidinn({})".format(
                        order, midi),
                    "TEST_HARMONIC": order,
                })
                label = (
                    "A4={} Csound string {} natural harmonic {}"
                    .format(a4, string, order))
                output = run_csound(csound, module, csd, values, label)
                check_harmonic_record(
                    output, sample_rate, string, open_frequency, sounded,
                    order, True, top)
                check_exact_pitch_record(
                    output, label, 1, string, sounded, open_frequency)

        high_order = 8
        values = geometry_values(sample_rate, block_size, a4=a4)
        values.update({
            "TEST_STRING": 4,
            "TEST_PROBE_STRING": 4,
            "TEST_FREQUENCY": "8*cpsmidinn({})".format(TOP_MIDI),
            "TEST_HARMONIC": high_order,
        })
        label = "A4={} Csound 8th-order G4 ceiling".format(a4)
        output = run_csound(csound, module, csd, values, label)
        check_harmonic_record(
            output, sample_rate, 4, opens[3], high_order * top,
            high_order, False, top)
        check_exact_pitch_record(
            output, label, 1, 4, high_order * top, top)

        for string, open_frequency in enumerate(opens, start=1):
            inside = pitch_at_snap_edge(open_frequency, -1.0, True)
            values = geometry_values(
                sample_rate, block_size, case=3, a4=a4)
            values["TEST_FREQUENCY"] = inside
            label = "A4={} normal string {} inside snap guard".format(
                a4, string)
            output = run_csound(csound, module, csd, values, label)
            selected = selected_auto_string(output, label)
            if selected != string:
                raise RuntimeError(
                    "{} chose string {}, wanted {}".format(
                        label, selected, string))
            check_geometry_record(
                output, sample_rate, string, open_frequency, tag=string,
                lowest_open=opens[0])
            check_exact_pitch_record(
                output, label, string, string,
                open_frequency, open_frequency)

            outside = pitch_at_snap_edge(open_frequency, -1.0, False)
            values["TEST_FREQUENCY"] = outside
            label = "A4={} normal string {} outside snap guard".format(
                a4, string)
            if string == 1:
                expect_rejection(csound, module, csd, values, label)
            else:
                output = run_csound(csound, module, csd, values, label)
                selected = selected_auto_string(output, label)
                if selected != string - 1:
                    raise RuntimeError(
                        "{} chose string {}, wanted lower string {}".format(
                            label, selected, string - 1))
                check_normal_stopped_record(
                    output, sample_rate, string - 1, opens[string - 2],
                    outside, tag=string - 1)

        inside_top = pitch_at_snap_edge(top, 1.0, True)
        values = geometry_values(
            sample_rate, block_size, case=3, a4=a4)
        values["TEST_FREQUENCY"] = inside_top
        label = "A4={} normal G4 inside snap guard".format(a4)
        output = run_csound(csound, module, csd, values, label)
        selected = selected_auto_string(output, label)
        if selected != 4:
            raise RuntimeError(
                "{} chose string {}, wanted 4".format(label, selected))
        check_normal_stopped_record(
            output, sample_rate, 4, opens[3], top, tag=4)
        check_exact_pitch_record(output, label, 4, 4, top, top)

        outside_top = pitch_at_snap_edge(top, 1.0, False)
        values["TEST_FREQUENCY"] = outside_top
        expect_rejection(
            csound, module, csd, values,
            "A4={} normal G4 outside snap guard".format(a4))

        order = 8
        for string, open_frequency in enumerate(opens, start=1):
            sounded = order * open_frequency
            inside = pitch_at_snap_edge(sounded, -1.0, True)
            values = geometry_values(
                sample_rate, block_size, case=3, a4=a4)
            values.update({
                "TEST_FREQUENCY": inside,
                "TEST_HARMONIC": order,
            })
            label = "A4={} harmonic string {} inside snap guard".format(
                a4, string)
            output = run_csound(csound, module, csd, values, label)
            selected = selected_auto_string(output, label)
            if selected != string:
                raise RuntimeError(
                    "{} chose string {}, wanted {}".format(
                        label, selected, string))
            check_harmonic_record(
                output, sample_rate, string, open_frequency, sounded,
                order, True, top, tag=string)
            check_exact_pitch_record(
                output, label, string, string, sounded, open_frequency)

            outside = pitch_at_snap_edge(sounded, -1.0, False)
            values["TEST_FREQUENCY"] = outside
            label = "A4={} harmonic string {} outside snap guard".format(
                a4, string)
            if string == 1:
                expect_rejection(csound, module, csd, values, label)
            else:
                output = run_csound(csound, module, csd, values, label)
                selected = selected_auto_string(output, label)
                if selected != string - 1:
                    raise RuntimeError(
                        "{} chose string {}, wanted lower string {}".format(
                            label, selected, string - 1))
                check_harmonic_record(
                    output, sample_rate, string - 1, opens[string - 2],
                    outside, order, False, top, tag=string - 1)

        sounded_top = order * top
        inside = pitch_at_snap_edge(sounded_top, 1.0, True)
        values = geometry_values(
            sample_rate, block_size, case=3, a4=a4)
        values.update({
            "TEST_FREQUENCY": inside,
            "TEST_HARMONIC": order,
        })
        label = "A4={} harmonic G4 inside snap guard".format(a4)
        output = run_csound(csound, module, csd, values, label)
        selected = selected_auto_string(output, label)
        if selected != 4:
            raise RuntimeError(
                "{} chose string {}, wanted 4".format(label, selected))
        check_harmonic_record(
            output, sample_rate, 4, opens[3], sounded_top,
            order, False, top, tag=4)
        check_exact_pitch_record(
            output, label, 4, 4, sounded_top, top)

        outside = pitch_at_snap_edge(sounded_top, 1.0, False)
        values["TEST_FREQUENCY"] = outside
        expect_rejection(
            csound, module, csd, values,
            "A4={} harmonic G4 outside snap guard".format(a4))


def check_harmonic_range(csound: Path, module: Path, csd: Path) -> None:
    for a4 in (440.0, 442.0):
        opens = [pitch(midi, a4) for midi in OPEN_MIDI]
        top = pitch(TOP_MIDI, a4)
        for string, open_frequency in enumerate(opens, start=1):
            ceiling_sounding = 8.0 * top
            output = harmonic_output(
                csound, module, csd, a4, string, ceiling_sounding, 8,
                "A4={} string {} inclusive harmonic G4 q".format(
                    a4, string))
            check_harmonic_record(
                output, 48000, string, open_frequency,
                ceiling_sounding, 8, False, top)

            values = geometry_values(48000, 32, a4=a4)
            values.update({
                "TEST_STRING": string,
                "TEST_PROBE_STRING": string,
                "TEST_HARMONIC": 8,
                "TEST_FREQUENCY": pitch_at_snap_edge(
                    8.0 * open_frequency, -1.0, False),
            })
            expect_rejection(
                csound, module, csd, values,
                "A4={} string {} harmonic q below open".format(a4, string))

            values["TEST_FREQUENCY"] = pitch_at_snap_edge(
                8.0 * top, 1.0, False)
            expect_rejection(
                csound, module, csd, values,
                "A4={} string {} harmonic q above G4".format(a4, string))


def check_invalid_harmonic_fallback(
    csound: Path, module: Path, csd: Path,
) -> None:
    opens = [pitch(midi) for midi in OPEN_MIDI]
    top = pitch(TOP_MIDI)
    sounding = pitch(45)
    string = 1
    wanted_stop = 1.0 - opens[0] / sounding
    for request in (-1.0, 1.0, 2.5, 9.0):
        values = geometry_values(48000, 32)
        values.update({
            "TEST_STRING": string,
            "TEST_PROBE_STRING": string,
            "TEST_FREQUENCY": sounding,
            "TEST_HARMONIC": request,
        })
        label = "invalid harmonic {} normal fallback".format(request)
        output = run_csound(csound, module, csd, values, label)
        harmonic = one_record(output, "WG_HARMONIC", 1, string, 9)
        finger = one_record(output, "WG_FINGER", 1, string, 5)
        wave = one_record(output, "WG_WAVE", 1, string, 7)
        require_close(label + " request", harmonic[0], request)
        require_close(label + " active order", harmonic[1], 0.0)
        require_close(label + " natural", harmonic[3], 0.0)
        require_close(label + " sounding pitch", harmonic[4], sounding)
        require_close(label + " fundamental", harmonic[5], sounding)
        require_close(label + " stop", harmonic[6], wanted_stop)
        require_close(label + " touch", harmonic[7], 0.0)
        require_close(label + " finger stop", finger[1], wanted_stop)
        require_close(label + " waveguide pitch", wave[3], sounding)
        require_close(label + " loop phase", wave[4], 48000 / sounding)
        if harmonic[8] != 1.0 or finger[4] != 1.0 or wave[6] != 1.0:
            raise RuntimeError("{} returned non-finite geometry".format(label))

        for rejected, edge in (
                (pitch_at_snap_edge(opens[0], -1.0, False), "below E1"),
                (pitch_at_snap_edge(top, 1.0, False), "above G4")):
            invalid = dict(values)
            invalid["TEST_FREQUENCY"] = rejected
            expect_rejection(
                csound, module, csd, invalid,
                "invalid harmonic {} fallback {}".format(request, edge))

    values = geometry_values(48000, 32, case=3)
    values.update({
        "TEST_FREQUENCY": sounding,
        "TEST_HARMONIC": 9.0,
    })
    label = "invalid automatic harmonic normal fallback"
    output = run_csound(csound, module, csd, values, label)
    selected = selected_auto_string(output, label)
    if selected != 4:
        raise RuntimeError(
            "{} chose string {}, wanted 4".format(label, selected))
    harmonic = one_record(output, "WG_HARMONIC", 4, 4, 9)
    require_close(label + " active order", harmonic[1], 0.0)
    require_close(label + " fundamental", harmonic[5], sounding)


def check_sample_rate_harmonic_ceiling(
    csound: Path, module: Path, csd: Path,
) -> None:
    sample_rate = 8000
    a4 = 440.0
    opens = [pitch(midi, a4) for midi in OPEN_MIDI]
    top = pitch(TOP_MIDI, a4)
    order = 8
    ceiling = min(order * top, 0.20 * sample_rate)

    values = geometry_values(sample_rate, 32, a4=a4)
    values.update({
        "TEST_STRING": 1,
        "TEST_PROBE_STRING": 1,
        "TEST_FREQUENCY": ceiling,
        "TEST_HARMONIC": order,
    })
    output = run_csound(
        csound, module, csd, values,
        "inclusive low-sample-rate harmonic ceiling")
    check_harmonic_record(
        output, sample_rate, 1, opens[0], ceiling, order, False, top)

    above = dict(values)
    above["TEST_FREQUENCY"] = math.nextafter(ceiling, math.inf)
    expect_rejection(
        csound, module, csd, above,
        "explicit harmonic above low-sample-rate ceiling",
        HARMONIC_DSP_DIAGNOSTIC)

    automatic = geometry_values(sample_rate, 32, case=3, a4=a4)
    automatic.update({
        "TEST_FREQUENCY": ceiling,
        "TEST_HARMONIC": order,
    })
    label = "automatic harmonic at low-sample-rate ceiling"
    auto_output = run_csound(csound, module, csd, automatic, label)
    selected = selected_auto_string(auto_output, label)
    if selected != 4:
        raise RuntimeError(
            "{} chose string {}, wanted 4".format(label, selected))
    check_harmonic_record(
        auto_output, sample_rate, 4, opens[3], ceiling, order,
        False, top, tag=4)
    automatic["TEST_FREQUENCY"] = math.nextafter(ceiling, math.inf)
    expect_rejection(
        csound, module, csd, automatic,
        "automatic harmonic above low-sample-rate ceiling",
        HARMONIC_DSP_DIAGNOSTIC)

    endpoint_rate = 1000
    endpoint_order = 7
    endpoint = geometry_values(endpoint_rate, 1)
    endpoint.update({
        "TEST_STRING": 1,
        "TEST_PROBE_STRING": 1,
        "TEST_FREQUENCY": endpoint_order * opens[0],
        "TEST_HARMONIC": endpoint_order,
    })
    expect_rejection(
        csound, module, csd, endpoint,
        "exact 7*E1 above low-sample-rate harmonic ceiling",
        HARMONIC_DSP_DIAGNOSTIC)


def check_passive_sympathetic_tuning(
    csound: Path, module: Path, csd: Path,
) -> None:
    for a4 in (440.0, 442.0):
        opens = [pitch(midi, a4) for midi in OPEN_MIDI]
        for target_string, target_open in enumerate(opens, start=1):
            drive_string = 4 if target_string == 1 else 1
            values = geometry_values(48000, 32, case=4, a4=a4)
            values.update({
                "TEST_DRIVE_STRING": drive_string,
                "TEST_PROBE_STRING": target_string,
                "TEST_FREQUENCY": opens[drive_string - 1],
            })
            label = "A4={} passive string {} tuning".format(
                a4, target_string)
            output = run_csound(csound, module, csd, values, label)
            check_geometry_record(
                output, 48000, target_string, target_open,
                lowest_open=opens[0])
            active, level, energy, peak, samples, finite = one_record(
                output, "WG_PASSIVE", 1, target_string, 6)
            if active != 1.0 or finite != 1.0 or samples < 4000.0:
                raise RuntimeError(
                    "{} did not advance finite passive state".format(label))
            if level <= 0.0 or energy <= 0.0 or peak <= 0.0:
                raise RuntimeError(
                    "{} received no sympathetic energy".format(label))
            if level >= 4.0 or peak >= 4.0:
                raise RuntimeError(
                    "{} passive state was not bounded".format(label))


def check_exact_handoffs(
    csound: Path, module: Path, csd: Path, root: Path,
) -> None:
    opens = [pitch(midi) for midi in OPEN_MIDI]
    for string, frequency in enumerate(opens, start=1):
        base = geometry_values(48000, 32, case=5)
        base.update({
            "TEST_STRING": string,
            "TEST_PROBE_STRING": string,
            "TEST_FREQUENCY": frequency,
            "TEST_SCORE_END": 0.3,
        })
        reference_values = dict(base)
        reference_values["TEST_SPLIT"] = 0
        split_values = dict(base)
        split_values["TEST_SPLIT"] = 1
        label = "string {} exact handoff".format(string)
        reference_path = root / "handoff-string-{}-reference.f64".format(string)
        split_path = root / "handoff-string-{}-split.f64".format(string)
        reference_output = render_csound(
            csound, module, csd, reference_values, reference_path,
            label + " reference")
        split_output = render_csound(
            csound, module, csd, split_values, split_path, label + " split")
        reference = read_pcm(reference_path, label + " reference")
        split = read_pcm(split_path, label + " split")
        compare_pcm(label, reference, split)
        for marker, size in (
                ("WG_STRING", 7),
                ("WG_FINGER", 5),
                ("WG_WAVE", 7),
                ("WG_BOW_GEOMETRY", 5),
                ("WG_HARMONIC", 9)):
            uninterrupted = one_record(
                reference_output, marker, 1, string, size)
            handed_off = one_record(split_output, marker, 1, string, size)
            for index, (wanted, found) in enumerate(
                    zip(uninterrupted, handed_off), start=1):
                require_close(
                    "{} {} field {}".format(label, marker, index),
                    found, wanted)
        check_geometry_record(
            reference_output, 48000, string, frequency,
            lowest_open=opens[0])


def check_automatic_handoffs(
    csound: Path, module: Path, csd: Path, root: Path,
) -> None:
    opens = [pitch(midi) for midi in OPEN_MIDI]
    for string, frequency in enumerate(opens, start=1):
        base = geometry_values(48000, 32, case=5)
        base.update({
            "TEST_STRING": 0,
            "TEST_PROBE_STRING": string,
            "TEST_FREQUENCY": frequency,
            "TEST_SCORE_END": 0.3,
        })
        reference_values = dict(base)
        reference_values["TEST_SPLIT"] = 0
        split_values = dict(base)
        split_values["TEST_SPLIT"] = 1
        label = "automatic string {} exact handoff".format(string)
        reference_path = root / "auto-handoff-{}-reference.f64".format(string)
        split_path = root / "auto-handoff-{}-split.f64".format(string)
        reference_output = render_csound(
            csound, module, csd, reference_values, reference_path,
            label + " reference")
        split_output = render_csound(
            csound, module, csd, split_values, split_path, label + " split")
        reference = read_pcm(reference_path, label + " reference")
        split = read_pcm(split_path, label + " split")
        compare_pcm(label, reference, split)
        for marker, size in (
                ("WG_STRING", 7),
                ("WG_FINGER", 5),
                ("WG_WAVE", 7),
                ("WG_BOW_GEOMETRY", 5),
                ("WG_HARMONIC", 9)):
            uninterrupted = one_record(
                reference_output, marker, 1, string, size)
            handed_off = one_record(split_output, marker, 1, string, size)
            for index, (wanted, found) in enumerate(
                    zip(uninterrupted, handed_off), start=1):
                require_close(
                    "{} {} field {}".format(label, marker, index),
                    found, wanted)
        check_geometry_record(
            reference_output, 48000, string, frequency,
            lowest_open=opens[0])


def check_bounded_glissandi(
    csound: Path, module: Path, csd: Path, root: Path,
) -> None:
    opens = [pitch(midi) for midi in OPEN_MIDI]
    endings = [pitch(midi) for midi in GLISS_END_MIDI]
    for string, (start, end) in enumerate(zip(opens, endings), start=1):
        values = geometry_values(48000, 32, case=6)
        values.update({
            "TEST_STRING": string,
            "TEST_FREQUENCY": start,
            "TEST_FREQUENCY2": end,
            "TEST_SCORE_END": 0.3,
        })
        label = "string {} bounded glissando".format(string)
        output_path = root / "gliss-string-{}.f64".format(string)
        output = render_csound(
            csound, module, csd, values, output_path, label)
        read_pcm(output_path, label)
        check_normal_stopped_record(
            output, 48000, string, start, end)


def check_long_gap_resume(
    csound: Path, module: Path, csd: Path, root: Path,
) -> None:
    opens = [pitch(midi) for midi in OPEN_MIDI]
    values = geometry_values(48000, 32, case=7)
    values["TEST_SCORE_END"] = 6.5
    label = "all-string long-gap resume"
    output_path = root / "long-gap.f64"
    output = render_csound(
        csound, module, csd, values, output_path, label)
    read_pcm(output_path, label)
    wanted_gap = (6.2 - 0.08) * 48000
    for string, frequency in enumerate(opens, start=1):
        check_geometry_record(
            output, 48000, string, frequency, tag=string,
            lowest_open=opens[0])
        state = one_record(output, "WG_STRING", string, string, 7)
        wave = one_record(output, "WG_WAVE", string, string, 7)
        require_close(
            "string {} long-gap samples".format(string), state[4], wanted_gap)
        if wave[5] < 1.0:
            raise RuntimeError(
                "string {} long gap did not clear its rails".format(string))


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
        # E1 comes first so the lowest and longest delay fails first.
        check_e1_matrix(options.csound, options.module, options.csd)
        check_all_opens(options.csound, options.module, options.csd)
        check_tuned_opens(options.csound, options.module, options.csd)
        check_reference_extreme_matrix(
            options.csound, options.module, options.csd)
        check_contact_split_corners(
            options.csound, options.module, options.csd)
        check_nonfinite_frequency_fallback(
            options.csound, options.module, options.csd)
        check_auto_selection(options.csound, options.module, options.csd)
        check_continuous_auto_boundary_handoffs(
            options.csound, options.module, options.csd)
        check_normal_range(options.csound, options.module, options.csd)
        check_harmonics(options.csound, options.module, options.csd)
        check_csound_pitch_snap(
            options.csound, options.module, options.csd)
        check_harmonic_range(options.csound, options.module, options.csd)
        check_invalid_harmonic_fallback(
            options.csound, options.module, options.csd)
        check_sample_rate_harmonic_ceiling(
            options.csound, options.module, options.csd)
        check_passive_sympathetic_tuning(
            options.csound, options.module, options.csd)
        with tempfile.TemporaryDirectory(prefix="hlolli-geometry-") as folder:
            root = Path(folder)
            check_exact_handoffs(
                options.csound, options.module, options.csd, root)
            check_automatic_handoffs(
                options.csound, options.module, options.csd, root)
            check_bounded_glissandi(
                options.csound, options.module, options.csd, root)
            check_long_gap_resume(
                options.csound, options.module, options.csd, root)
    except (OSError, RuntimeError, subprocess.TimeoutExpired, ValueError,
            OverflowError) as error:
        print("geometry test failed: {}".format(error), file=sys.stderr)
        return 1
    print(
        "double-bass geometry passed: {} sample-rate/block pairs, range, "
        "harmonics, tuning, sympathy, handoff, glissando, and long gaps".format(
            len(SAMPLE_RATES) * len(BLOCK_SIZES)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
