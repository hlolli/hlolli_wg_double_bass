#!/usr/bin/env python3
"""Check double-bass handles, owners, handoff, and elapsed decay."""

from __future__ import annotations

import argparse
import json
import math
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Mapping, Sequence, Tuple


StringState = Tuple[float, ...]
RendererState = Tuple[float, ...]

STRING_FIELDS = (
    "phase", "vibrato_phase", "frequency", "trigger", "force", "speed",
    "activity", "last_sample", "gap_samples", "owner", "successor",
)
RENDERER_FIELDS = (
    "body", "sympathetic", "mute", "activity", "last_sample",
    "gap_samples", "owner", "successor",
)


def run_csound(
    csound: Path,
    module: Path,
    csd: Path,
    definitions: Mapping[str, object],
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
        "-n",
        "-d",
        "-m128",
        str(csd),
    ]
    return subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=20,
        check=False,
    )


def read_states(
    output: str, marker: str, field_count: int,
) -> Dict[int, Tuple[float, ...]]:
    states: Dict[int, Tuple[float, ...]] = {}
    for line in output.splitlines():
        start = line.find(marker + " ")
        if start < 0:
            continue
        words = line[start:].split()
        if len(words) < field_count + 2:
            continue
        tag = int(float(words[1]))
        states[tag] = tuple(
            float(value) for value in words[2:2 + field_count])
    return states


def read_creators(output: str) -> Tuple[float, float] | None:
    for line in output.splitlines():
        start = line.find("WG_CREATOR ")
        if start >= 0:
            words = line[start:].split()
            if len(words) >= 3:
                return float(words[1]), float(words[2])
    return None


def add_process_failure(
    failures: List[str], label: str, completed: subprocess.CompletedProcess[str],
) -> None:
    failures.append(
        "{} failed with status {}:\n{}".format(
            label, completed.returncode, completed.stdout.rstrip()))


def expect_success(
    failures: List[str], label: str, completed: subprocess.CompletedProcess[str],
) -> bool:
    if completed.returncode == 0:
        return True
    add_process_failure(failures, label, completed)
    return False


def expect_failure(
    failures: List[str], label: str, completed: subprocess.CompletedProcess[str],
    text: str,
) -> None:
    if completed.returncode == 0:
        failures.append("{} was accepted".format(label))
    elif text not in completed.stdout:
        failures.append(
            "{} lacked {!r}:\n{}".format(
                label, text, completed.stdout.rstrip()))


def close(left: float, right: float, tolerance: float = 2.0e-12) -> bool:
    return math.isclose(left, right, rel_tol=tolerance, abs_tol=tolerance)


def compare_state(
    failures: List[str], label: str, fields: Sequence[str],
    reference: Sequence[float], actual: Sequence[float],
) -> None:
    if len(reference) != len(fields) or len(actual) != len(fields):
        failures.append("{} returned the wrong state size".format(label))
        return
    for field, wanted, found in zip(fields, reference, actual):
        if not close(wanted, found):
            failures.append(
                "{} changed {}: {:.17g} != {:.17g}".format(
                    label, field, found, wanted))


def one_state(
    failures: List[str], label: str, completed: subprocess.CompletedProcess[str],
    marker: str, field_count: int,
) -> Tuple[float, ...] | None:
    if not expect_success(failures, label, completed):
        return None
    states = read_states(completed.stdout, marker, field_count)
    if 1 not in states:
        failures.append("{} printed no {} state".format(label, marker))
        return None
    return states[1]


def check_creators(
    failures: List[str], csound: Path, module: Path, ownership_csd: Path,
) -> None:
    completed = run_csound(
        csound, module, ownership_csd, {"TEST_CASE": 0})
    if not expect_success(failures, "creator check", completed):
        return
    handles = read_creators(completed.stdout)
    if handles is None:
        failures.append("creator check printed no handles")
        return
    for handle in handles:
        if not math.isfinite(handle) or handle < 1.0 or handle != math.floor(handle):
            failures.append("creator returned invalid handle {!r}".format(handle))
    if handles[0] == handles[1]:
        failures.append("the two creators returned the same handle")


