#!/usr/bin/env python3
"""Derive the mechanical-burst-off module for the optional legato demo.

This is an explicit alternative module, not the standard sound default. Keep RNG advancement,
contact and friction noise intact, as in the original audition declaration.
"""

from __future__ import annotations

import argparse
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    options = parser.parse_args()
    if options.source.resolve() == options.output.resolve():
        parser.error("the demo copy must not overwrite the canonical source")
    source = options.source.read_text(encoding="utf-8")
    injection = (
        "noise = noise_level * string->bow_noise_highpass +\n"
        "        string->mechanical_envelope *"
    )
    if source.count(injection) != 1:
        parser.error("production mechanical injection changed; review the fixture")
    candidate = source.replace(
        injection, injection.replace(
            "string->mechanical_envelope", "0.0 * string->mechanical_envelope"))
    options.output.parent.mkdir(parents=True, exist_ok=True)
    options.output.write_text(candidate, encoding="utf-8")


if __name__ == "__main__":
    main()
