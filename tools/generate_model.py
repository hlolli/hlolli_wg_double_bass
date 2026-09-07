#!/usr/bin/env python3
"""Check one fixed model file and embed it in the canonical C source."""

from __future__ import annotations

import argparse
import copy
from decimal import Decimal
import difflib
import hashlib
import json
import math
import os
from pathlib import Path
import tempfile
from typing import Any, Iterable, Mapping, Sequence


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MODEL = ROOT / "model" / "double_bass-v1.json"
DEFAULT_SOURCE = ROOT / "src" / "hlolli_wg_double_bass.c"
BEGIN_MARKER = "/* BEGIN GENERATED DOUBLE BASS MODEL DATA */"
END_MARKER = "/* END GENERATED DOUBLE BASS MODEL DATA */"

DONOR = {
    "repository": "https://github.com/hlolli/hlolli_wg_violin",
    "commit": "12f064bc9a84f7dd9cd44dc570bec69cc8e3ad72",
    "source_path": "src/hlolli_wg_violin.c",
    "source_sha256": (
        "6814ff43dc60ca5a13880c13cde1fce3"
        "f4b51888b0a285314e9c78e844b2362e"
    ),
    "embedded_model_sha256": (
        "9cec902aff31ce81edb134279d66a3585"
        "1ed13c30d8fd45ffe6c61fbe538d8f6"
    ),
}


class ModelError(ValueError):
    """The fixed model source is invalid."""


def fail(message: str) -> None:
    raise ModelError(message)