def check_errors(
    failures: List[str], csound: Path, module: Path,
    ownership_csd: Path, handoff_csd: Path,
) -> None:
    cases = (
        ("zero voice handle", 2, "handle must be a positive integer"),
        ("fractional voice handle", 3, "handle must be a positive integer"),
        ("unknown voice handle", 4, "unknown double-bass handle 999"),
        ("zero renderer handle", 5, "handle must be a positive integer"),
        ("fractional renderer handle", 6, "handle must be a positive integer"),
        ("unknown renderer handle", 7, "unknown double-bass handle 999"),
        ("local-ksmps creator", 8, "handles require engine ksmps 32"),
        ("local-ksmps voice", 9, "requires engine ksmps 32"),
        ("local-ksmps renderer", 10, "requires engine ksmps 32"),
        ("duplicate renderer", 11, "already has an output opcode"),
        ("duplicate string", 12, "string 1 already has a controller"),
        ("extra auto controller", 14, "all four strings are busy"),
    )
    for label, test_case, text in cases:
        completed = run_csound(
            csound, module, ownership_csd, {"TEST_CASE": test_case})
        expect_failure(failures, label, completed, text)

    for label, target, text in (
        ("overlapping controllers", 1, "string 1 already has a controller"),
        ("overlapping renderers", 2, "already has an output opcode"),
    ):
        completed = run_csound(
            csound,
            module,
            handoff_csd,
            {
                "TEST_TARGET": target,
                "TEST_SPLIT": 1,
                "TEST_SPLIT_TIME": 0.016125,
                "TEST_OVERLAP": 1,
            },
        )
        expect_failure(failures, label, completed, text)

    for label, target, text in (
        ("one-sample controller overlap", 1,
         "string 1 already has a controller"),
        ("one-sample renderer overlap", 2,
         "already has an output opcode"),
    ):
        completed = run_csound(
            csound, module, handoff_csd,
            {"TEST_TARGET": target, "TEST_SAMPLE_SPAN": 1})
        expect_failure(failures, label, completed, text)


def check_string_slots(
    failures: List[str], csound: Path, module: Path, ownership_csd: Path,
) -> None:
    completed = run_csound(
        csound, module, ownership_csd, {"TEST_CASE": 13})
    expect_success(failures, "four explicit strings", completed)


def check_isolation(
    failures: List[str], csound: Path, module: Path, ownership_csd: Path,
) -> None:
    completed = run_csound(
        csound, module, ownership_csd, {"TEST_CASE": 15})
    if not expect_success(failures, "two-handle isolation", completed):
        return
    states = read_states(completed.stdout, "WG_STRING", len(STRING_FIELDS))
    if 1 not in states or 2 not in states:
        failures.append("two-handle isolation printed fewer than two states")
        return
    bass_a = states[1]
    bass_b = states[2]
    checks = (
        ("double-bass A frequency", bass_a[2], 110.0),
        ("double-bass A force", bass_a[4], 0.2),
        ("double-bass A speed", bass_a[5], -0.25),
        ("double-bass A activity", bass_a[6], 0.35),
        ("double-bass A last sample", bass_a[7], 800.0),
        ("double-bass B frequency", bass_b[2], 220.0),
        ("double-bass B force", bass_b[4], 0.9),
        ("double-bass B speed", bass_b[5], 0.75),
        ("double-bass B activity", bass_b[6], 0.8),
        ("double-bass B last sample", bass_b[7], 1184.0),
    )
    for label, found, wanted in checks:
        if not close(found, wanted):
            failures.append(
                "{} was {:.17g}, wanted {:.17g}".format(
                    label, found, wanted))
    if bass_a == bass_b:
        failures.append("the two double-bass snapshots were equal")
    for label, state in (("double-bass A", bass_a), ("double-bass B", bass_b)):
        if state[9] != 0.0 or state[10] != 0.0:
            failures.append("{} kept an owner after release".format(label))


def run_handoff(
    csound: Path, module: Path, handoff_csd: Path,
    target: int, split: bool, split_time: float = 0.016,
) -> subprocess.CompletedProcess[str]:
    return run_csound(
        csound,
        module,
        handoff_csd,
        {
            "TEST_TARGET": target,
            "TEST_SPLIT": int(split),
            "TEST_SPLIT_TIME": split_time,
        },
    )


