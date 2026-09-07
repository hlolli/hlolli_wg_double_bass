#!/usr/bin/env python3
"""Check constructors, run order, workers, handles, and prepared PCM."""

from __future__ import annotations

import argparse
from array import array
import hashlib
import math
from pathlib import Path
import subprocess
import sys
import tempfile
from typing import List, Tuple


# Worker scheduling may change the order in which Csound adds independent
# instrument outputs. These limits allow binary64 addition-order noise below
# -240 dBFS peak and -260 dBFS RMS, while still catching shifted or wrong PCM.
MAX_PARITY_ABSOLUTE_ERROR = 1.0e-12
MAX_PARITY_RMS_ERROR = 1.0e-13
OPEN_FREQUENCIES_A440 = (
    41.20344461410875,
    55.0,
    73.41619197935188,
    97.99885899543733,
)
OPEN_FREQUENCIES_A442 = (
    41.390732998718335,
    55.25,
    73.74990194289438,
    98.4443083545075,
)


def run_csound(command: list[str], label: str) -> str:
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


def constructor_command(
    csound: Path, module: Path, csd: Path, definitions: List[str],
) -> List[str]:
    return [
        str(csound),
        "--opcode-lib={}".format(module),
        "--sample-accurate",
        "--num-threads=1",
        *definitions,
        "-n",
        "-d",
        "-m128",
        str(csd),
    ]


def check_constructors(csound: Path, module: Path, csd: Path) -> None:
    output = run_csound(
        constructor_command(csound, module, csd, []),
        "constructor check",
    )
    marker = "WG_CONSTRUCTORS "
    values = None
    for line in output.splitlines():
        start = line.find(marker)
        if start >= 0:
            values = [float(word) for word in line[start:].split()[1:]]
            break
    if values is None or len(values) != 15:
        raise RuntimeError("constructor check printed the wrong facts")
    handles = values[:3]
    if len(set(handles)) != 3 or any(
            not math.isfinite(value) or value < 1 or value != math.floor(value)
            for value in handles):
        raise RuntimeError("constructors returned invalid or repeated handles")
    default = values[3:7]
    a440 = values[7:11]
    a442 = values[11:15]
    for string_index, (plain, tuned_440, tuned_442, wanted_440, wanted_442) in enumerate(
            zip(default, a440, a442,
                OPEN_FREQUENCIES_A440, OPEN_FREQUENCIES_A442), start=1):
        if not math.isclose(plain, tuned_440, rel_tol=2.0e-12, abs_tol=2.0e-12):
            raise RuntimeError(
                "no-argument constructor differs from A4=440 on string {}".format(
                    string_index))
        if not math.isclose(
                tuned_440, wanted_440, rel_tol=2.0e-12, abs_tol=2.0e-12):
            raise RuntimeError(
                "A4=440 open pitch failed on string {}: {:.17g} != {:.17g}".format(
                    string_index, tuned_440, wanted_440))
        if not math.isclose(
                tuned_442, wanted_442, rel_tol=2.0e-12, abs_tol=2.0e-12):
            raise RuntimeError(
                "A4=442 open pitch failed on string {}: {:.17g} != {:.17g}".format(
                    string_index, tuned_442, wanted_442))
        ratio = tuned_442 / tuned_440
        if not math.isclose(
                ratio, 442.0 / 440.0, rel_tol=2.0e-12, abs_tol=2.0e-12):
            raise RuntimeError(
                "numeric-A4 scaling failed on string {}".format(string_index))


def check_constructor_bounds(csound: Path, module: Path, csd: Path) -> None:
    for a4 in (380, 480):
        output = run_csound(
            constructor_command(
                csound, module, csd,
                ["--omacro:TEST_A4={}".format(a4)]),
            "A4={} constructor boundary".format(a4),
        )
        marker = "WG_CONSTRUCTOR_BOUND {} ".format(a4)
        if marker not in output:
            raise RuntimeError(
                "A4={} constructor printed no valid handle".format(a4))
    for a4 in (379.999999, 480.000001):
        completed = subprocess.run(
            constructor_command(
                csound, module, csd,
                ["--omacro:TEST_A4={:.6f}".format(a4)]),
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=30,
        )
        if completed.returncode == 0:
            raise RuntimeError("out-of-range A4={} was accepted".format(a4))
        if "reference pitch must be from 380 to 480 Hz" not in completed.stdout:
            raise RuntimeError(
                "out-of-range A4={} returned the wrong error".format(a4))


def render(
    csound: Path,
    module: Path,
    csd: Path,
    output: Path,
    case: int,
    order: int,
    threads: int,
) -> str:
    return run_csound(
        [
            str(csound),
            "--opcode-lib={}".format(module),
            "--sample-accurate",
            "--num-threads={}".format(threads),
            "--omacro:TEST_CASE={}".format(case),
            "--omacro:TEST_ORDER={}".format(order),
            "--format=raw",
            "--format=double",
            "--nopeaks",
            "-d",
            "-m128",
            "-o",
            str(output),
            str(csd),
        ],
        "case {} order {} with {} worker(s)".format(case, order, threads),
    )


def read_pcm(path: Path) -> Tuple[array, float, str]:
    pcm = path.read_bytes()
    if not pcm or len(pcm) % 8 != 0:
        raise RuntimeError("invalid float64 PCM file: {}".format(path))
    samples = array("d")
    samples.frombytes(pcm)
    if len(samples) % 2 != 0:
        raise RuntimeError("float64 PCM is not stereo: {}".format(path))
    if len(samples) // 2 < int(48000 * 0.44):
        raise RuntimeError("short float64 render: {}".format(path))
    if any(not math.isfinite(sample) for sample in samples):
        raise RuntimeError("non-finite float64 render: {}".format(path))
    peak = max(abs(sample) for sample in samples)
    active = sum(abs(sample) > 1.0e-15 for sample in samples)
    if peak < 1.0e-6 or active < 1000:
        raise RuntimeError("silent float64 render: {}".format(path))
    if peak >= 1.0:
        raise RuntimeError("bounded output clipped: {}".format(path))
    return samples, peak, hashlib.sha256(pcm).hexdigest()