def unique_object(pairs: Sequence[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            fail(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def reject_constant(value: str) -> Any:
    fail(f"non-finite JSON number: {value}")


def load_model(path: Path) -> tuple[dict[str, Any], str]:
    raw = path.read_bytes()
    try:
        model = json.loads(
            raw.decode("utf-8"),
            parse_float=Decimal,
            parse_int=int,
            parse_constant=reject_constant,
            object_pairs_hook=unique_object,
        )
    except (UnicodeError, json.JSONDecodeError) as error:
        fail(f"{path}: {error}")
    if not isinstance(model, dict):
        fail(f"{path}: top level must be an object")
    return model, hashlib.sha256(raw).hexdigest()


def expect_keys(
    value: Any, expected: Iterable[str], label: str
) -> Mapping[str, Any]:
    if not isinstance(value, dict):
        fail(f"{label} must be an object")
    wanted = set(expected)
    found = set(value)
    if found != wanted:
        fail(
            f"{label} fields differ; extra={sorted(found - wanted)} "
            f"missing={sorted(wanted - found)}"
        )
    return value


def expect_array(value: Any, size: int, label: str) -> Sequence[Any]:
    if not isinstance(value, list) or len(value) != size:
        fail(f"{label} must contain {size} items")
    return value


def as_float(value: Any, label: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, Decimal)):
        fail(f"{label} must be a number")
    result = float(value)
    if not math.isfinite(result):
        fail(f"{label} must fit in a finite binary64 value")
    return result


def bounded_number(value: Any, low: float, high: float, label: str) -> float:
    result = as_float(value, label)
    if result < low or result > high:
        fail(f"{label} must be in [{low:.15g}, {high:.15g}]")
    return result


def bounded_fields(
    value: Any,
    bounds: Sequence[tuple[str, float, float]],
    label: str,
) -> Mapping[str, Any]:
    fields = tuple(field for field, _low, _high in bounds)
    result = expect_keys(value, fields, label)
    for field, low, high in bounds:
        bounded_number(result[field], low, high, f"{label}.{field}")
    return result


def bound_field_names(
    bounds: Sequence[tuple[str, float, float]],
) -> tuple[str, ...]:
    return tuple(field for field, _low, _high in bounds)


EXCITER_MAX_PULSE_SECONDS = 0.004

STRING_BOUNDS = (
    ("characteristic_impedance_kg_per_second", 0.01, 10.0),
    ("loss_time_constant_seconds", 0.01, 30.0),
    ("nut_cutoff_hz", 100.0, 100000.0),
    ("bridge_cutoff_hz", 100.0, 100000.0),
    ("nut_loss_fraction", 0.0, 1.0),
)
STRING_FIELDS = bound_field_names(STRING_BOUNDS)
SECOND_POLARIZATION_BOUNDS = (
    ("mix", 0.0, 0.95),
    ("detune_cents", -100.0, 100.0),
    ("loss_time_constant_seconds", 0.01, 30.0),
    ("bridge_cutoff_hz", 100.0, 100000.0),
)
SECOND_POLARIZATION_FIELDS = bound_field_names(SECOND_POLARIZATION_BOUNDS)
FRICTION_BOUNDS = (
    ("floor_mu", 0.001, 4.0),
    ("slow_gain", 0.0, 4.0),
    ("slow_speed", 1.0e-5, 4.0),
    ("fast_gain", 0.0, 4.0),
    ("fast_speed", 1.0e-5, 4.0),
)
FRICTION_FIELDS = bound_field_names(FRICTION_BOUNDS)
FORCE_MAP_BOUNDS = (
    ("mu_drop_floor", 0.001, 4.0),
    ("minimum_divisor", 1.0, 10000.0),
    ("control_low", 0.001, 0.99),
    ("control_high", 0.01, 0.999),
    ("normal_max_scale", 0.01, 2.0),
    ("extreme_max_scale", 0.01, 8.0),
)
FORCE_MAP_FIELDS = bound_field_names(FORCE_MAP_BOUNDS)
CONTACT_BOUNDS = (
    ("stiffness", 0.01, 1.0e7),
    ("damping", 0.001, 1.0e4),
    ("breakaway", 0.0, 1.0),
    ("dynamic_mix", 0.0, 1.0),
    ("memory_release_seconds", 1.0e-5, 10.0),
    ("thermal_attack_seconds", 1.0e-5, 10.0),
    ("thermal_release_seconds", 1.0e-5, 10.0),
    ("thermal_drop", 0.0, 0.95),
    ("thermal_work_scale", 1.0e-6, 100.0),
    ("hair_stiffness", 0.01, 1.0e7),
    ("hair_damping", 0.001, 1.0e4),
    ("hair_contact_mix", 0.0, 1.0),
)
CONTACT_FIELDS = bound_field_names(CONTACT_BOUNDS)
GESTURE_BOUNDS = (
    ("transition_seconds", 1.0e-5, 2.0),
    ("bow_change_seconds", 1.0e-5, 2.0),
    ("bow_change_contact_dip", 0.0, 1.0),
    ("bow_change_force_floor", 0.0, 1.0),
)
DETACHE_BOUNDS = (
    ("onset_seconds", 1.0e-5, 2.0),
    ("stroke_seconds", 1.0e-5, 2.0),
    ("force_base", -8.0, 8.0),
    ("force_attack", -8.0, 8.0),
    ("force_settle", -8.0, 8.0),
    ("speed_base", -8.0, 8.0),
    ("speed_attack", -8.0, 8.0),
    ("speed_settle", -8.0, 8.0),
    ("contact_attack_scale", 0.0, 8.0),
)
DETACHE_FIELDS = bound_field_names(DETACHE_BOUNDS)
MARTELE_BOUNDS = (
    ("preload_seconds", 1.0e-5, 2.0),
    ("acceleration_seconds", 1.0e-5, 2.0),
    ("stroke_seconds", 1.0e-5, 2.0),
    ("force_base", -8.0, 8.0),
    ("force_preload", -8.0, 8.0),
    ("force_acceleration", -8.0, 8.0),
    ("force_settle", -8.0, 8.0),
    ("speed_preload", -8.0, 8.0),
    ("speed_acceleration", -8.0, 8.0),
    ("speed_settle", -8.0, 8.0),
    ("contact_preload_scale", 0.0, 8.0),
)
MARTELE_FIELDS = bound_field_names(MARTELE_BOUNDS)
SPICCATO_BOUNDS = (
    ("stroke_fast_seconds", 1.0e-5, 2.0),
    ("stroke_slow_seconds", 1.0e-5, 2.0),
    ("force_base", -8.0, 8.0),
    ("force_bounce", -8.0, 8.0),
    ("speed_base", -8.0, 8.0),
    ("speed_bounce", -8.0, 8.0),
)
SPICCATO_FIELDS = bound_field_names(SPICCATO_BOUNDS)
TREMOLO_BOUNDS = (
    ("onset_seconds", 1.0e-5, 2.0),
    ("rate_min_hz", 0.01, 20.0),
    ("rate_max_hz", 0.01, 20.0),
    ("force_base", -8.0, 8.0),
    ("force_motion", -8.0, 8.0),
    ("contact_base", -8.0, 8.0),
    ("contact_motion", -8.0, 8.0),
)
TREMOLO_FIELDS = bound_field_names(TREMOLO_BOUNDS)
RELEASE_FIELDS = (
    "arco", "detache", "martele", "spiccato", "tremolo",
    "pizzicato_right", "pizzicato_left", "bartok", "battuto", "tratto",
)
RELEASE_BOUNDS = tuple((field, 0.01, 30.0) for field in RELEASE_FIELDS)
HARMONIC_BOUNDS = (
    ("attack_seconds", 1.0e-5, 2.0),
    ("release_seconds", 1.0e-5, 2.0),
    ("order_transition_seconds", 1.0e-5, 2.0),
)
EXCITER_BOUNDS = (
    ("noise_highpass", 0.0, 0.9999),
    ("contact_attack_seconds", 1.0e-6, 1.0),
    ("contact_release_seconds", 1.0e-6, 1.0),
    ("speed_smoothing_seconds", 1.0e-6, 1.0),
)
PIZZICATO_BOUNDS = (
    ("pulse_min_seconds", 0.0, 0.1),
    ("pulse_range_seconds", 0.0, 0.1),
    ("amplitude", 0.0, 8.0),
    ("noise_gain", 0.0, 8.0),
)
BARTOK_BOUNDS = (
    ("pulse_min_seconds", 0.0, 0.1),
    ("pulse_range_seconds", 0.0, 0.1),
    ("impact_delay_min_seconds", 0.0, 0.1),
    ("impact_delay_range_seconds", 0.0, 0.1),
    ("impact_min_seconds", 0.0, 0.1),
    ("impact_range_seconds", 0.0, 0.1),
    ("amplitude", 0.0, 8.0),
    ("noise_gain", 0.0, 8.0),
    ("impact_gain", 0.0, 8.0),
    ("impact_alternating_mix", 0.0, 1.0),
)
BATTUTO_BOUNDS = (
    ("collision_seconds", 1.0e-6, 0.1),
    ("noise_gain", 0.0, 8.0),
    ("speed_base", 0.0, 8.0),
    ("speed_force", 0.0, 8.0),
    ("stiffness_base", 0.01, 1.0e9),
    ("stiffness_force", 0.0, 1.0e9),
    ("damping_base", 0.0, 1.0e6),
    ("damping_force", 0.0, 1.0e6),
    ("mass_kg", 1.0e-6, 10.0),
)
TRATTO_BOUNDS = (
    ("speed_scale", 0.0, 2.0),
    ("transition_min", 1.0e-6, 2.0),
    ("transition_range", 0.0, 2.0),
    ("force_gain", 0.0, 8.0),
    ("grain_gain", 0.0, 8.0),
)
PIZZICATO_FIELDS = bound_field_names(PIZZICATO_BOUNDS)
BARTOK_FIELDS = bound_field_names(BARTOK_BOUNDS)
BATTUTO_FIELDS = bound_field_names(BATTUTO_BOUNDS)
TRATTO_FIELDS = bound_field_names(TRATTO_BOUNDS)
BODY_BOUNDS = (
    ("wet_gain", 0.0, 4.0),
    ("gain_smoothing_seconds", 1.0e-5, 10.0),
    ("bridge_radiation_cutoff_hz", 100.0, 100000.0),
    ("bridge_radiation_gain", 0.0, 1.0),
    ("body_decay_base", 0.01, 10.0),
    ("body_decay_range", 0.0, 10.0),
    ("mute_decay_scale", 0.0, 20.0),
    ("body_tone_depth", 0.0, 4.0),
    ("mute_low_attenuation", 0.0, 1.0),
    ("mute_high_attenuation", 0.0, 1.0),
    ("mute_level_attenuation", 0.0, 1.0),
)
BODY_FIELDS = bound_field_names(BODY_BOUNDS)
MODE_BOUNDS = (
    ("frequency_hz", 20.0, 100000.0),
    ("bandwidth_hz", 0.01, 100000.0),
    ("gain", 0.0, 100.0),
    ("pan", -1.0, 1.0),
)
MODE_FIELDS = bound_field_names(MODE_BOUNDS)


def validate_model(model: Mapping[str, Any]) -> None:
    expect_keys(
        model,
        (
            "$schema",
            "schema_version",
            "id",
            "display_name",
            "evidence_status",
            "donor",
            "strings",
            "bow",
            "gestures",
            "release_t60_seconds",
            "harmonics",
            "exciters",
            "coupling",
            "body",
        ),
        "model",
    )
    fixed = {
        "$schema": "hlolli-wg-double-bass-model-v2",
        "schema_version": 2,
        "id": "double_bass_v1",
        "display_name": "Double Bass v1",
        "evidence_status": "violin-derived",
    }
    for field, wanted in fixed.items():
        if model[field] != wanted:
            fail(f"model.{field} must be {wanted!r}")
    donor = expect_keys(model["donor"], DONOR, "model.donor")
    for field, wanted in DONOR.items():
        if donor[field] != wanted:
            fail(f"model.donor.{field} does not match the import record")

    strings = expect_array(model["strings"], 4, "model.strings")
    for index, string in enumerate(strings):
        values = expect_keys(
            string,
            STRING_FIELDS + ("second_polarization",),
            f"model.strings[{index}]",
        )
        for field, low, high in STRING_BOUNDS:
            bounded_number(
                values[field], low, high, f"model.strings[{index}].{field}"
            )
        bounded_fields(
            values["second_polarization"],
            SECOND_POLARIZATION_BOUNDS,
            f"model.strings[{index}].second_polarization",
        )

    bow = expect_keys(
        model["bow"],
        ("speed_scale", "friction", "force_map", "contact"),
        "model.bow",
    )
    bounded_number(bow["speed_scale"], 0.0, 2.0, "model.bow.speed_scale")
    friction = bounded_fields(
        bow["friction"], FRICTION_BOUNDS, "model.bow.friction"
    )
    force_map = bounded_fields(
        bow["force_map"], FORCE_MAP_BOUNDS, "model.bow.force_map"
    )
    bounded_fields(bow["contact"], CONTACT_BOUNDS, "model.bow.contact")

    friction_components = tuple(
        as_float(friction[field], f"model.bow.friction.{field}")
        for field in ("floor_mu", "slow_gain", "fast_gain")
    )
    friction_sum = sum(friction_components)
    generated_static_mu = float(
        sum(
            Decimal(str(friction[field]))
            for field in ("floor_mu", "slow_gain", "fast_gain")
        )
    )
    if generated_static_mu < 0.01 or generated_static_mu > 2.5:
        fail("model.bow.friction derived static_mu must be in [0.01, 2.5]")
    if abs(generated_static_mu - friction_sum) > 1.0e-12:
        fail("model.bow.friction derived static_mu does not match its parts")

    control_low = as_float(
        force_map["control_low"], "model.bow.force_map.control_low"
    )
    control_high = as_float(
        force_map["control_high"], "model.bow.force_map.control_high"
    )
    if control_low >= control_high:
        fail("bow force control_low must be below control_high")
    control_middle_span = float(
        Decimal(str(force_map["control_high"]))
        - Decimal(str(force_map["control_low"]))
    )
    control_high_span = float(
        Decimal("1") - Decimal(str(force_map["control_high"]))
    )
    if control_middle_span < 1.0e-6 or control_middle_span > 1.0:
        fail("model.bow.force_map derived control_middle_span is out of range")
    if control_high_span < 1.0e-6 or control_high_span > 1.0:
        fail("model.bow.force_map derived control_high_span is out of range")
    if abs(control_low + control_middle_span - control_high) > 1.0e-12:
        fail("model.bow.force_map derived control_middle_span does not match")
    if abs(control_high + control_high_span - 1.0) > 1.0e-12:
        fail("model.bow.force_map derived control_high_span does not match")
    normal_max_scale = as_float(
        force_map["normal_max_scale"], "model.bow.force_map.normal_max_scale"
    )
    extreme_max_scale = as_float(
        force_map["extreme_max_scale"], "model.bow.force_map.extreme_max_scale"
    )
    if extreme_max_scale < normal_max_scale:
        fail("model.bow.force_map.extreme_max_scale must not be below normal_max_scale")

    gestures = expect_keys(
        model["gestures"],
        (
            "transition_seconds",
            "bow_change_seconds",
            "bow_change_contact_dip",
            "bow_change_force_floor",
            "detache",
            "martele",
            "spiccato",
            "tremolo",
        ),
        "model.gestures",
    )
    for field, low, high in GESTURE_BOUNDS:
        bounded_number(gestures[field], low, high, f"model.gestures.{field}")

    detache = bounded_fields(
        gestures["detache"], DETACHE_BOUNDS, "model.gestures.detache"
    )
    detache_onset = as_float(
        detache["onset_seconds"], "model.gestures.detache.onset_seconds"
    )
    detache_stroke = as_float(
        detache["stroke_seconds"], "model.gestures.detache.stroke_seconds"
    )
    if detache_stroke < detache_onset:
        fail("model.gestures.detache.stroke_seconds must not be below onset_seconds")

    martele = bounded_fields(
        gestures["martele"], MARTELE_BOUNDS, "model.gestures.martele"
    )
    martele_preload = as_float(
        martele["preload_seconds"], "model.gestures.martele.preload_seconds"
    )
    martele_acceleration = as_float(
        martele["acceleration_seconds"],
        "model.gestures.martele.acceleration_seconds",
    )
    martele_stroke = as_float(
        martele["stroke_seconds"], "model.gestures.martele.stroke_seconds"
    )
    if martele_stroke < martele_preload + martele_acceleration:
        fail(
            "model.gestures.martele.stroke_seconds must cover preload and "
            "acceleration"
        )

    spiccato = bounded_fields(
        gestures["spiccato"], SPICCATO_BOUNDS, "model.gestures.spiccato"
    )
    stroke_fast = as_float(
        spiccato["stroke_fast_seconds"],
        "model.gestures.spiccato.stroke_fast_seconds",
    )
    stroke_slow = as_float(
        spiccato["stroke_slow_seconds"],
        "model.gestures.spiccato.stroke_slow_seconds",
    )
    stroke_range = float(
        Decimal(str(spiccato["stroke_slow_seconds"]))
        - Decimal(str(spiccato["stroke_fast_seconds"]))
    )
    if stroke_slow < stroke_fast:
        fail("model.gestures.spiccato slow stroke must not be below fast stroke")
    if stroke_range < 1.0e-5 or stroke_range > 2.0:
        fail("model.gestures.spiccato derived stroke range is out of range")
    if abs(stroke_fast + stroke_range - stroke_slow) > 1.0e-12:
        fail("model.gestures.spiccato derived stroke range does not match")

    tremolo = bounded_fields(
        gestures["tremolo"], TREMOLO_BOUNDS, "model.gestures.tremolo"
    )
    if as_float(
        tremolo["rate_min_hz"], "model.gestures.tremolo.rate_min_hz"
    ) > as_float(
        tremolo["rate_max_hz"], "model.gestures.tremolo.rate_max_hz"
    ):
        fail("tremolo rate bounds are reversed")

    bounded_fields(
        model["release_t60_seconds"],
        RELEASE_BOUNDS,
        "model.release_t60_seconds",
    )
    harmonics = expect_keys(
        model["harmonics"],
        (
            "touch_level",
            "attack_seconds",
            "release_seconds",
            "order_transition_seconds",
        ),
        "model.harmonics",
    )
    for index, value in enumerate(
        expect_array(harmonics["touch_level"], 7, "model.harmonics.touch_level")
    ):
        bounded_number(
            value, 0.0, 1.0, f"model.harmonics.touch_level[{index}]"
        )
    for field, low, high in HARMONIC_BOUNDS:
        bounded_number(harmonics[field], low, high, f"model.harmonics.{field}")

    exciters = expect_keys(
        model["exciters"],
        (
            "noise_highpass",
            "contact_attack_seconds",
            "contact_release_seconds",
            "speed_smoothing_seconds",
            "pizzicato_right",
            "pizzicato_left",
            "bartok",
            "battuto",
            "tratto",
        ),
        "model.exciters",
    )
    for field, low, high in EXCITER_BOUNDS:
        bounded_number(exciters[field], low, high, f"model.exciters.{field}")
    for name in ("pizzicato_right", "pizzicato_left"):
        pizzicato = bounded_fields(
            exciters[name], PIZZICATO_BOUNDS, f"model.exciters.{name}"
        )
        pulse_total = as_float(
            pizzicato["pulse_min_seconds"],
            f"model.exciters.{name}.pulse_min_seconds",
        ) + as_float(
            pizzicato["pulse_range_seconds"],
            f"model.exciters.{name}.pulse_range_seconds",
        )
        if pulse_total > EXCITER_MAX_PULSE_SECONDS:
            fail(
                f"model.exciters.{name} pulse duration must not exceed "
                f"{EXCITER_MAX_PULSE_SECONDS} seconds"
            )

    bartok = bounded_fields(
        exciters["bartok"], BARTOK_BOUNDS, "model.exciters.bartok"
    )
    for minimum, span, label in (
        ("pulse_min_seconds", "pulse_range_seconds", "pulse"),
        (
            "impact_delay_min_seconds",
            "impact_delay_range_seconds",
            "impact delay",
        ),
        ("impact_min_seconds", "impact_range_seconds", "impact"),
    ):
        duration = as_float(
            bartok[minimum], f"model.exciters.bartok.{minimum}"
        ) + as_float(bartok[span], f"model.exciters.bartok.{span}")
        if duration > EXCITER_MAX_PULSE_SECONDS:
            fail(
                f"model.exciters.bartok {label} duration must not exceed "
                f"{EXCITER_MAX_PULSE_SECONDS} seconds"
            )
    bounded_fields(
        exciters["battuto"], BATTUTO_BOUNDS, "model.exciters.battuto"
    )
    bounded_fields(exciters["tratto"], TRATTO_BOUNDS, "model.exciters.tratto")

    coupling = expect_keys(
        model["coupling"],
        ("bridge_coefficients", "sympathetic_open_scale"),
        "model.coupling",
    )
    bounded_number(
        coupling["sympathetic_open_scale"],
        0.0,
        100.0,
        "model.coupling.sympathetic_open_scale",
    )
    matrix = expect_array(
        coupling["bridge_coefficients"],
        4,
        "model.coupling.bridge_coefficients",
    )
    numeric_matrix: list[list[float]] = []
    for row_index, row in enumerate(matrix):
        numeric_matrix.append(
            [
                bounded_number(
                    value,
                    0.0,
                    0.1,
                    f"model.coupling.bridge_coefficients"
                    f"[{row_index}][{column_index}]",
                )
                for column_index, value in enumerate(
                    expect_array(
                        row,
                        4,
                        f"model.coupling.bridge_coefficients[{row_index}]",
                    )
                )
            ]
        )
    for row in range(4):
        if numeric_matrix[row][row] != 0.0:
            fail("bridge matrix diagonal must be zero")
        for column in range(row + 1, 4):
            if numeric_matrix[row][column] != numeric_matrix[column][row]:
                fail("bridge matrix must be symmetric")

    body = expect_keys(model["body"], BODY_FIELDS + ("modes",), "model.body")
    for field, low, high in BODY_BOUNDS:
        bounded_number(body[field], low, high, f"model.body.{field}")
    if as_float(
        body["body_decay_range"], "model.body.body_decay_range"
    ) >= as_float(body["body_decay_base"], "model.body.body_decay_base"):
        fail("model.body.body_decay_range must be below body_decay_base")
    modes = expect_array(body["modes"], 12, "model.body.modes")
    previous_frequency = 0.0
    for index, mode in enumerate(modes):
        values = bounded_fields(
            mode, MODE_BOUNDS, f"model.body.modes[{index}]"
        )
        frequency = as_float(
            values["frequency_hz"], f"model.body.modes[{index}].frequency_hz"
        )
        bandwidth = as_float(
            values["bandwidth_hz"], f"model.body.modes[{index}].bandwidth_hz"
        )
        if frequency <= previous_frequency:
            fail("body mode frequencies must increase")
        if bandwidth > 2.0 * frequency:
            fail("body mode bandwidth is invalid")
        previous_frequency = frequency


def c_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=True)


def c_number(value: Any) -> str:
    result = as_float(value, "generated number")
    if result == 0.0:
        return "0.0"
    text = repr(result)
    if "." not in text and "e" not in text.lower():
        text += ".0"
    return text


def values(item: Mapping[str, Any], fields: Sequence[str]) -> str:
    return ", ".join(c_number(item[field]) for field in fields)


def render_model(model: Mapping[str, Any], source_hash: str) -> str:
    bow = model["bow"]
    gestures = model["gestures"]
    exciters = model["exciters"]
    coupling = model["coupling"]
    body = model["body"]
    friction = bow["friction"]
    force_map = bow["force_map"]
    lines = [
        BEGIN_MARKER,
        "/* Generated from model/double_bass-v1.json. Do not edit by hand. */",
        f"/* Evidence status: {model['evidence_status']}. */",
        "static const WG_DOUBLE_BASS_MODEL wg_double_bass_model = {",
        "    .schema_version = WG_DOUBLE_BASS_MODEL_SCHEMA_VERSION,",
        f"    .id = {c_string(model['id'])},",
        f"    .display_name = {c_string(model['display_name'])},",
        f"    .evidence_status = {c_string(model['evidence_status'])},",
        "    .source_sha256 =",
        f"        {c_string(source_hash)},",
        "    .strings = {",
    ]
    for string in model["strings"]:
        lines.append(
            "        {"
            + values(string, STRING_FIELDS)
            + ", {"
            + values(
                string["second_polarization"], SECOND_POLARIZATION_FIELDS
            )
            + "}},"
        )
    static_mu = sum(Decimal(str(friction[field])) for field in ("floor_mu", "slow_gain", "fast_gain"))
    lines.extend([
        "    },",
        "    .bow = {",
        f"        .speed_scale = {c_number(bow['speed_scale'])},",
        f"        .friction = {{{values(friction, FRICTION_FIELDS)}, {c_number(static_mu)}}},",
        "        .force_map = {" + values(force_map, ("mu_drop_floor", "minimum_divisor", "control_low", "control_high")) + ", " + c_number(Decimal(str(force_map["control_high"])) - Decimal(str(force_map["control_low"]))) + ", " + c_number(Decimal("1") - Decimal(str(force_map["control_high"]))) + ", " + values(force_map, ("normal_max_scale", "extreme_max_scale")) + "},",
        f"        .contact = {{{values(bow['contact'], CONTACT_FIELDS)}}},",
        "    },",
        "    .gestures = {",
        f"        .transition_seconds = {c_number(gestures['transition_seconds'])},",
        f"        .bow_change_seconds = {c_number(gestures['bow_change_seconds'])},",
        f"        .bow_change_contact_dip = {c_number(gestures['bow_change_contact_dip'])},",
        f"        .bow_change_force_floor = {c_number(gestures['bow_change_force_floor'])},",
        f"        .detache = {{{values(gestures['detache'], DETACHE_FIELDS)}}},",
        f"        .martele = {{{values(gestures['martele'], MARTELE_FIELDS)}}},",
        "        .spiccato = {" + values(gestures["spiccato"], ("stroke_fast_seconds", "stroke_slow_seconds")) + ", " + c_number(Decimal(str(gestures["spiccato"]["stroke_slow_seconds"])) - Decimal(str(gestures["spiccato"]["stroke_fast_seconds"]))) + ", " + values(gestures["spiccato"], ("force_base", "force_bounce", "speed_base", "speed_bounce")) + "},",
        f"        .tremolo = {{{values(gestures['tremolo'], TREMOLO_FIELDS)}}},",
        "    },",
        f"    .release_t60_seconds = {{{values(model['release_t60_seconds'], RELEASE_FIELDS)}}},",
        "    .harmonics = {",
        "        .touch_level = {" + ", ".join(c_number(value) for value in model["harmonics"]["touch_level"]) + "},",
        f"        .attack_seconds = {c_number(model['harmonics']['attack_seconds'])},",
        f"        .release_seconds = {c_number(model['harmonics']['release_seconds'])},",
        f"        .order_transition_seconds = {c_number(model['harmonics']['order_transition_seconds'])},",
        "    },",
        "    .exciters = {",
        f"        .noise_highpass = {c_number(exciters['noise_highpass'])},",
        f"        .contact_attack_seconds = {c_number(exciters['contact_attack_seconds'])},",
        f"        .contact_release_seconds = {c_number(exciters['contact_release_seconds'])},",
        f"        .speed_smooth_seconds = {c_number(exciters['speed_smoothing_seconds'])},",
        f"        .pizzicato_right = {{{values(exciters['pizzicato_right'], PIZZICATO_FIELDS)}}},",
        f"        .pizzicato_left = {{{values(exciters['pizzicato_left'], PIZZICATO_FIELDS)}}},",
        f"        .bartok = {{{values(exciters['bartok'], BARTOK_FIELDS)}}},",
        f"        .battuto = {{{values(exciters['battuto'], BATTUTO_FIELDS)}}},",
        f"        .tratto = {{{values(exciters['tratto'], TRATTO_FIELDS)}}},",
        "    },",
        "    .coupling = {",
        "        .bridge_coefficients = {",
    ])
    for row in coupling["bridge_coefficients"]:
        lines.append("            {" + ", ".join(c_number(value) for value in row) + "},")
    lines.extend([
        "        },",
        f"        .sympathetic_open_scale = {c_number(coupling['sympathetic_open_scale'])},",
        "    },",
        "    .body = {",
    ])
    for field in BODY_FIELDS:
        lines.append(f"        .{field} = {c_number(body[field])},")
    lines.append("        .modes = {")
    for mode in body["modes"]:
        lines.append(f"            {{{values(mode, MODE_FIELDS)}}},")
    lines.extend(["        },", "    },", "};", END_MARKER])
    return "\n".join(lines)


def replace_generated_block(source: str, block: str) -> str:
    if source.count(BEGIN_MARKER) != 1 or source.count(END_MARKER) != 1:
        fail("C source must contain one model marker pair")
    begin = source.index(BEGIN_MARKER)
    end = source.index(END_MARKER) + len(END_MARKER)
    if begin >= end:
        fail("C model markers are out of order")
    return source[:begin] + block + source[end:]


def write_atomic(path: Path, text: str) -> None:
    mode = path.stat().st_mode
    temporary: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            "w", encoding="utf-8", newline="\n", dir=path.parent, delete=False
        ) as stream:
            temporary = stream.name
            stream.write(text)
            stream.flush()
            os.fsync(stream.fileno())
        os.chmod(temporary, mode)
        os.replace(temporary, path)
    finally:
        if temporary is not None and os.path.exists(temporary):
            os.unlink(temporary)


