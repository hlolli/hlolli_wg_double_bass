#!/usr/bin/env python3
"""Check the sample-free bowed speed, force, and position control laws."""

from __future__ import annotations

import argparse
import json
import math
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import List, Mapping


OPEN_FREQUENCIES = (
    41.20344461410875,
    55.0,
    73.41619197935188,
    97.99885899543733,
)
FRICTION_LUT_SIZE = 2049
FRICTION_MAX_SPEED = 2.0


@dataclass(frozen=True)
class Probe:
    requested_force: float
    effective_force: float
    bow_speed: float
    contact: float
    target_position: float
    effective_position: float
    impedance: float
    minimum_force: float
    maximum_force: float
    bow_finite: float
    force_target: float
    speed_target: float
    force_output: float
    speed_output: float
    contact_output: float
    gesture_recoveries: float
    gesture_finite: float
    release_articulation: float
    release_gain_target: float
    bow_state: float
    gesture_active: float
    second_polarization: float
    physics_finite: float


def clamp(value: float, low: float, high: float) -> float:
    return min(high, max(low, value))


def close(left: float, right: float, tolerance: float = 1.0e-8) -> bool:
    return abs(left - right) <= tolerance * max(1.0, abs(left), abs(right))


def friction_mu(model: Mapping[str, object], speed: float) -> float:
    bow = model["bow"]
    assert isinstance(bow, dict)
    friction = bow["friction"]
    assert isinstance(friction, dict)

    def evaluate(value: float) -> float:
        return (
            float(friction["floor_mu"])
            + float(friction["slow_gain"])
            * math.exp(-value / float(friction["slow_speed"]))
            + float(friction["fast_gain"])
            * math.exp(-value / float(friction["fast_speed"]))
        )

    speed = abs(speed)
    if speed >= FRICTION_MAX_SPEED:
        return float(friction["floor_mu"])
    scaled = speed * (FRICTION_LUT_SIZE - 1) / FRICTION_MAX_SPEED
    index = math.floor(scaled)
    fraction = scaled - index
    step = FRICTION_MAX_SPEED / (FRICTION_LUT_SIZE - 1)
    return ((1.0 - fraction) * evaluate(index * step)
            + fraction * evaluate((index + 1) * step))


def run_probe(
    csound: Path,
    module: Path,
    csd: Path,
    string_index: int,
    force: float,
    speed: float,
    position: float,
    release_time: float = 1.0,
) -> Probe:
    definitions = {
        "TEST_STRING": string_index,
        "TEST_FREQUENCY": format(OPEN_FREQUENCIES[string_index - 1], ".17g"),
        "TEST_FORCE": format(force, ".17g"),
        "TEST_SPEED": format(speed, ".17g"),
        "TEST_POSITION": format(position, ".17g"),
        "TEST_RELEASE_TIME": format(release_time, ".17g"),
    }
    command = [
        str(csound),
        "--opcode-lib={}".format(module),
        "--sample-accurate",
        "--num-threads=1",
        *[
            "--omacro:{}={}".format(name, value)
            for name, value in definitions.items()
        ],
        str(csd),
    ]
    completed = subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=30,
        check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            "bow-control probe failed for {!r}:\n{}".format(
                definitions, completed.stdout.rstrip()))
    marker = "WG_BOW_CONTROL "
    rows = [
        line[line.index(marker) + len(marker):].split()
        for line in completed.stdout.splitlines()
        if marker in line
    ]
    if len(rows) != 1 or len(rows[0]) != 21:
        raise RuntimeError(
            "bow-control probe returned malformed output for {!r}:\n{}".format(
                definitions, completed.stdout.rstrip()))
    polarization_marker = "WG_POLARIZATION "
    polarization_rows = [
        line[
            line.index(polarization_marker) + len(polarization_marker):
        ].split()
        for line in completed.stdout.splitlines()
        if polarization_marker in line
    ]
    if len(polarization_rows) != 1 or len(polarization_rows[0]) != 2:
        raise RuntimeError(
            "bow-control polarization probe returned malformed output for "
            "{!r}:\n{}".format(definitions, completed.stdout.rstrip()))
    return Probe(*(
        float(value) for value in rows[0] + polarization_rows[0]
    ))