def compare_pcm(
    label: str,
    reference: array,
    actual: array,
    maximum_limit: float = MAX_PARITY_ABSOLUTE_ERROR,
    rms_limit: float = MAX_PARITY_RMS_ERROR,
) -> Tuple[float, float]:
    if len(reference) != len(actual):
        raise RuntimeError(
            "{} sample counts differ: {} != {}".format(
                label, len(reference), len(actual)))
    maximum = 0.0
    square_sum = 0.0
    for wanted, found in zip(reference, actual):
        difference = abs(wanted - found)
        maximum = max(maximum, difference)
        square_sum += difference * difference
    rms = math.sqrt(square_sum / len(reference))
    if maximum > maximum_limit or rms > rms_limit:
        raise RuntimeError(
            "{} differs: max={:.17g} (limit {:.1e}), "
            "rms={:.17g} (limit {:.1e})".format(
                label, maximum, maximum_limit, rms, rms_limit))
    print(
        "{}: samples={} max={:.17g} rms={:.17g} "
        "limits={:.1e}/{:.1e}".format(
            label, len(reference), maximum, rms, maximum_limit, rms_limit))
    return maximum, rms


def read_multi_handles(output: str) -> None:
    marker = "WG_MULTI_HANDLES "
    for line in output.splitlines():
        start = line.find(marker)
        if start < 0:
            continue
        handles = [float(word) for word in line[start:].split()[1:5]]
        if len(handles) == 4 and len(set(handles)) == 4 and all(
                value >= 1 and value == math.floor(value) for value in handles):
            return
    raise RuntimeError("multi-handle render printed invalid handles")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csound", required=True, type=Path)
    parser.add_argument("--module", required=True, type=Path)
    parser.add_argument("--test-module", required=True, type=Path)
    parser.add_argument("--prepared-module", required=True, type=Path)
    parser.add_argument("--constructors-csd", required=True, type=Path)
    parser.add_argument("--constructor-bounds-csd", required=True, type=Path)
    parser.add_argument("--parity-csd", required=True, type=Path)
    options = parser.parse_args()
    for path in (
            options.csound, options.module, options.test_module,
            options.prepared_module, options.constructors_csd,
            options.constructor_bounds_csd, options.parity_csd):
        if not path.is_file():
            parser.error("missing regular file: {}".format(path))

    try:
        check_constructors(
            options.csound, options.test_module, options.constructors_csd)
        check_constructor_bounds(
            options.csound, options.module, options.constructor_bounds_csd)
        with tempfile.TemporaryDirectory(prefix="hlolli-parity-") as folder:
            root = Path(folder)

            native_path = root / "native.f64"
            render(
                options.csound, options.module, options.parity_csd,
                native_path, 1, 0, 1)
            native_pcm, native_peak, native_digest = read_pcm(native_path)

            prepared_path = root / "prepared.f64"
            render(
                options.csound, options.prepared_module, options.parity_csd,
                prepared_path, 1, 0, 1)
            prepared_pcm, _, prepared_digest = read_pcm(prepared_path)
            if prepared_pcm != native_pcm:
                raise RuntimeError(
                    "prepared WASM source changed native production PCM")

            print(
                "native/prepared float64: samples={} max=0 rms=0".format(
                    len(native_pcm)))

            early_path = root / "body-first.f64"
            late_path = root / "voice-first.f64"
            render(
                options.csound, options.module, options.parity_csd,
                early_path, 2, 0, 1)
            render(
                options.csound, options.module, options.parity_csd,
                late_path, 2, 1, 1)
            early_pcm, _, early_digest = read_pcm(early_path)
            late_pcm, _, _ = read_pcm(late_path)
            compare_pcm("body/voice run-order float64", early_pcm, late_pcm)

            one_worker_path = root / "multi-one-worker.f64"
            two_worker_path = root / "multi-two-workers.f64"
            two_worker_repeat_path = root / "multi-two-workers-repeat.f64"
            one_output = render(
                options.csound, options.module, options.parity_csd,
                one_worker_path, 3, 0, 1)
            two_output = render(
                options.csound, options.module, options.parity_csd,
                two_worker_path, 3, 0, 2)
            render(
                options.csound, options.module, options.parity_csd,
                two_worker_repeat_path, 3, 0, 2)
            read_multi_handles(one_output)
            read_multi_handles(two_output)
            one_pcm, _, one_digest = read_pcm(one_worker_path)
            two_pcm, _, two_digest = read_pcm(two_worker_path)
            repeat_pcm, _, repeat_digest = read_pcm(two_worker_repeat_path)
            compare_pcm("one/two-worker float64", one_pcm, two_pcm)
            compare_pcm("repeat two-worker float64", two_pcm, repeat_pcm)

            print(
                "constructors passed; native/prepared={} peak={}; "
                "run-order={}; multi-handle one={} two={} repeat={}".format(
                    native_digest, native_peak, early_digest,
                    one_digest, two_digest, repeat_digest))
            if prepared_digest != native_digest:
                raise RuntimeError("native/prepared digest mismatch")
    except (OSError, RuntimeError, subprocess.TimeoutExpired, ValueError,
            OverflowError) as error:
        print("parity test failed: {}".format(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