def expect_failure(
    label: str, action: Any, expected_message: str | None = None
) -> None:
    try:
        action()
    except ModelError as error:
        if expected_message is not None and expected_message not in str(error):
            fail(
                f"self-test got the wrong error for {label}: {error}; "
                f"wanted {expected_message!r}"
            )
        return
    fail(f"self-test did not reject {label}")


def run_self_tests(model: Mapping[str, Any]) -> None:
    validate_model(model)

    def changed(path: Sequence[Any], value: Any) -> Mapping[str, Any]:
        sample = copy.deepcopy(model)
        target: Any = sample
        for key in path[:-1]:
            target = target[key]
        target[path[-1]] = value
        return sample

    def changed_many(
        changes: Sequence[tuple[Sequence[Any], Any]],
    ) -> Mapping[str, Any]:
        sample = copy.deepcopy(model)
        for path, value in changes:
            target: Any = sample
            for key in path[:-1]:
                target = target[key]
            target[path[-1]] = value
        return sample

    bound_groups = [
        (("bow", "friction"), FRICTION_BOUNDS),
        (("bow", "force_map"), FORCE_MAP_BOUNDS),
        (("bow", "contact"), CONTACT_BOUNDS),
        (("gestures",), GESTURE_BOUNDS),
        (("gestures", "detache"), DETACHE_BOUNDS),
        (("gestures", "martele"), MARTELE_BOUNDS),
        (("gestures", "spiccato"), SPICCATO_BOUNDS),
        (("gestures", "tremolo"), TREMOLO_BOUNDS),
        (("release_t60_seconds",), RELEASE_BOUNDS),
        (("harmonics",), HARMONIC_BOUNDS),
        (("exciters",), EXCITER_BOUNDS),
        (("exciters", "pizzicato_right"), PIZZICATO_BOUNDS),
        (("exciters", "pizzicato_left"), PIZZICATO_BOUNDS),
        (("exciters", "bartok"), BARTOK_BOUNDS),
        (("exciters", "battuto"), BATTUTO_BOUNDS),
        (("exciters", "tratto"), TRATTO_BOUNDS),
        (("body",), BODY_BOUNDS),
    ]
    bound_groups.extend(
        (("strings", index), STRING_BOUNDS) for index in range(4)
    )
    bound_groups.extend(
        (("strings", index, "second_polarization"),
         SECOND_POLARIZATION_BOUNDS)
        for index in range(4)
    )
    bound_groups.extend(
        (("body", "modes", index), MODE_BOUNDS) for index in range(12)
    )
    for path, bounds in bound_groups:
        for field, low, high in bounds:
            for side, value in (
                ("low", Decimal(str(low)) - Decimal("1")),
                ("high", Decimal(str(high)) + Decimal("1")),
            ):
                label = f"{'.'.join(str(key) for key in path)}.{field} {side} bound"
                sample = changed(path + (field,), value)
                expect_failure(label, lambda sample=sample: validate_model(sample))

    for index in range(7):
        sample = changed(("harmonics", "touch_level", index), Decimal("2"))
        expect_failure(
            f"harmonics touch level {index}",
            lambda sample=sample: validate_model(sample),
        )
    for row in range(4):
        for column in range(4):
            sample = changed(
                ("coupling", "bridge_coefficients", row, column),
                Decimal("0.2"),
            )
            expect_failure(
                f"bridge coefficient {row},{column} range",
                lambda sample=sample: validate_model(sample),
            )
    for label, path, low, high in (
        ("bow speed scale", ("bow", "speed_scale"), 0.0, 2.0),
        (
            "sympathetic open scale",
            ("coupling", "sympathetic_open_scale"),
            0.0,
            100.0,
        ),
    ):
        for side, value in (
            ("low", Decimal(str(low)) - Decimal("1")),
            ("high", Decimal(str(high)) + Decimal("1")),
        ):
            sample = changed(path, value)
            expect_failure(
                f"{label} {side} bound",
                lambda sample=sample: validate_model(sample),
            )

    cases = (
        ("evidence status", changed(("evidence_status",), "bass-derived")),
        ("donor hash", changed(("donor", "source_sha256"), "0" * 64)),
        ("string count", changed(("strings",), list(model["strings"][:3]))),
        ("friction sum", changed(("bow", "friction", "fast_gain"), Decimal("2.0"))),
        (
            "friction sum floor",
            changed_many(
                (
                    (("bow", "friction", "floor_mu"), Decimal("0.001")),
                    (("bow", "friction", "slow_gain"), Decimal("0")),
                    (("bow", "friction", "fast_gain"), Decimal("0")),
                )
            ),
        ),
        ("force bounds", changed(("bow", "force_map", "control_low"), Decimal("0.9"))),
        (
            "force middle span",
            changed(("bow", "force_map", "control_high"), Decimal("0.1000005")),
        ),
        (
            "force scale order",
            changed(("bow", "force_map", "extreme_max_scale"), Decimal("0.5")),
        ),
        (
            "detache stroke order",
            changed(("gestures", "detache", "stroke_seconds"), Decimal("0.006")),
        ),
        (
            "martele stroke order",
            changed(("gestures", "martele", "stroke_seconds"), Decimal("0.01")),
        ),
        ("spiccato order", changed(("gestures", "spiccato", "stroke_slow_seconds"), Decimal("0.01"))),
        (
            "spiccato range",
            changed(
                ("gestures", "spiccato", "stroke_slow_seconds"),
                Decimal("0.032005"),
            ),
        ),
        (
            "tremolo rate order",
            changed(("gestures", "tremolo", "rate_min_hz"), Decimal("15")),
        ),
        (
            "right pizzicato pulse total",
            changed(
                ("exciters", "pizzicato_right", "pulse_range_seconds"),
                Decimal("0.004"),
            ),
        ),
        (
            "left pizzicato pulse total",
            changed(
                ("exciters", "pizzicato_left", "pulse_range_seconds"),
                Decimal("0.004"),
            ),
        ),
        (
            "bartok pulse total",
            changed(
                ("exciters", "bartok", "pulse_range_seconds"),
                Decimal("0.004"),
            ),
        ),
        (
            "bartok impact delay total",
            changed(
                ("exciters", "bartok", "impact_delay_range_seconds"),
                Decimal("0.004"),
            ),
        ),
        (
            "bartok impact total",
            changed(
                ("exciters", "bartok", "impact_range_seconds"),
                Decimal("0.004"),
            ),
        ),
        ("bridge diagonal", changed(("coupling", "bridge_coefficients", 0, 0), Decimal("0.001"))),
        ("bridge symmetry", changed(("coupling", "bridge_coefficients", 0, 1), Decimal("0.001"))),
        (
            "body decay order",
            changed(("body", "body_decay_range"), Decimal("1.0")),
        ),
        ("mode order", changed(("body", "modes", 1, "frequency_hz"), Decimal("200"))),
        (
            "mode bandwidth",
            changed(("body", "modes", 0, "bandwidth_hz"), Decimal("531")),
        ),
    )
    for label, sample in cases:
        expect_failure(label, lambda sample=sample: validate_model(sample))
    message_cases = (
        (
            "bow speed scale message",
            changed(("bow", "speed_scale"), Decimal("99")),
            "model.bow.speed_scale must be in [0, 2]",
        ),
        (
            "release time message",
            changed(("release_t60_seconds", "arco"), Decimal("-1")),
            "model.release_t60_seconds.arco must be in [0.01, 30]",
        ),
        (
            "body wet gain message",
            changed(("body", "wet_gain"), Decimal("-1")),
            "model.body.wet_gain must be in [0, 4]",
        ),
        (
            "body mute attenuation message",
            changed(("body", "mute_low_attenuation"), Decimal("2")),
            "model.body.mute_low_attenuation must be in [0, 1]",
        ),
    )
    for label, sample, message in message_cases:
        expect_failure(
            label,
            lambda sample=sample: validate_model(sample),
            message,
        )
    expect_failure("duplicate key", lambda: unique_object((("x", 1), ("x", 2))))
    valid_source = f"{BEGIN_MARKER}\n{END_MARKER}\n"
    replace_generated_block(valid_source, f"{BEGIN_MARKER}\n{END_MARKER}")
    expect_failure("missing marker", lambda: replace_generated_block("", ""))
    print("fixed model generator self-test passed")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", type=Path, default=DEFAULT_MODEL)
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--self-test", action="store_true")
    return parser.parse_args()


def main() -> int:
    options = parse_args()
    try:
        model, source_hash = load_model(options.model)
        validate_model(model)
        if options.self_test:
            run_self_tests(model)
            return 0
        block = render_model(model, source_hash)
        source = options.source.read_text(encoding="utf-8")
        expected = replace_generated_block(source, block)
        if options.check:
            if expected != source:
                difference = difflib.unified_diff(
                    source.splitlines(),
                    expected.splitlines(),
                    fromfile=str(options.source),
                    tofile="generated",
                    lineterm="",
                )
                print("\n".join(list(difference)[:120]))
                fail("generated C model data is stale")
            print(f"fixed model check passed: {source_hash}")
            return 0
        write_atomic(options.source, expected)
        print(f"embedded fixed model {source_hash} in {options.source}")
        return 0
    except (OSError, UnicodeError, ModelError) as error:
        print(f"generate_model.py: {error}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