def check_handoffs(
    failures: List[str], csound: Path, module: Path, handoff_csd: Path,
) -> None:
    string_reference = one_state(
        failures,
        "continuous controller",
        run_handoff(csound, module, handoff_csd, 1, False),
        "WG_STRING",
        len(STRING_FIELDS),
    )
    renderer_reference = one_state(
        failures,
        "continuous renderer",
        run_handoff(csound, module, handoff_csd, 2, False),
        "WG_RENDERER",
        len(RENDERER_FIELDS),
    )

    for name, split_time in (("block", 0.016), ("mid-block", 0.016125)):
        string_split = one_state(
            failures,
            "{} controller handoff".format(name),
            run_handoff(csound, module, handoff_csd, 1, True, split_time),
            "WG_STRING",
            len(STRING_FIELDS),
        )
        renderer_split = one_state(
            failures,
            "{} renderer handoff".format(name),
            run_handoff(csound, module, handoff_csd, 2, True, split_time),
            "WG_RENDERER",
            len(RENDERER_FIELDS),
        )
        if string_reference is not None and string_split is not None:
            compare_state(
                failures,
                "{} controller handoff".format(name),
                STRING_FIELDS,
                string_reference,
                string_split,
            )
        if renderer_reference is not None and renderer_split is not None:
            compare_state(
                failures,
                "{} renderer handoff".format(name),
                RENDERER_FIELDS,
                renderer_reference,
                renderer_split,
            )


def check_restarts(
    failures: List[str], csound: Path, module: Path, handoff_csd: Path,
    string_loss_seconds: float,
) -> None:
    string = one_state(
        failures,
        "controller restart",
        run_csound(
            csound,
            module,
            handoff_csd,
            {"TEST_TARGET": 1, "TEST_RESTART": 1},
        ),
        "WG_STRING",
        len(STRING_FIELDS),
    )
    renderer = one_state(
        failures,
        "renderer restart",
        run_csound(
            csound,
            module,
            handoff_csd,
            {"TEST_TARGET": 2, "TEST_RESTART": 1},
        ),
        "WG_RENDERER",
        len(RENDERER_FIELDS),
    )

    gap_samples = 768.0
    if string is not None:
        expected_ratio = math.exp(
            -gap_samples / (48000.0 * string_loss_seconds))
        checks = (
            ("controller gap samples", string[8], gap_samples),
            ("controller last sample", string[7], 2336.0),
            ("controller gap phase", string[0], (333.0 * 0.048) % 1.0),
            ("controller gap vibrato", string[1], (5.5 * 0.048) % 1.0),
            ("controller decay ratio", string[6] / 0.8, expected_ratio),
            ("controller owner", string[9], 0.0),
            ("controller successor", string[10], 0.0),
        )
        for label, found, wanted in checks:
            if not close(found, wanted):
                failures.append(
                    "{} was {:.17g}, wanted {:.17g}".format(
                        label, found, wanted))

    if renderer is not None:
        expected_ratio = math.exp(-gap_samples / (48000.0 * 0.75))
        checks = (
            ("renderer gap samples", renderer[5], gap_samples),
            ("renderer last sample", renderer[4], 2336.0),
            ("renderer decay ratio", renderer[3] / 0.6, expected_ratio),
            ("renderer body", renderer[0], 0.37),
            ("renderer sympathetic", renderer[1], 0.63),
            ("renderer mute", renderer[2], 0.21),
            ("renderer owner", renderer[6], 0.0),
            ("renderer successor", renderer[7], 0.0),
        )
        for label, found, wanted in checks:
            if not close(found, wanted):
                failures.append(
                    "{} was {:.17g}, wanted {:.17g}".format(
                        label, found, wanted))


def check_same_block_gaps(
    failures: List[str], csound: Path, module: Path, handoff_csd: Path,
    string_loss_seconds: float,
) -> None:
    string = one_state(
        failures,
        "same-block controller gap",
        run_csound(
            csound,
            module,
            handoff_csd,
            {"TEST_TARGET": 1, "TEST_SAME_BLOCK_GAP": 1},
        ),
        "WG_STRING",
        len(STRING_FIELDS),
    )
    renderer = one_state(
        failures,
        "same-block renderer gap",
        run_csound(
            csound,
            module,
            handoff_csd,
            {"TEST_TARGET": 2, "TEST_SAME_BLOCK_GAP": 1},
        ),
        "WG_RENDERER",
        len(RENDERER_FIELDS),
    )

    gap_samples = 4.0
    if string is not None:
        expected_ratio = math.exp(
            -gap_samples / (48000.0 * string_loss_seconds))
        checks = (
            ("same-block controller gap samples", string[8], gap_samples),
            ("same-block controller last sample", string[7], 2250.0),
            ("same-block controller phase", string[0],
             (333.0 * 2218.0 / 48000.0) % 1.0),
            ("same-block controller vibrato", string[1],
             (5.5 * 2218.0 / 48000.0) % 1.0),
            ("same-block controller decay ratio",
             string[6] / 0.8, expected_ratio),
            ("same-block controller owner", string[9], 0.0),
            ("same-block controller successor", string[10], 0.0),
        )
        for label, found, wanted in checks:
            if not close(found, wanted):
                failures.append(
                    "{} was {:.17g}, wanted {:.17g}".format(
                        label, found, wanted))

    if renderer is not None:
        expected_ratio = math.exp(-gap_samples / (48000.0 * 0.75))
        checks = (
            ("same-block renderer gap samples", renderer[5], gap_samples),
            ("same-block renderer last sample", renderer[4], 2250.0),
            ("same-block renderer decay ratio",
             renderer[3] / 0.6, expected_ratio),
            ("same-block renderer owner", renderer[6], 0.0),
            ("same-block renderer successor", renderer[7], 0.0),
        )
        for label, found, wanted in checks:
            if not close(found, wanted):
                failures.append(
                    "{} was {:.17g}, wanted {:.17g}".format(
                        label, found, wanted))