def expected_forces(
    model: Mapping[str, object],
    probe: Probe,
    control: float,
    input_speed: float,
) -> tuple[float, float, float, float]:
    bow = model["bow"]
    assert isinstance(bow, dict)
    force_map = bow["force_map"]
    friction = bow["friction"]
    assert isinstance(force_map, dict)
    assert isinstance(friction, dict)
    velocity = max(abs(clamp(input_speed, -1.0, 1.0)
                       * float(bow["speed_scale"])), 0.02)
    dynamic_mu = friction_mu(model, velocity / probe.effective_position)
    static_mu = (
        float(friction["floor_mu"])
        + float(friction["slow_gain"])
        + float(friction["fast_gain"])
    )
    mu_drop = max(
        float(force_map["mu_drop_floor"]), static_mu - dynamic_mu)
    maximum = clamp(
        2.0 * probe.impedance * velocity
        / (probe.effective_position * mu_drop),
        0.05,
        4.0,
    )
    minimum = clamp(
        probe.impedance * velocity
        / (float(force_map["minimum_divisor"])
           * probe.effective_position ** 2 * mu_drop),
        0.005,
        maximum / 1.5,
    )
    control = clamp(control, 0.0, 1.0)
    low = float(force_map["control_low"])
    high = float(force_map["control_high"])
    if control < low:
        requested = minimum * control / low
    elif control < high:
        mix = (control - low) / (high - low)
        requested = math.exp(
            (1.0 - mix) * math.log(minimum)
            + mix * math.log(float(force_map["normal_max_scale"]) * maximum)
        )
    else:
        mix = (control - high) / (1.0 - high)
        extreme = min(4.0, float(force_map["extreme_max_scale"]) * maximum)
        requested = math.exp(
            (1.0 - mix)
            * math.log(float(force_map["normal_max_scale"]) * maximum)
            + mix * math.log(extreme)
        )
    cap = 2.0 * probe.impedance * FRICTION_MAX_SPEED / static_mu
    return minimum, maximum, requested, min(requested, cap)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csound", required=True, type=Path)
    parser.add_argument("--module", required=True, type=Path)
    parser.add_argument("--csd", required=True, type=Path)
    parser.add_argument("--model", required=True, type=Path)
    arguments = parser.parse_args()
    model = json.loads(arguments.model.read_text(encoding="utf-8"))
    failures: List[str] = []

    bow = model.get("bow", {})
    if bow.get("speed_scale") != 0.65:
        failures.append("fixed bow speed scale changed from declared candidate")
    if model.get("evidence_status") != "violin-derived":
        failures.append("physical constants prematurely changed evidence status")

    maximum_speed_error = 0.0
    for speed in (-1.0, -0.4, 0.4, 1.0):
        try:
            probe = run_probe(
                arguments.csound, arguments.module, arguments.csd,
                1, 1.0e-5, speed, 0.12)
        except RuntimeError as error:
            failures.append(str(error))
            continue
        expected_speed = 0.65 * speed
        maximum_speed_error = max(
            maximum_speed_error, abs(probe.bow_speed - expected_speed))
        if not close(probe.speed_target, speed) or not close(
                probe.speed_output, speed):
            failures.append(
                "direct arco changed normalized speed {} to {}/{}".format(
                    speed, probe.speed_target, probe.speed_output))
        if abs(probe.bow_speed - expected_speed) > 2.0e-6:
            failures.append(
                "speed {} mapped to {} m/s instead of {} m/s".format(
                    speed, probe.bow_speed, expected_speed))
        if probe.bow_finite != 1.0 or probe.gesture_finite != 1.0:
            failures.append("speed {} produced non-finite state".format(speed))

    maximum_formula_error = 0.0
    maximum_ratio_error = 0.0
    checked_regions = 0
    for string_index in range(1, 5):
        string_model = model["strings"][string_index - 1]
        assert isinstance(string_model, dict)
        second_polarization = string_model["second_polarization"]
        assert isinstance(second_polarization, dict)
        expected_polarization = float(second_polarization["mix"])
        for position in (0.08, 0.12, 0.20):
            force = 0.475
            speed = 0.12
            try:
                probe = run_probe(
                    arguments.csound, arguments.module, arguments.csd,
                    string_index, force, speed, position)
            except RuntimeError as error:
                failures.append(str(error))
                continue
            expected = expected_forces(model, probe, force, speed)
            observed = (
                probe.minimum_force / probe.contact,
                probe.maximum_force / probe.contact,
                probe.requested_force / probe.contact,
                probe.effective_force / probe.contact,
            )
            maximum_formula_error = max(
                maximum_formula_error,
                *(abs(actual - wanted)
                  for actual, wanted in zip(observed, expected)),
            )
            ratio = observed[1] / observed[0]
            expected_ratio = expected[1] / expected[0]
            maximum_ratio_error = max(
                maximum_ratio_error, abs(ratio - expected_ratio))
            if any(not close(actual, wanted, 2.0e-7)
                   for actual, wanted in zip(observed, expected)):
                failures.append(
                    "string {} position {} force map mismatch: observed {!r}, "
                    "expected {!r}".format(
                        string_index, position,
                        tuple(round(value, 9) for value in observed),
                        tuple(round(value, 9) for value in expected)))
            if not (probe.minimum_force < probe.requested_force
                    < 0.85 * probe.maximum_force):
                failures.append(
                    "interior force left the normal Schelleng region for "
                    "string {} position {}".format(string_index, position))
            if (not close(probe.target_position, position, 2.0e-7)
                    or not close(probe.effective_position, position, 2.0e-7)):
                failures.append(
                    "position {} did not reach bow geometry {}/{}".format(
                        position, probe.target_position,
                        probe.effective_position))
            if (probe.bow_finite != 1.0 or probe.gesture_finite != 1.0
                    or probe.physics_finite != 1.0):
                failures.append(
                    "string {} position {} produced non-finite state".format(
                        string_index, position))
            if not close(probe.second_polarization, expected_polarization):
                failures.append(
                    "string {} polarization was {}, expected {}".format(
                        string_index, probe.second_polarization,
                        expected_polarization))
            checked_regions += 1

    for force in (0.05, 0.10, 0.475, 0.85, 1.0):
        try:
            probe = run_probe(
                arguments.csound, arguments.module, arguments.csd,
                1, force, 0.12, 0.12)
        except RuntimeError as error:
            failures.append(str(error))
            continue
        expected = expected_forces(model, probe, force, 0.12)
        observed = (
            probe.minimum_force / probe.contact,
            probe.maximum_force / probe.contact,
            probe.requested_force / probe.contact,
            probe.effective_force / probe.contact,
        )
        if any(not close(actual, wanted, 2.0e-7)
               for actual, wanted in zip(observed, expected)):
            failures.append(
                "normalized force {} did not select the declared piecewise "
                "region".format(force))
        if not close(probe.force_target, force) or not close(
                probe.force_output, force):
            failures.append(
                "direct arco changed normalized force {} to {}/{}".format(
                    force, probe.force_target, probe.force_output))

    clamp_cases = (
        (2.0, 0.12, 0.12, 1.0, 0.12, 0.12),
        (0.475, 2.0, 0.12, 0.475, 1.0, 0.12),
        (0.475, 0.12, 0.80, 0.475, 0.12, 0.49),
    )
    for force, speed, position, wanted_force, wanted_speed, wanted_position in clamp_cases:
        try:
            probe = run_probe(
                arguments.csound, arguments.module, arguments.csd,
                1, force, speed, position)
        except RuntimeError as error:
            failures.append(str(error))
            continue
        if (not close(probe.force_target, wanted_force)
                or not close(probe.speed_target, wanted_speed)
                or not close(probe.target_position, wanted_position, 2.0e-7)
                or probe.bow_finite != 1.0
                or probe.gesture_finite != 1.0):
            failures.append(
                "unsafe clamp result for force/speed/position {!r}: {!r}".format(
                    (force, speed, position),
                    (probe.force_target, probe.speed_target,
                     probe.target_position, probe.bow_finite,
                     probe.gesture_finite)))

    for string_index in range(1, 5):
        try:
            release_probe = run_probe(
                arguments.csound, arguments.module, arguments.csd,
                string_index, 0.475, 0.12, 0.12, release_time=0.10)
        except RuntimeError as error:
            failures.append(str(error))
            continue
        if (release_probe.release_articulation != 0.0
                or not close(release_probe.release_gain_target, 1.0)
                or release_probe.bow_state != 0.0
                or release_probe.gesture_active != 0.0
                or abs(release_probe.requested_force) > 1.0e-12
                or abs(release_probe.effective_force) > 1.0e-12
                or abs(release_probe.contact) > 1.0e-7
                or abs(release_probe.contact_output) > 1.0e-8
                or release_probe.bow_finite != 1.0
                or release_probe.gesture_finite != 1.0
                or release_probe.gesture_recoveries != 0.0):
            failures.append(
                "string {} ordinary arco bow lift did not reach finite "
                "contact-only passive decay: {!r}".format(
                    string_index, release_probe))

    print(
        "bow speed law: max physical-speed error {:.9g} m/s".format(
            maximum_speed_error))
    print(
        "bow Schelleng law: {} string/position regions, max force error "
        "{:.9g} N, max interval-ratio error {:.9g}".format(
            checked_regions, maximum_formula_error, maximum_ratio_error))
    if failures:
        print("physical bow-control test failed:", file=sys.stderr)
        for failure in failures:
            print("- " + failure, file=sys.stderr)
        return 1
    print("physical bow-control relationships passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