def check_three_span_queues(
    failures: List[str], csound: Path, module: Path, handoff_csd: Path,
) -> None:
    for target, marker, fields, name in (
        (1, "WG_STRING", STRING_FIELDS, "controller"),
        (2, "WG_RENDERER", RENDERER_FIELDS, "renderer"),
    ):
        reference = one_state(
            failures,
            "single-span {} queue reference".format(name),
            run_csound(
                csound,
                module,
                handoff_csd,
                {"TEST_TARGET": target, "TEST_QUEUE_SPANS": 1},
            ),
            marker,
            len(fields),
        )
        queued = one_state(
            failures,
            "three-span {} queue".format(name),
            run_csound(
                csound,
                module,
                handoff_csd,
                {"TEST_TARGET": target, "TEST_QUEUE_SPANS": 3},
            ),
            marker,
            len(fields),
        )
        if reference is not None and queued is not None:
            compare_state(
                failures,
                "three-span {} queue".format(name),
                fields,
                reference,
                queued,
            )


def check_long_exact_handoffs(
    failures: List[str], csound: Path, module: Path, handoff_csd: Path,
) -> None:
    for target, name in ((1, "controller"), (2, "renderer")):
        completed = run_csound(
            csound, module, handoff_csd,
            {
                "TEST_TARGET": target,
                "TEST_SAMPLE_SPAN": 2,
                "TEST_SCORE_END": 128.006,
            },
        )
        expect_success(
            failures, "long-score exact {} handoff".format(name), completed)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csound", required=True, type=Path)
    parser.add_argument("--module", required=True, type=Path)
    parser.add_argument("--model", required=True, type=Path)
    parser.add_argument("--ownership-csd", required=True, type=Path)
    parser.add_argument("--handoff-csd", required=True, type=Path)
    arguments = parser.parse_args()
    model = json.loads(arguments.model.read_text(encoding="utf-8"))
    string_loss_seconds = float(
        model["strings"][0]["loss_time_constant_seconds"])

    failures: List[str] = []
    try:
        check_creators(
            failures, arguments.csound, arguments.module,
            arguments.ownership_csd)
        check_errors(
            failures, arguments.csound, arguments.module,
            arguments.ownership_csd, arguments.handoff_csd)
        check_string_slots(
            failures, arguments.csound, arguments.module,
            arguments.ownership_csd)
        check_isolation(
            failures, arguments.csound, arguments.module,
            arguments.ownership_csd)
        check_handoffs(
            failures, arguments.csound, arguments.module,
            arguments.handoff_csd)
        check_restarts(
            failures, arguments.csound, arguments.module,
            arguments.handoff_csd, string_loss_seconds)
        check_same_block_gaps(
            failures, arguments.csound, arguments.module,
            arguments.handoff_csd, string_loss_seconds)
        check_three_span_queues(
            failures, arguments.csound, arguments.module,
            arguments.handoff_csd)
        check_long_exact_handoffs(
            failures, arguments.csound, arguments.module,
            arguments.handoff_csd)
    except (OSError, subprocess.TimeoutExpired, ValueError) as error:
        print("ownership test failed: {}".format(error), file=sys.stderr)
        return 2

    if failures:
        for failure in failures:
            print("failure: {}".format(failure), file=sys.stderr)
        return 1
    print("double-bass ownership checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
