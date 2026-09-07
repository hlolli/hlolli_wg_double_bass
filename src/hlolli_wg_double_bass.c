/*
  SPDX-License-Identifier: GPL-3.0-only

  Sample-free double-bass model for Csound 7.
  One handle owns four strings and their shared bridge and body.
  Native and WASM builds use this file.
*/

#include <csdl.h>

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if !defined(CS_VERSION) || CS_VERSION < 7
#error "hlolli_wg_double_bass requires Csound 7 or newer"
#endif

#define WG_DOUBLE_BASS_STRINGS 4U
#define WG_DOUBLE_BASS_MANAGER_NAME "::hlolli_wg_double_bass::manager_v2::"
#define WG_DOUBLE_BASS_RENDERER_DECAY_SECONDS 0.75
#define WG_DOUBLE_BASS_TWO_PI 6.283185307179586476925286766559
#define WG_DOUBLE_BASS_MAX_FREQUENCY_RATIO 0.20
#define WG_DOUBLE_BASS_RAIL_MARGIN 8U
#define WG_DOUBLE_BASS_DELAY_SLEW 0.25
#define WG_DOUBLE_BASS_DELAY_ACCEL_SECONDS 0.0006
#define WG_DOUBLE_BASS_GAP_EXACT_RAILS 2U
#define WG_DOUBLE_BASS_GAP_CLEAR_SECONDS 6.0
#define WG_DOUBLE_BASS_WAVE_LIMIT 4.0
#define WG_DOUBLE_BASS_DRY_GAIN 0.02
#define WG_DOUBLE_BASS_BRIDGE_DYNAMIC_CUTOFF_HZ 12.0
#define WG_DOUBLE_BASS_MIN_BRANCH_DELAY 1.0
#define WG_DOUBLE_BASS_FRICTION_LUT_SIZE 2049U
#define WG_DOUBLE_BASS_FRICTION_MAX_SPEED 2.0
#define WG_DOUBLE_BASS_BOW_SCAN_STEPS 16U
#define WG_DOUBLE_BASS_BOW_SOLVE_STEPS 12U
#define WG_DOUBLE_BASS_BOW_RECOVERY_FAILURES 8U
#define WG_DOUBLE_BASS_BOW_RECOVERY_SAMPLES 64U
#define WG_DOUBLE_BASS_BODY_MODES 12U
#define WG_DOUBLE_BASS_BODY_DRIVE_LIMIT 8.0
#define WG_DOUBLE_BASS_BODY_STATE_LIMIT 64.0
#define WG_DOUBLE_BASS_BODY_OUTPUT_LIMIT 4.0
#define WG_DOUBLE_BASS_FINGER_STOP_EPSILON 1.0e-7
#define WG_DOUBLE_BASS_FINGER_SETTLE_EPSILON 1.0e-8
#define WG_DOUBLE_BASS_FINGER_NOISE_LIMIT 0.004
#define WG_DOUBLE_BASS_EXCITER_LIMIT 1.25
#define WG_DOUBLE_BASS_EXCITER_MIN_PULSE_SAMPLES 4U
#define WG_DOUBLE_BASS_EXCITER_MAX_PULSE_SECONDS 0.004
#define WG_DOUBLE_BASS_RELEASE_LEVEL 0.001
#define WG_DOUBLE_BASS_RELEASE_ENGAGE_SECONDS 0.00035
#define WG_DOUBLE_BASS_HARMONIC_MIN_ORDER 2U
#define WG_DOUBLE_BASS_HARMONIC_MAX_ORDER 8U
#define WG_DOUBLE_BASS_PITCH_SNAP_EPSILONS 8.0
#define WG_DOUBLE_BASS_GESTURE_SPEED_EPSILON 0.015
#define WG_DOUBLE_BASS_CONTACT_MEMORY_LIMIT 0.002
#define WG_DOUBLE_BASS_HAIR_SPEED_LIMIT 0.03
#define WG_DOUBLE_BASS_HAIR_DISPLACEMENT_LIMIT 0.0015
#if !defined(WG_DOUBLE_BASS_THERMAL_DROP)
#define WG_DOUBLE_BASS_THERMAL_DROP 0.08
#endif
#define WG_DOUBLE_BASS_BOW_MECHANICS_LIMIT 0.03
#define WG_DOUBLE_BASS_BOARD_VELOCITY_LIMIT 0.008
#define WG_DOUBLE_BASS_COUPLING_LIMIT 0.0015
#define WG_DOUBLE_BASS_BOARD_CLEARANCE 0.00065
#define WG_DOUBLE_BASS_BOARD_DISPLACEMENT_LIMIT 0.004
#define WG_DOUBLE_BASS_BOARD_STIFFNESS 350000.0
#define WG_DOUBLE_BASS_BOARD_DAMPING 90.0
#define WG_DOUBLE_BASS_STRANGE_COUPLING 4.0
#define WG_DOUBLE_BASS_STRANGE_OUTPUT_LIMIT 0.018
#define WG_DOUBLE_BASS_STRANGE_AIR_LIMIT 0.006
#define WG_DOUBLE_BASS_STRANGE_SUBHARMONIC_LIMIT 0.006
#define WG_DOUBLE_BASS_STRANGE_SQUEAL_LIMIT 0.008
#define WG_DOUBLE_BASS_DEFAULT_REFERENCE_PITCH_HZ 440.0
#define WG_DOUBLE_BASS_MIN_REFERENCE_PITCH_HZ 380.0
#define WG_DOUBLE_BASS_MAX_REFERENCE_PITCH_HZ 480.0
#define WG_DOUBLE_BASS_A440_NORMAL_MAX_FREQUENCY 391.99543598174927
#define WG_DOUBLE_BASS_NAN ((double)NAN)

/* Sounding 12-TET E1, A1, D2, and G2 at A4 = 440 Hz. The string, bow,
   bridge, and body data below remain violin-derived. */
static const double wg_double_bass_a440_open_frequencies[WG_DOUBLE_BASS_STRINGS] = {
    41.20344461410875,
    55.0,
    73.41619197935188,
    97.99885899543733,
};

enum {
  WG_DOUBLE_BASS_BOW_OFF = 0,
  WG_DOUBLE_BASS_BOW_NO_MOTION = 1,
  WG_DOUBLE_BASS_BOW_STICK = 2,
  WG_DOUBLE_BASS_BOW_SLIP = 3,
  WG_DOUBLE_BASS_BOW_SCRATCH = 4,
};

enum {
  WG_DOUBLE_BASS_FINGER_STILL = 0,
  WG_DOUBLE_BASS_FINGER_ARRIVING = 1,
  WG_DOUBLE_BASS_FINGER_RELEASING = 2,
  WG_DOUBLE_BASS_FINGER_SHIFTING = 3,
};

enum {
  WG_DOUBLE_BASS_ARTICULATION_ARCO = 0,
  WG_DOUBLE_BASS_ARTICULATION_DETACHE = 1,
  WG_DOUBLE_BASS_ARTICULATION_MARTELE = 2,
  WG_DOUBLE_BASS_ARTICULATION_SPICCATO = 3,
  WG_DOUBLE_BASS_ARTICULATION_TREMOLO = 4,
  WG_DOUBLE_BASS_ARTICULATION_PIZZICATO = 5,
  WG_DOUBLE_BASS_ARTICULATION_BARTOK = 6,
  WG_DOUBLE_BASS_ARTICULATION_BATTUTO = 7,
  WG_DOUBLE_BASS_ARTICULATION_TRATTO = 8,
  WG_DOUBLE_BASS_ARTICULATION_MAX = 15,
};

typedef struct {
  double frequency;
  double bandwidth;
  double gain;
  double pan;
} WG_DOUBLE_BASS_BODY_MODE_SPEC;

typedef struct {
  double mix;
  double detune_cents;
  double loss_time_constant_seconds;
  double bridge_cutoff_hz;
} WG_DOUBLE_BASS_POLARIZATION_MODEL;

typedef struct {
  double characteristic_impedance;
  double loss_time_constant_seconds;
  double nut_cutoff_hz;
  double bridge_cutoff_hz;
  double nut_loss_fraction;
  WG_DOUBLE_BASS_POLARIZATION_MODEL second_polarization;
} WG_DOUBLE_BASS_STRING_MODEL;

typedef struct {
  double floor_mu;
  double slow_gain;
  double slow_speed;
  double fast_gain;
  double fast_speed;
  double static_mu;
} WG_DOUBLE_BASS_FRICTION_MODEL;

typedef struct {
  double mu_drop_floor;
  double minimum_divisor;
  double control_low;
  double control_high;
  double control_middle_span;
  double control_high_span;
  double normal_max_scale;
  double extreme_max_scale;
} WG_DOUBLE_BASS_BOW_FORCE_MAP_MODEL;

typedef struct {
  double stiffness;
  double damping;
  double breakaway;
  double dynamic_mix;
  double memory_release_seconds;
  double thermal_attack_seconds;
  double thermal_release_seconds;
  double thermal_drop;
  double thermal_work_scale;
  double hair_stiffness;
  double hair_damping;
  double hair_contact_mix;
} WG_DOUBLE_BASS_BOW_CONTACT_MODEL;

typedef struct {
  double speed_scale;
  WG_DOUBLE_BASS_FRICTION_MODEL friction;
  WG_DOUBLE_BASS_BOW_FORCE_MAP_MODEL force_map;
  WG_DOUBLE_BASS_BOW_CONTACT_MODEL contact;
} WG_DOUBLE_BASS_BOW_MODEL;

typedef struct {
  double onset_seconds;
  double stroke_seconds;
  double force_base;
  double force_attack;
  double force_settle;
  double speed_base;
  double speed_attack;
  double speed_settle;
  double contact_attack_scale;
} WG_DOUBLE_BASS_DETACHE_MODEL;

typedef struct {
  double preload_seconds;
  double acceleration_seconds;
  double stroke_seconds;
  double force_base;
  double force_preload;
  double force_acceleration;
  double force_settle;
  double speed_preload;
  double speed_acceleration;
  double speed_settle;
  double contact_preload_scale;
} WG_DOUBLE_BASS_MARTELE_MODEL;

typedef struct {
  double stroke_fast_seconds;
  double stroke_slow_seconds;
  double stroke_range_seconds;
  double force_base;
  double force_bounce;
  double speed_base;
  double speed_bounce;
} WG_DOUBLE_BASS_SPICCATO_MODEL;

typedef struct {
  double onset_seconds;
  double rate_min_hz;
  double rate_max_hz;
  double force_base;
  double force_motion;
  double contact_base;
  double contact_motion;
} WG_DOUBLE_BASS_TREMOLO_MODEL;

typedef struct {
  double transition_seconds;
  double bow_change_seconds;
  double bow_change_contact_dip;
  double bow_change_force_floor;
  WG_DOUBLE_BASS_DETACHE_MODEL detache;
  WG_DOUBLE_BASS_MARTELE_MODEL martele;
  WG_DOUBLE_BASS_SPICCATO_MODEL spiccato;
  WG_DOUBLE_BASS_TREMOLO_MODEL tremolo;
} WG_DOUBLE_BASS_GESTURE_MODEL;

typedef struct {
  double arco;
  double detache;
  double martele;
  double spiccato;
  double tremolo;
  double pizzicato_right;
  double pizzicato_left;
  double bartok;
  double battuto;
  double tratto;
} WG_DOUBLE_BASS_RELEASE_T60_MODEL;

typedef struct {
  double touch_level[WG_DOUBLE_BASS_HARMONIC_MAX_ORDER -
                     WG_DOUBLE_BASS_HARMONIC_MIN_ORDER + 1U];
  double attack_seconds;
  double release_seconds;
  double order_transition_seconds;
} WG_DOUBLE_BASS_HARMONICS_MODEL;

typedef struct {
  double pulse_min_seconds;
  double pulse_range_seconds;
  double amplitude;
  double noise_gain;
} WG_DOUBLE_BASS_PIZZICATO_MODEL;

typedef struct {
  double pulse_min_seconds;
  double pulse_range_seconds;
  double impact_delay_min_seconds;
  double impact_delay_range_seconds;
  double impact_min_seconds;
  double impact_range_seconds;
  double amplitude;
  double noise_gain;
  double impact_gain;
  double impact_alternating_mix;
} WG_DOUBLE_BASS_BARTOK_MODEL;

typedef struct {
  double collision_seconds;
  double noise_gain;
  double speed_base;
  double speed_force;
  double stiffness_base;
  double stiffness_force;
  double damping_base;
  double damping_force;
  double mass_kg;
} WG_DOUBLE_BASS_BATTUTO_MODEL;

typedef struct {
  double speed_scale;
  double transition_min;
  double transition_range;
  double force_gain;
  double grain_gain;
} WG_DOUBLE_BASS_TRATTO_MODEL;

typedef struct {
  double noise_highpass;
  double contact_attack_seconds;
  double contact_release_seconds;
  double speed_smooth_seconds;
  WG_DOUBLE_BASS_PIZZICATO_MODEL pizzicato_right;
  WG_DOUBLE_BASS_PIZZICATO_MODEL pizzicato_left;
  WG_DOUBLE_BASS_BARTOK_MODEL bartok;
  WG_DOUBLE_BASS_BATTUTO_MODEL battuto;
  WG_DOUBLE_BASS_TRATTO_MODEL tratto;
} WG_DOUBLE_BASS_EXCITER_MODEL;

typedef struct {
  double bridge_coefficients[WG_DOUBLE_BASS_STRINGS][WG_DOUBLE_BASS_STRINGS];
  double sympathetic_open_scale;
} WG_DOUBLE_BASS_COUPLING_MODEL;

typedef struct {
  double wet_gain;
  double gain_smoothing_seconds;
  double bridge_radiation_cutoff_hz;
  double bridge_radiation_gain;
  double body_decay_base;
  double body_decay_range;
  double mute_decay_scale;
  double body_tone_depth;
  double mute_low_attenuation;
  double mute_high_attenuation;
  double mute_level_attenuation;
  WG_DOUBLE_BASS_BODY_MODE_SPEC modes[WG_DOUBLE_BASS_BODY_MODES];
} WG_DOUBLE_BASS_BODY_MODEL;

typedef struct {
  uint32_t schema_version;
  const char *id;
  const char *display_name;
  const char *evidence_status;
  const char *source_sha256;
  WG_DOUBLE_BASS_STRING_MODEL strings[WG_DOUBLE_BASS_STRINGS];
  WG_DOUBLE_BASS_BOW_MODEL bow;
  WG_DOUBLE_BASS_GESTURE_MODEL gestures;
  WG_DOUBLE_BASS_RELEASE_T60_MODEL release_t60_seconds;
  WG_DOUBLE_BASS_HARMONICS_MODEL harmonics;
  WG_DOUBLE_BASS_EXCITER_MODEL exciters;
  WG_DOUBLE_BASS_COUPLING_MODEL coupling;
  WG_DOUBLE_BASS_BODY_MODEL body;
} WG_DOUBLE_BASS_MODEL;

typedef struct {
  double real;
  double imaginary;
  double radius;
  double cosine;
  double sine;
  double effective_bandwidth;
  double gain;
  double target_gain;
  double left_gain;
  double right_gain;
  double brightness;
  double source_gain[WG_DOUBLE_BASS_STRINGS];
  int32_t active;
} WG_DOUBLE_BASS_BODY_MODE_STATE;

#define WG_DOUBLE_BASS_MODEL_SCHEMA_VERSION 2U

/* BEGIN GENERATED DOUBLE BASS MODEL DATA */
/* Generated from model/double_bass-v1.json. Do not edit by hand. */
/* Evidence status: violin-derived. */
static const WG_DOUBLE_BASS_MODEL wg_double_bass_model = {
    .schema_version = WG_DOUBLE_BASS_MODEL_SCHEMA_VERSION,
    .id = "double_bass_v1",
    .display_name = "Double Bass v1",
    .evidence_status = "violin-derived",
    .source_sha256 =
        "e92b2293a57b95ef7e242c308076523ae4009a444a708b517e387b4a1b23d6ae",
    .strings = {
        {0.55, 0.35, 20000.0, 5386.995271806526, 0.25, {0.0, 0.0, 0.35, 5386.995271806526}},
        {0.42, 0.45, 4200.0, 8000.0, 0.25, {0.0, 0.0, 0.45, 8000.0}},
        {0.3, 1.1, 12000.0, 2000.0, 0.25, {0.0, 0.0, 1.1, 2000.0}},
        {0.22, 0.35, 12000.0, 3500.0, 0.25, {0.6, 8.0, 0.7, 6000.0}},
    },
    .bow = {
        .speed_scale = 0.65,
        .friction = {0.35, 0.45, 0.1, 0.4, 0.01, 1.2},
        .force_map = {0.2, 60.0, 0.1, 0.85, 0.75, 0.15, 0.85, 1.5},
        .contact = {6000.0, 0.18, 0.7, 0.03, 0.012, 0.01, 0.18, 0.08, 0.16, 2500.0, 2.0, 0.15},
    },
    .gestures = {
        .transition_seconds = 0.012,
        .bow_change_seconds = 0.012,
        .bow_change_contact_dip = 0.48,
        .bow_change_force_floor = 0.72,
        .detache = {0.007, 0.045, 0.62, 0.5, -0.18, 0.96, 0.08, -0.04, 2.0},
        .martele = {0.004, 0.008, 0.04, 0.35, 1.05, -0.35, -0.23, 0.03, 1.12, -0.37, 1.7},
        .spiccato = {0.032, 0.05, 0.018, 0.25, 1.05, 0.18, 1.02},
        .tremolo = {0.006, 8.0, 14.0, 0.82, 0.22, 0.68, 0.32},
    },
    .release_t60_seconds = {0.42, 0.38, 0.13, 0.09, 0.18, 0.72, 0.32, 0.38, 0.16, 0.22},
    .harmonics = {
        .touch_level = {0.97, 0.97, 0.97, 0.97, 0.97, 0.97, 0.97},
        .attack_seconds = 0.012,
        .release_seconds = 0.025,
        .order_transition_seconds = 0.012,
    },
    .exciters = {
        .noise_highpass = 0.89,
        .contact_attack_seconds = 0.0008,
        .contact_release_seconds = 0.003,
        .speed_smooth_seconds = 0.001,
        .pizzicato_right = {0.00018, 0.00072, 0.36, 0.0025},
        .pizzicato_left = {0.0001, 0.00034, 0.27, 0.018},
        .bartok = {0.0001, 0.0003, 0.00045, 0.00085, 0.0001, 0.00024, 0.48, 0.01, 0.62, 0.62},
        .battuto = {0.003, 0.1, 0.16, 0.72, 350000.0, 1250000.0, 3.0, 7.0, 0.0025},
        .tratto = {0.55, 0.04, 0.12, 0.12, 0.006},
    },
    .coupling = {
        .bridge_coefficients = {
            {0.0, 0.00035, 0.00035, 0.00035},
            {0.00035, 0.0, 0.00035, 0.00035},
            {0.00035, 0.00035, 0.0, 0.00035},
            {0.00035, 0.00035, 0.00035, 0.0},
        },
        .sympathetic_open_scale = 6.0,
    },
    .body = {
        .wet_gain = 0.45,
        .gain_smoothing_seconds = 0.015,
        .bridge_radiation_cutoff_hz = 4000.0,
        .bridge_radiation_gain = 0.02,
        .body_decay_base = 1.0,
        .body_decay_range = 0.1,
        .mute_decay_scale = 2.5,
        .body_tone_depth = 0.28,
        .mute_low_attenuation = 0.15,
        .mute_high_attenuation = 0.65,
        .mute_level_attenuation = 0.55,
        .modes = {
            {66.25, 3.75, 0.631, 0.0},
            {98.75, 6.25, 0.2818, -0.1},
            {107.5, 5.0, 0.3981, 0.08},
            {137.5, 7.5, 0.7079, -0.16},
            {166.25, 7.5, 1.0, 0.16},
            {205.0, 13.75, 0.52, -0.22},
            {260.0, 20.0, 0.4, 0.22},
            {330.0, 30.0, 0.34, -0.26},
            {427.5, 45.0, 0.31, 0.26},
            {587.5, 105.0, 0.5, -0.3},
            {825.0, 162.5, 0.36, 0.3},
            {1175.0, 275.0, 0.18, -0.24},
        },
    },
};
/* END GENERATED DOUBLE BASS MODEL DATA */

#if defined(USE_DOUBLE)
#define WG_DOUBLE_BASS_MAX_HANDLE (INT32_MAX - 1)
#else
/* Reserve 2^24 so a rounded larger input cannot alias a valid handle. */
#define WG_DOUBLE_BASS_MAX_HANDLE 16777215
#endif

#if defined(__wasi__)
#define WG_DOUBLE_BASS_REQUIRE_MUTEXES 0
#else
#define WG_DOUBLE_BASS_REQUIRE_MUTEXES 1
#endif

typedef struct {
  const WG_DOUBLE_BASS_MODEL *model;
  uint32_t string_index;
  void *lock;
  OPDS *controller_owner;
  OPDS *controller_tail;
  uint64_t owner_serial;
  double phase;
  double vibrato_phase;
  double frequency;
  double last_controller_frequency;
  uint64_t last_controller_end_sample;
  double trigger;
  double force;
  double speed;
  double position;
  double vibrato_depth_cents;
  double vibrato_rate_hz;
  double strange;
  uint32_t articulation;
  uint32_t harmonic;
  uint32_t harmonic_filter_order;
  uint32_t harmonic_pending_order;
  uint64_t update_epoch;
  uint32_t updated_until;
  int32_t update_epoch_valid;
  double activity;
  uint64_t last_update_sample;
  uint64_t gap_samples;
  int32_t last_update_valid;
  int32_t last_controller_automatic;
  uint32_t last_controller_articulation;
  int32_t last_controller_continuous;
  int32_t last_controller_valid;
  double *toward_bridge;
  double *toward_nut;
  double *second_toward_bridge;
  double *second_toward_nut;
  double *harmonic_history;
  double *bridge_send[2];
  double *strange_coupling_send[2];
  const double *friction_lut;
  uint64_t bridge_send_epoch[2];
  int32_t bridge_send_valid[2];
  uint64_t strange_coupling_send_epoch[2];
  int32_t strange_coupling_send_valid[2];
  uint64_t bridge_send_samples;
  double bridge_send_energy;
  double bridge_send_peak;
  uint32_t rail_size;
  uint32_t bridge_write_index;
  uint32_t nut_write_index;
  uint32_t harmonic_write_index;
  double delay;
  double target_delay;
  double bridge_delay;
  double nut_delay;
  double target_bridge_delay;
  double target_nut_delay;
  double target_frequency;
  double effective_frequency;
  double sounding_frequency;
  double harmonic_fundamental_frequency;
  double harmonic_request;
  double harmonic_stop_position;
  double harmonic_touch_position;
  double harmonic_touch_target;
  double harmonic_touch_pressure;
  double harmonic_touch_attack_smoothing;
  double harmonic_touch_release_smoothing;
  double harmonic_order_mix;
  double harmonic_order_mix_step;
  double harmonic_touch_period;
  double harmonic_projection_input;
  double harmonic_projection_output;
  double delay_tuning_frequency;
  double delay_position;
  double delay_velocity;
  double delay_velocity_smoothing;
  double nut_pole;
  double nut_gain;
  double nut_gain_target;
  double nut_state;
  double bridge_pole;
  double bridge_gain;
  double bridge_gain_target;
  double bridge_state;
  double second_delay;
  double second_bridge_delay;
  double second_nut_delay;
  double second_nut_pole;
  double second_nut_gain;
  double second_nut_gain_target;
  double second_nut_state;
  double second_bridge_pole;
  double second_bridge_gain;
  double second_bridge_gain_target;
  double second_bridge_state;
  double string_loss_smoothing;
  double release_loop_gain;
  double release_loop_gain_target;
  double release_gain_smoothing;
  double release_model_speed;
  double bridge_dynamic_pole;
  double bridge_dynamic_input;
  double bridge_dynamic_output;
  double last_bridge_output;
  double open_frequency;
  double finger_target_position;
  double finger_position;
  double finger_velocity;
  double finger_position_smoothing;
  double finger_pressure_target;
  double finger_pressure;
  double finger_pressure_attack_smoothing;
  double finger_pressure_release_smoothing;
  double finger_noise_envelope;
  double finger_noise_decay;
  double finger_noise_highpass;
  double finger_noise_previous_white;
  double finger_last_noise;
  double finger_noise_energy;
  double finger_noise_peak;
  double finger_last_command_frequency;
  double exciter_position;
  double exciter_amplitude;
  double exciter_noise_level;
  double exciter_impact_amplitude;
  double exciter_contact_target;
  double exciter_contact;
  double exciter_contact_attack_smoothing;
  double exciter_contact_release_smoothing;
  double exciter_speed_target;
  double exciter_speed;
  double exciter_speed_smoothing;
  double exciter_noise_highpass;
  double exciter_noise_previous_white;
  double exciter_last_output;
  double exciter_energy;
  double exciter_peak;
  double collision_compression;
  double collision_stick_speed;
  double collision_stiffness;
  double collision_damping;
  double collision_mass;
  double collision_force;
  double characteristic_impedance;
  double bow_speed_smoothing;
  double bow_contact_attack_smoothing;
  double bow_contact_release_smoothing;
  double bow_position_smoothing;
  double bow_scratch_rise;
  double bow_scratch_fall;
  double bow_speed_target;
  double bow_speed;
  double bow_contact_target;
  double bow_contact;
  double bow_position_target;
  double bow_position;
  double bow_requested_force;
  double bow_effective_force;
  double bow_min_force;
  double bow_max_force;
  double bow_free_velocity;
  double bow_string_velocity;
  double bow_relative_velocity;
  double bow_friction_force;
  double bow_junction_increment;
  double bow_previous_slip_speed;
  double bow_solver_residual;
  double bow_solver_bracket_width;
  double bow_scratch_score;
  double bow_max_abs_relative;
  double contact_memory;
  double contact_memory_velocity;
  double contact_adhesion;
  double contact_steady_displacement;
  double contact_force;
  double contact_temperature;
  double contact_thermal_scale;
  double contact_thermal_work;
  double contact_thermal_attack;
  double contact_thermal_release;
  double contact_memory_release;
  double hair_displacement;
  double hair_velocity;
  double hair_effective_speed;
  double board_displacement;
  double board_compression;
  double board_force;
  double board_energy;
  double board_previous_compression;
  double bow_noise_highpass;
  double bow_noise_previous_white;
  double bow_noise_last;
  double bow_noise_energy;
  double bow_noise_peak;
  double mechanical_envelope;
  double mechanical_decay;
  double board_active_decay;
  double board_inactive_decay;
#if defined(HLOLLI_WG_DOUBLE_BASS_TEST_API)
  double diagnostic_finger_gain;
  double diagnostic_board_gain;
  double diagnostic_rosin_gain;
  double diagnostic_mechanical_gain;
  double diagnostic_nut_cutoff_scale;
  double diagnostic_bridge_cutoff_scale;
#endif
  double coupling_input;
  double coupling_energy;
  double coupling_peak;
  double strange_air_state;
  double strange_air_previous_white;
  double strange_dispersion_input;
  double strange_dispersion_output;
  double strange_subharmonic_phase;
  double strange_subharmonic_envelope;
  double strange_squeal_phase;
  double strange_squeal_state;
  double strange_coupling_mix;
  double strange_last_output;
  double strange_energy;
  double strange_peak;
  double passive_open_level;
  double passive_open_energy;
  double passive_open_peak;
  double gesture_phase;
  double gesture_force_target;
  double gesture_speed_target;
  double gesture_position_target;
  double gesture_force_output;
  double gesture_speed_output;
  double gesture_contact_output;
  double gesture_position_output;
  double gesture_transition_force;
  double gesture_transition_speed;
  double gesture_transition_contact;
  double gesture_transition_position;
  double gesture_transition_mix;
  double gesture_transition_step;
  double gesture_bow_change_start_speed;
  double gesture_bow_change_target_speed;
  double gesture_bow_change_mix;
  double gesture_bow_change_step;
  double gesture_tremolo_phase;
  double gesture_tremolo_rate;
  uint32_t bow_state;
  uint32_t bow_root_count;
  uint32_t bow_solver_iterations;
  uint32_t bow_solver_iterations_max;
  uint32_t bow_failure_streak;
  uint32_t bow_clip_streak;
  uint32_t bow_recovery_remaining;
  uint32_t contact_solver_iterations;
  uint32_t bow_noise_rng;
  uint32_t strange_rng;
  int32_t mechanical_last_bow_direction;
  int32_t mechanical_contact_positive;
  uint32_t strange_last_bow_state;
  uint32_t coupling_source_mask;
  uint32_t finger_rng;
  uint32_t exciter_rng;
  uint32_t finger_motion_kind;
  uint32_t exciter_mode;
  uint32_t exciter_gate_mode;
  uint32_t exciter_age;
  uint32_t exciter_total_samples;
  uint32_t exciter_pulse_samples;
  uint32_t exciter_notch_delay;
  uint32_t exciter_impact_delay;
  uint32_t exciter_impact_samples;
  uint32_t collision_sample_limit;
  uint32_t release_model_articulation;
  uint32_t gesture_articulation;
  uint32_t gesture_age;
  uint32_t gesture_stroke_age;
  uint32_t gesture_onset_samples;
  uint32_t gesture_stroke_samples;
  int32_t bow_direction;
  int32_t trigger_armed;
  int32_t release_gate_positive;
  int32_t exciter_armed;
  int32_t exciter_impact_counted;
  int32_t delay_initialized;
  int32_t finger_initialized;
  int32_t harmonic_valid;
  int32_t harmonic_natural;
  int32_t harmonic_rejected_state;
  int32_t harmonic_initialized;
  int32_t gesture_active;
  int32_t gesture_initialized;
  int32_t gesture_bow_change_active;
  int32_t gesture_command_direction;
  int32_t passive_open_active;
  uint64_t wave_samples;
  uint64_t wave_resets;
  uint64_t excitation_count;
  uint64_t wave_clears;
  uint64_t wave_clips;
  uint64_t bow_solver_calls;
  uint64_t bow_solver_failures;
  uint64_t bow_solver_fallbacks;
  uint64_t bow_stick_samples;
  uint64_t bow_slip_samples;
  uint64_t bow_scratch_samples;
  uint64_t bow_no_motion_samples;
  uint64_t bow_state_transitions;
  uint64_t bow_direction_changes;
  uint64_t bow_recoveries;
  uint64_t contact_solver_failures;
  uint64_t board_impacts;
  uint64_t bow_noise_samples;
  uint64_t mechanical_events;
  uint64_t coupling_samples;
  uint64_t bow_mechanics_recoveries;
  uint64_t passive_open_samples;
  uint64_t strange_samples;
  uint64_t strange_recoveries;
  uint64_t gesture_strokes;
  uint64_t gesture_preset_changes;
  uint64_t gesture_direct_overrides;
  uint64_t gesture_recoveries;
  uint64_t finger_arrivals;
  uint64_t finger_releases;
  uint64_t finger_shifts;
  uint64_t finger_movement_samples;
  uint64_t finger_noise_samples;
  uint64_t normal_pizzicato_attacks;
  uint64_t left_hand_pizzicato_attacks;
  uint64_t bartok_pizzicato_attacks;
  uint64_t fingerboard_impacts;
  uint64_t wood_strikes;
  uint64_t wood_tratto_attacks;
  uint64_t wood_tratto_samples;
  uint64_t collision_samples;
  uint64_t exciter_samples;
  uint64_t exciter_recoveries;
  uint64_t harmonic_transitions;
  uint64_t harmonic_rejections;
  uint64_t harmonic_touch_samples;
} WG_DOUBLE_BASS_STRING_STATE;

typedef struct WG_DOUBLE_BASS_STATE_ {
  int32_t handle;
  const WG_DOUBLE_BASS_MODEL *model;
  uint32_t ksmps;
  double sample_rate;
  double reference_pitch_hz;
  double normal_max_frequency;
  double sounded_max_frequency;
  void *state_lock;
  void *resonance_lock;
  OPDS *renderer_owner;
  OPDS *renderer_tail;
  uint64_t render_epoch;
  uint32_t rendered_until;
  int32_t render_epoch_valid;
  double resonance_activity;
  uint64_t last_render_sample;
  uint64_t renderer_gap_samples;
  int32_t last_render_valid;
  uint64_t next_voice_serial;
  double *rail_memory;
  double *bridge_memory;
  double *body_input;
  double *sympathetic_control[2];
  uint64_t sympathetic_control_epoch[2];
  int32_t sympathetic_control_valid[2];
  uint32_t rail_size;
  WG_DOUBLE_BASS_STRING_STATE strings[WG_DOUBLE_BASS_STRINGS];
  double bridge_coupling_gain[WG_DOUBLE_BASS_STRINGS][WG_DOUBLE_BASS_STRINGS];
  WG_DOUBLE_BASS_BODY_MODE_STATE body_modes[WG_DOUBLE_BASS_BODY_MODES];
  double body;
  double sympathetic;
  double mute;
  double body_gain_smoothing;
  double body_wet_level;
  double body_wet_level_target;
  double body_last_drive;
  double body_drive_energy;
  double bridge_radiation_lowpass;
  double body_last_left;
  double body_last_right;
  double body_output_energy_left;
  double body_output_energy_right;
  double body_peak_left;
  double body_peak_right;
  uint64_t body_samples;
  uint64_t body_fast_forward_samples;
  uint64_t body_resets;
  uint64_t body_clips;
  uint32_t body_limit_streak;
  uint32_t body_recovery_remaining;
  struct WG_DOUBLE_BASS_STATE_ *next;
} WG_DOUBLE_BASS_STATE;

typedef struct {
  WG_DOUBLE_BASS_STATE *first;
  int32_t next_handle;
  void *lock;
  double friction_lut[WG_DOUBLE_BASS_FRICTION_LUT_SIZE];
} WG_DOUBLE_BASS_MANAGER;

typedef struct {
  OPDS h;
  MYFLT *handle;
} WG_DOUBLE_BASS_CREATE;

typedef struct {
  OPDS h;
  MYFLT *handle;
  MYFLT *reference_pitch_hz;
} WG_DOUBLE_BASS_CREATE_TUNED;

typedef struct {
  uint64_t start_sample;
  uint64_t end_sample;
  int32_t valid;
} WG_DOUBLE_BASS_INSTANCE_SPAN;

typedef struct {
  OPDS h;
  MYFLT *out_left;
  MYFLT *out_right;
  MYFLT *ktrigger;
  MYFLT *kfrequency;
  MYFLT *kforce;
  MYFLT *kspeed;
  MYFLT *kposition;
  MYFLT *kvibrato_depth;
  MYFLT *kvibrato_rate;
  MYFLT *karticulation;
  MYFLT *kharmonic;
  MYFLT *kstrange;
  MYFLT *istring;
  MYFLT *idouble_bass;
  WG_DOUBLE_BASS_STATE *double_bass;
  WG_DOUBLE_BASS_INSTANCE_SPAN span;
  uint64_t voice_serial;
  uint32_t string_index;
  OPDS *handoff_next;
  uint64_t predecessor_end_sample;
  uint32_t predecessor_articulation;
  int32_t automatic_string;
  int32_t predecessor_continuous;
  int32_t predecessor_valid;
  int32_t continuity_checked;
  int32_t handed_off;
  int32_t owns_string;
} WG_DOUBLE_BASS_VOICE;

typedef struct {
  OPDS h;
  MYFLT *out_left;
  MYFLT *out_right;
  MYFLT *idouble_bass;
  MYFLT *kbody;
  MYFLT *ksympathetic;
  MYFLT *kmute;
  WG_DOUBLE_BASS_STATE *double_bass;
  WG_DOUBLE_BASS_INSTANCE_SPAN span;
  OPDS *handoff_next;
  int32_t handed_off;
  int32_t owns_renderer;
} WG_DOUBLE_BASS_RESONANCE;

#if defined(HLOLLI_WG_DOUBLE_BASS_TEST_API)
typedef struct {
  OPDS h;
  MYFLT *result;
  MYFLT *idouble_bass;
  MYFLT *istring;
  MYFLT *ifinger;
  MYFLT *iboard;
  MYFLT *irosin;
  MYFLT *imechanical;
  MYFLT *inut_cutoff_scale;
  MYFLT *ibridge_cutoff_scale;
} WG_DOUBLE_BASS_TEST_DIAGNOSTIC_GAINS;

typedef struct {
  OPDS h;
  MYFLT *phase;
  MYFLT *vibrato_phase;
  MYFLT *frequency;
  MYFLT *trigger;
  MYFLT *force;
  MYFLT *speed;
  MYFLT *activity;
  MYFLT *last_update_sample;
  MYFLT *gap_samples;
  MYFLT *owner;
  MYFLT *successor;
  MYFLT *idouble_bass;
  MYFLT *istring;
} WG_DOUBLE_BASS_TEST_STRING;

typedef struct {
  OPDS h;
  MYFLT *capacity;
  MYFLT *read_delay;
  MYFLT *target_delay;
  MYFLT *target_frequency;
  MYFLT *loop_phase_samples;
  MYFLT *toward_nut_energy;
  MYFLT *toward_bridge_energy;
  MYFLT *toward_nut_signature;
  MYFLT *toward_bridge_signature;
  MYFLT *nut_pole;
  MYFLT *nut_gain;
  MYFLT *nut_state;
  MYFLT *bridge_pole;
  MYFLT *bridge_gain;
  MYFLT *bridge_state;
  MYFLT *write_index;
  MYFLT *wave_samples;
  MYFLT *excitation_count;
  MYFLT *clear_count;
  MYFLT *reset_count;
  MYFLT *clip_count;
  MYFLT *bridge_dynamic_input;
  MYFLT *bridge_dynamic_output;
  MYFLT *last_bridge_output;
  MYFLT *finite;
  MYFLT *idouble_bass;
  MYFLT *istring;
} WG_DOUBLE_BASS_TEST_WAVEGUIDE;

typedef struct {
  OPDS h;
  MYFLT *open_frequency;
  MYFLT *target_position;
  MYFLT *position;
  MYFLT *target_pressure;
  MYFLT *pressure;
  MYFLT *velocity;
  MYFLT *delay_velocity;
  MYFLT *effective_frequency;
  MYFLT *last_noise;
  MYFLT *noise_energy;
  MYFLT *noise_peak;
  MYFLT *noise_state;
  MYFLT *arrivals;
  MYFLT *releases;
  MYFLT *shifts;
  MYFLT *movement_samples;
  MYFLT *noise_samples;
  MYFLT *finite;
  MYFLT *idouble_bass;
  MYFLT *istring;
} WG_DOUBLE_BASS_TEST_FINGER;

typedef struct {
  OPDS h;
  MYFLT *requested_order;
  MYFLT *active_order;
  MYFLT *filter_order;
  MYFLT *valid;
  MYFLT *natural;
  MYFLT *sounding_frequency;
  MYFLT *fundamental_frequency;
  MYFLT *stop_position;
  MYFLT *touch_position;
  MYFLT *touch_target;
  MYFLT *touch_pressure;
  MYFLT *touch_period;
  MYFLT *projection_input;
  MYFLT *projection_output;
  MYFLT *transitions;
  MYFLT *rejections;
  MYFLT *touch_samples;
  MYFLT *history_index;
  MYFLT *finite;
  MYFLT *idouble_bass;
  MYFLT *istring;
} WG_DOUBLE_BASS_TEST_HARMONIC;

typedef struct {
  OPDS h;
  MYFLT *articulation;
  MYFLT *mode;
  MYFLT *armed;
  MYFLT *active;
  MYFLT *age;
  MYFLT *total_samples;
  MYFLT *pulse_samples;
  MYFLT *notch_delay;
  MYFLT *position;
  MYFLT *contact_target;
  MYFLT *contact;
  MYFLT *speed_target;
  MYFLT *speed;
  MYFLT *last_output;
  MYFLT *energy;
  MYFLT *peak;
  MYFLT *rng;
  MYFLT *pending_impact;
  MYFLT *normal_pizzicato_attacks;
  MYFLT *left_hand_pizzicato_attacks;
  MYFLT *bartok_pizzicato_attacks;
  MYFLT *fingerboard_impacts;
  MYFLT *wood_strikes;
  MYFLT *wood_tratto_attacks;
  MYFLT *wood_tratto_samples;
  MYFLT *collision_samples;
  MYFLT *collision_compression;
  MYFLT *collision_force;
  MYFLT *exciter_samples;
  MYFLT *recoveries;
  MYFLT *finite;
  MYFLT *idouble_bass;
  MYFLT *istring;
} WG_DOUBLE_BASS_TEST_EXCITER;

typedef struct {
  OPDS h;
  MYFLT *result;
  MYFLT *idouble_bass;
  MYFLT *istring;
  MYFLT *imode;
} WG_DOUBLE_BASS_TEST_EXCITER_FAULT;

typedef struct {
  OPDS h;
  MYFLT *state;
  MYFLT *requested_force;
  MYFLT *effective_force;
  MYFLT *bow_speed;
  MYFLT *contact;
  MYFLT *target_position;
  MYFLT *effective_position;
  MYFLT *bridge_delay;
  MYFLT *nut_delay;
  MYFLT *impedance;
  MYFLT *free_velocity;
  MYFLT *string_velocity;
  MYFLT *relative_velocity;
  MYFLT *friction_force;
  MYFLT *junction_increment;
  MYFLT *min_force;
  MYFLT *max_force;
  MYFLT *residual;
  MYFLT *bracket_width;
  MYFLT *root_count;
  MYFLT *iterations_last;
  MYFLT *iterations_max;
  MYFLT *solver_calls;
  MYFLT *solver_failures;
  MYFLT *solver_fallbacks;
  MYFLT *stick_samples;
  MYFLT *slip_samples;
  MYFLT *scratch_samples;
  MYFLT *no_motion_samples;
  MYFLT *state_transitions;
  MYFLT *direction_changes;
  MYFLT *attacks;
  MYFLT *recoveries;
  MYFLT *scratch_score;
  MYFLT *max_abs_relative;
  MYFLT *finite;
  MYFLT *idouble_bass;
  MYFLT *istring;
} WG_DOUBLE_BASS_TEST_BOW;

typedef struct {
  OPDS h;
  MYFLT *result;
  MYFLT *idouble_bass;
  MYFLT *istring;
  MYFLT *imode;
} WG_DOUBLE_BASS_TEST_BOW_FAULT;

typedef struct {
  OPDS h;
  MYFLT *articulation;
  MYFLT *active;
  MYFLT *phase;
  MYFLT *force_target;
  MYFLT *speed_target;
  MYFLT *force_output;
  MYFLT *speed_output;
  MYFLT *contact_output;
  MYFLT *onset_samples;
  MYFLT *stroke_samples;
  MYFLT *strokes;
  MYFLT *bow_changes;
  MYFLT *preset_changes;
  MYFLT *direct_overrides;
  MYFLT *recoveries;
  MYFLT *release_articulation;
  MYFLT *release_gain_target;
  MYFLT *finite;
  MYFLT *idouble_bass;
  MYFLT *istring;
} WG_DOUBLE_BASS_TEST_GESTURE;

typedef struct {
  OPDS h;
  MYFLT *result;
  MYFLT *idouble_bass;
  MYFLT *istring;
  MYFLT *imode;
} WG_DOUBLE_BASS_TEST_GESTURE_FAULT;

typedef struct {
  OPDS h;
  MYFLT *contact_memory;
  MYFLT *contact_memory_velocity;
  MYFLT *contact_adhesion;
  MYFLT *contact_steady_displacement;
  MYFLT *contact_force;
  MYFLT *contact_iterations;
  MYFLT *contact_failures;
  MYFLT *temperature;
  MYFLT *thermal_scale;
  MYFLT *thermal_work;
  MYFLT *hair_displacement;
  MYFLT *hair_velocity;
  MYFLT *hair_effective_speed;
  MYFLT *board_displacement;
  MYFLT *board_compression;
  MYFLT *board_force;
  MYFLT *board_energy;
  MYFLT *board_impacts;
  MYFLT *noise_last;
  MYFLT *noise_energy;
  MYFLT *noise_peak;
  MYFLT *noise_samples;
  MYFLT *mechanical_events;
  MYFLT *coupling_input;
  MYFLT *coupling_energy;
  MYFLT *coupling_peak;
  MYFLT *coupling_samples;
  MYFLT *coupling_source_mask;
  MYFLT *recoveries;
  MYFLT *second_polarization;
  MYFLT *finite;
  MYFLT *idouble_bass;
  MYFLT *istring;
} WG_DOUBLE_BASS_TEST_PHYSICS;

typedef struct {
  OPDS h;
  MYFLT *result;
  MYFLT *idouble_bass;
  MYFLT *istring;
  MYFLT *imode;
} WG_DOUBLE_BASS_TEST_PHYSICS_FAULT;

typedef struct {
  OPDS h;
  MYFLT *strange;
  MYFLT *sympathetic;
  MYFLT *sympathetic_applied;
  MYFLT *air_state;
  MYFLT *dispersion_input;
  MYFLT *dispersion_output;
  MYFLT *subharmonic_phase;
  MYFLT *subharmonic_envelope;
  MYFLT *squeal_state;
  MYFLT *squeal_frequency;
  MYFLT *coupling_mix;
  MYFLT *last_output;
  MYFLT *energy;
  MYFLT *peak;
  MYFLT *samples;
  MYFLT *recoveries;
  MYFLT *passive_active;
  MYFLT *passive_level;
  MYFLT *passive_energy;
  MYFLT *passive_peak;
  MYFLT *passive_samples;
  MYFLT *finite;
  MYFLT *idouble_bass;
  MYFLT *istring;
} WG_DOUBLE_BASS_TEST_STRANGE;

typedef struct {
  OPDS h;
  MYFLT *result;
  MYFLT *idouble_bass;
  MYFLT *istring;
  MYFLT *imode;
} WG_DOUBLE_BASS_TEST_STRANGE_FAULT;

typedef struct {
  OPDS h;
  MYFLT *result;
  MYFLT *idouble_bass;
  MYFLT *istring;
  MYFLT *ifrequency;
  MYFLT *iamplitude;
} WG_DOUBLE_BASS_TEST_STRING_IMPULSE;

typedef struct {
  OPDS h;
  MYFLT *body;
  MYFLT *sympathetic;
  MYFLT *mute;
  MYFLT *activity;
  MYFLT *last_update_sample;
  MYFLT *gap_samples;
  MYFLT *owner;
  MYFLT *successor;
  MYFLT *idouble_bass;
} WG_DOUBLE_BASS_TEST_RENDERER;

typedef struct {
  OPDS h;
  MYFLT *body;
  MYFLT *sympathetic;
  MYFLT *mute;
  MYFLT *mode_count;
  MYFLT *send_samples_1;
  MYFLT *send_samples_2;
  MYFLT *send_samples_3;
  MYFLT *send_samples_4;
  MYFLT *send_energy_1;
  MYFLT *send_energy_2;
  MYFLT *send_energy_3;
  MYFLT *send_energy_4;
  MYFLT *last_drive;
  MYFLT *drive_energy;
  MYFLT *modal_energy;
  MYFLT *state_signature;
  MYFLT *last_left;
  MYFLT *last_right;
  MYFLT *output_energy_left;
  MYFLT *output_energy_right;
  MYFLT *peak_left;
  MYFLT *peak_right;
  MYFLT *processed_samples;
  MYFLT *fast_forward_samples;
  MYFLT *reset_count;
  MYFLT *clip_count;
  MYFLT *finite;
  MYFLT *idouble_bass;
} WG_DOUBLE_BASS_TEST_BODY;

typedef struct {
  OPDS h;
  MYFLT *frequency;
  MYFLT *base_bandwidth;
  MYFLT *effective_bandwidth;
  MYFLT *radius;
  MYFLT *gain;
  MYFLT *pan;
  MYFLT *real;
  MYFLT *imaginary;
  MYFLT *finite;
  MYFLT *idouble_bass;
  MYFLT *imode;
} WG_DOUBLE_BASS_TEST_BODY_MODE;

typedef struct {
  OPDS h;
  MYFLT *result;
  MYFLT *idouble_bass;
  MYFLT *imode;
} WG_DOUBLE_BASS_TEST_BODY_FAULT;

typedef struct {
  OPDS h;
  MYFLT *result;
  MYFLT *idouble_bass;
  MYFLT *istring;
  MYFLT *iamplitude;
} WG_DOUBLE_BASS_TEST_BRIDGE_IMPULSE;
#endif

static double wg_double_bass_clamp(double value, double low, double high)
{
  if (!isfinite(value)) {
    return low;
  }
  if (value < low) {
    return low;
  }
  if (value > high) {
    return high;
  }
  return value;
}

static double wg_double_bass_input(const MYFLT *value, double fallback)
{
  const double result = value != NULL ? (double)*value : fallback;
  return isfinite(result) ? result : fallback;
}

static double wg_double_bass_scaled_pitch(
    double a440_frequency, double reference_pitch_hz)
{
  return reference_pitch_hz * (
      a440_frequency / WG_DOUBLE_BASS_DEFAULT_REFERENCE_PITCH_HZ);
}

static const WG_DOUBLE_BASS_MODEL *wg_double_bass_fixed_model(void)
{
  return &wg_double_bass_model;
}

static int32_t wg_double_bass_model_number_is_valid(
    double value, double low, double high)
{
  return isfinite(value) && value >= low && value <= high;
}

static int32_t wg_double_bass_model_id_is_valid(const char *id)
{
  size_t index;
  size_t length;

  if (id == NULL || id[0] < 'a' || id[0] > 'z') {
    return 0;
  }
  length = strlen(id);
  if (length == 0U || length > 63U) {
    return 0;
  }
  for (index = 1U; index < length; index++) {
    const char character = id[index];
    if (!((character >= 'a' && character <= 'z') ||
          (character >= '0' && character <= '9') || character == '_')) {
      return 0;
    }
  }
  return 1;
}

static int32_t wg_double_bass_model_hash_is_valid(const char *hash)
{
  size_t index;

  if (hash == NULL || strlen(hash) != 64U) {
    return 0;
  }
  for (index = 0U; index < 64U; index++) {
    if (!((hash[index] >= '0' && hash[index] <= '9') ||
          (hash[index] >= 'a' && hash[index] <= 'f'))) {
      return 0;
    }
  }
  return 1;
}

static int32_t wg_double_bass_string_model_is_valid(
    const WG_DOUBLE_BASS_STRING_MODEL *string)
{
  const WG_DOUBLE_BASS_POLARIZATION_MODEL *second;

  if (string == NULL) {
    return 0;
  }
  second = &string->second_polarization;
  return wg_double_bass_model_number_is_valid(
          string->characteristic_impedance, 0.01, 10.0) &&
      wg_double_bass_model_number_is_valid(
          string->loss_time_constant_seconds, 0.01, 30.0) &&
      wg_double_bass_model_number_is_valid(
          string->nut_cutoff_hz, 100.0, 100000.0) &&
      wg_double_bass_model_number_is_valid(
          string->bridge_cutoff_hz, 100.0, 100000.0) &&
      wg_double_bass_model_number_is_valid(
          string->nut_loss_fraction, 0.0, 1.0) &&
      wg_double_bass_model_number_is_valid(second->mix, 0.0, 0.95) &&
      wg_double_bass_model_number_is_valid(
          second->detune_cents, -100.0, 100.0) &&
      wg_double_bass_model_number_is_valid(
          second->loss_time_constant_seconds, 0.01, 30.0) &&
      wg_double_bass_model_number_is_valid(
          second->bridge_cutoff_hz, 100.0, 100000.0);
}

static int32_t wg_double_bass_bow_model_is_valid(
    const WG_DOUBLE_BASS_BOW_MODEL *bow)
{
  const WG_DOUBLE_BASS_FRICTION_MODEL *friction;
  const WG_DOUBLE_BASS_BOW_FORCE_MAP_MODEL *force_map;
  const WG_DOUBLE_BASS_BOW_CONTACT_MODEL *contact;
  double component_static_mu;

  if (bow == NULL ||
      !wg_double_bass_model_number_is_valid(bow->speed_scale, 0.0, 2.0)) {
    return 0;
  }
  friction = &bow->friction;
  component_static_mu =
      friction->floor_mu + friction->slow_gain + friction->fast_gain;
  if (!wg_double_bass_model_number_is_valid(friction->floor_mu, 0.001, 4.0) ||
      !wg_double_bass_model_number_is_valid(friction->slow_gain, 0.0, 4.0) ||
      !wg_double_bass_model_number_is_valid(friction->slow_speed, 1.0e-5, 4.0) ||
      !wg_double_bass_model_number_is_valid(friction->fast_gain, 0.0, 4.0) ||
      !wg_double_bass_model_number_is_valid(friction->fast_speed, 1.0e-5, 4.0) ||
      !wg_double_bass_model_number_is_valid(friction->static_mu, 0.01, 2.5) ||
      fabs(friction->static_mu - component_static_mu) > 1.0e-12) {
    return 0;
  }
  force_map = &bow->force_map;
  if (!wg_double_bass_model_number_is_valid(
          force_map->mu_drop_floor, 0.001, 4.0) ||
      !wg_double_bass_model_number_is_valid(
          force_map->minimum_divisor, 1.0, 10000.0) ||
      !wg_double_bass_model_number_is_valid(force_map->control_low, 0.001, 0.99) ||
      !wg_double_bass_model_number_is_valid(force_map->control_high, 0.01, 0.999) ||
      force_map->control_low >= force_map->control_high ||
      !wg_double_bass_model_number_is_valid(
          force_map->control_middle_span, 1.0e-6, 1.0) ||
      !wg_double_bass_model_number_is_valid(
          force_map->control_high_span, 1.0e-6, 1.0) ||
      fabs(force_map->control_low + force_map->control_middle_span -
               force_map->control_high) > 1.0e-12 ||
      fabs(force_map->control_high + force_map->control_high_span - 1.0) >
          1.0e-12 ||
      !wg_double_bass_model_number_is_valid(
          force_map->normal_max_scale, 0.01, 2.0) ||
      !wg_double_bass_model_number_is_valid(
          force_map->extreme_max_scale, force_map->normal_max_scale, 8.0)) {
    return 0;
  }
  contact = &bow->contact;
  return wg_double_bass_model_number_is_valid(contact->stiffness, 0.01, 1.0e7) &&
      wg_double_bass_model_number_is_valid(contact->damping, 0.001, 1.0e4) &&
      wg_double_bass_model_number_is_valid(contact->breakaway, 0.0, 1.0) &&
      wg_double_bass_model_number_is_valid(contact->dynamic_mix, 0.0, 1.0) &&
      wg_double_bass_model_number_is_valid(
          contact->memory_release_seconds, 1.0e-5, 10.0) &&
      wg_double_bass_model_number_is_valid(
          contact->thermal_attack_seconds, 1.0e-5, 10.0) &&
      wg_double_bass_model_number_is_valid(
          contact->thermal_release_seconds, 1.0e-5, 10.0) &&
      wg_double_bass_model_number_is_valid(contact->thermal_drop, 0.0, 0.95) &&
      wg_double_bass_model_number_is_valid(
          contact->thermal_work_scale, 1.0e-6, 100.0) &&
      wg_double_bass_model_number_is_valid(
          contact->hair_stiffness, 0.01, 1.0e7) &&
      wg_double_bass_model_number_is_valid(contact->hair_damping, 0.001, 1.0e4) &&
      wg_double_bass_model_number_is_valid(contact->hair_contact_mix, 0.0, 1.0);
}

static int32_t wg_double_bass_gesture_model_is_valid(
    const WG_DOUBLE_BASS_GESTURE_MODEL *gesture)
{
  const WG_DOUBLE_BASS_DETACHE_MODEL *detache;
  const WG_DOUBLE_BASS_MARTELE_MODEL *martele;
  const WG_DOUBLE_BASS_SPICCATO_MODEL *spiccato;
  const WG_DOUBLE_BASS_TREMOLO_MODEL *tremolo;

  if (gesture == NULL ||
      !wg_double_bass_model_number_is_valid(
          gesture->transition_seconds, 1.0e-5, 2.0) ||
      !wg_double_bass_model_number_is_valid(
          gesture->bow_change_seconds, 1.0e-5, 2.0) ||
      !wg_double_bass_model_number_is_valid(
          gesture->bow_change_contact_dip, 0.0, 1.0) ||
      !wg_double_bass_model_number_is_valid(
          gesture->bow_change_force_floor, 0.0, 1.0)) {
    return 0;
  }
  detache = &gesture->detache;
  if (!wg_double_bass_model_number_is_valid(detache->onset_seconds, 1.0e-5, 2.0) ||
      !wg_double_bass_model_number_is_valid(
          detache->stroke_seconds, detache->onset_seconds, 2.0) ||
      !wg_double_bass_model_number_is_valid(detache->force_base, -8.0, 8.0) ||
      !wg_double_bass_model_number_is_valid(detache->force_attack, -8.0, 8.0) ||
      !wg_double_bass_model_number_is_valid(detache->force_settle, -8.0, 8.0) ||
      !wg_double_bass_model_number_is_valid(detache->speed_base, -8.0, 8.0) ||
      !wg_double_bass_model_number_is_valid(detache->speed_attack, -8.0, 8.0) ||
      !wg_double_bass_model_number_is_valid(detache->speed_settle, -8.0, 8.0) ||
      !wg_double_bass_model_number_is_valid(
          detache->contact_attack_scale, 0.0, 8.0)) {
    return 0;
  }
  martele = &gesture->martele;
  if (!wg_double_bass_model_number_is_valid(
          martele->preload_seconds, 1.0e-5, 2.0) ||
      !wg_double_bass_model_number_is_valid(
          martele->acceleration_seconds, 1.0e-5, 2.0) ||
      !wg_double_bass_model_number_is_valid(
          martele->stroke_seconds,
          martele->preload_seconds + martele->acceleration_seconds, 2.0) ||
      !wg_double_bass_model_number_is_valid(martele->force_base, -8.0, 8.0) ||
      !wg_double_bass_model_number_is_valid(martele->force_preload, -8.0, 8.0) ||
      !wg_double_bass_model_number_is_valid(
          martele->force_acceleration, -8.0, 8.0) ||
      !wg_double_bass_model_number_is_valid(martele->force_settle, -8.0, 8.0) ||
      !wg_double_bass_model_number_is_valid(martele->speed_preload, -8.0, 8.0) ||
      !wg_double_bass_model_number_is_valid(
          martele->speed_acceleration, -8.0, 8.0) ||
      !wg_double_bass_model_number_is_valid(martele->speed_settle, -8.0, 8.0) ||
      !wg_double_bass_model_number_is_valid(
          martele->contact_preload_scale, 0.0, 8.0)) {
    return 0;
  }
  spiccato = &gesture->spiccato;
  if (!wg_double_bass_model_number_is_valid(
          spiccato->stroke_fast_seconds, 1.0e-5, 2.0) ||
      !wg_double_bass_model_number_is_valid(
          spiccato->stroke_slow_seconds,
          spiccato->stroke_fast_seconds, 2.0) ||
      !wg_double_bass_model_number_is_valid(
          spiccato->stroke_range_seconds, 1.0e-5, 2.0) ||
      fabs(spiccato->stroke_fast_seconds + spiccato->stroke_range_seconds -
               spiccato->stroke_slow_seconds) > 1.0e-12 ||
      !wg_double_bass_model_number_is_valid(spiccato->force_base, -8.0, 8.0) ||
      !wg_double_bass_model_number_is_valid(spiccato->force_bounce, -8.0, 8.0) ||
      !wg_double_bass_model_number_is_valid(spiccato->speed_base, -8.0, 8.0) ||
      !wg_double_bass_model_number_is_valid(spiccato->speed_bounce, -8.0, 8.0)) {
    return 0;
  }
  tremolo = &gesture->tremolo;
  return wg_double_bass_model_number_is_valid(
             tremolo->onset_seconds, 1.0e-5, 2.0) &&
      wg_double_bass_model_number_is_valid(tremolo->rate_min_hz, 0.01, 20.0) &&
      wg_double_bass_model_number_is_valid(
          tremolo->rate_max_hz, tremolo->rate_min_hz, 20.0) &&
      wg_double_bass_model_number_is_valid(tremolo->force_base, -8.0, 8.0) &&
      wg_double_bass_model_number_is_valid(tremolo->force_motion, -8.0, 8.0) &&
      wg_double_bass_model_number_is_valid(tremolo->contact_base, -8.0, 8.0) &&
      wg_double_bass_model_number_is_valid(tremolo->contact_motion, -8.0, 8.0);
}

static int32_t wg_double_bass_release_model_is_valid(
    const WG_DOUBLE_BASS_RELEASE_T60_MODEL *release)
{
  return release != NULL &&
      wg_double_bass_model_number_is_valid(release->arco, 0.01, 30.0) &&
      wg_double_bass_model_number_is_valid(release->detache, 0.01, 30.0) &&
      wg_double_bass_model_number_is_valid(release->martele, 0.01, 30.0) &&
      wg_double_bass_model_number_is_valid(release->spiccato, 0.01, 30.0) &&
      wg_double_bass_model_number_is_valid(release->tremolo, 0.01, 30.0) &&
      wg_double_bass_model_number_is_valid(
          release->pizzicato_right, 0.01, 30.0) &&
      wg_double_bass_model_number_is_valid(
          release->pizzicato_left, 0.01, 30.0) &&
      wg_double_bass_model_number_is_valid(release->bartok, 0.01, 30.0) &&
      wg_double_bass_model_number_is_valid(release->battuto, 0.01, 30.0) &&
      wg_double_bass_model_number_is_valid(release->tratto, 0.01, 30.0);
}

static int32_t wg_double_bass_harmonics_model_is_valid(
    const WG_DOUBLE_BASS_HARMONICS_MODEL *harmonics)
{
  uint32_t order_index;

  if (harmonics == NULL ||
      !wg_double_bass_model_number_is_valid(
          harmonics->attack_seconds, 1.0e-5, 2.0) ||
      !wg_double_bass_model_number_is_valid(
          harmonics->release_seconds, 1.0e-5, 2.0) ||
      !wg_double_bass_model_number_is_valid(
          harmonics->order_transition_seconds, 1.0e-5, 2.0)) {
    return 0;
  }
  for (order_index = 0U;
       order_index < WG_DOUBLE_BASS_HARMONIC_MAX_ORDER -
                         WG_DOUBLE_BASS_HARMONIC_MIN_ORDER + 1U;
       order_index++) {
    if (!wg_double_bass_model_number_is_valid(
            harmonics->touch_level[order_index], 0.0, 1.0)) {
      return 0;
    }
  }
  return 1;
}

static int32_t wg_double_bass_pizzicato_model_is_valid(
    const WG_DOUBLE_BASS_PIZZICATO_MODEL *pizzicato)
{
  return pizzicato != NULL &&
      wg_double_bass_model_number_is_valid(
          pizzicato->pulse_min_seconds, 0.0, 0.1) &&
      wg_double_bass_model_number_is_valid(
          pizzicato->pulse_range_seconds, 0.0, 0.1) &&
      pizzicato->pulse_min_seconds + pizzicato->pulse_range_seconds <=
          WG_DOUBLE_BASS_EXCITER_MAX_PULSE_SECONDS &&
      wg_double_bass_model_number_is_valid(pizzicato->amplitude, 0.0, 8.0) &&
      wg_double_bass_model_number_is_valid(pizzicato->noise_gain, 0.0, 8.0);
}

static int32_t wg_double_bass_exciter_model_is_valid(
    const WG_DOUBLE_BASS_EXCITER_MODEL *exciters)
{
  const WG_DOUBLE_BASS_BARTOK_MODEL *bartok;
  const WG_DOUBLE_BASS_BATTUTO_MODEL *battuto;
  const WG_DOUBLE_BASS_TRATTO_MODEL *tratto;

  if (exciters == NULL ||
      !wg_double_bass_model_number_is_valid(exciters->noise_highpass, 0.0, 0.9999) ||
      !wg_double_bass_model_number_is_valid(
          exciters->contact_attack_seconds, 1.0e-6, 1.0) ||
      !wg_double_bass_model_number_is_valid(
          exciters->contact_release_seconds, 1.0e-6, 1.0) ||
      !wg_double_bass_model_number_is_valid(
          exciters->speed_smooth_seconds, 1.0e-6, 1.0) ||
      !wg_double_bass_pizzicato_model_is_valid(&exciters->pizzicato_right) ||
      !wg_double_bass_pizzicato_model_is_valid(&exciters->pizzicato_left)) {
    return 0;
  }
  bartok = &exciters->bartok;
  if (!wg_double_bass_model_number_is_valid(
          bartok->pulse_min_seconds, 0.0, 0.1) ||
      !wg_double_bass_model_number_is_valid(
          bartok->pulse_range_seconds, 0.0, 0.1) ||
      !wg_double_bass_model_number_is_valid(
          bartok->impact_delay_min_seconds, 0.0, 0.1) ||
      !wg_double_bass_model_number_is_valid(
          bartok->impact_delay_range_seconds, 0.0, 0.1) ||
      !wg_double_bass_model_number_is_valid(
          bartok->impact_min_seconds, 0.0, 0.1) ||
      !wg_double_bass_model_number_is_valid(
          bartok->impact_range_seconds, 0.0, 0.1) ||
      bartok->pulse_min_seconds + bartok->pulse_range_seconds >
          WG_DOUBLE_BASS_EXCITER_MAX_PULSE_SECONDS ||
      bartok->impact_delay_min_seconds + bartok->impact_delay_range_seconds >
          WG_DOUBLE_BASS_EXCITER_MAX_PULSE_SECONDS ||
      bartok->impact_min_seconds + bartok->impact_range_seconds >
          WG_DOUBLE_BASS_EXCITER_MAX_PULSE_SECONDS ||
      !wg_double_bass_model_number_is_valid(bartok->amplitude, 0.0, 8.0) ||
      !wg_double_bass_model_number_is_valid(bartok->noise_gain, 0.0, 8.0) ||
      !wg_double_bass_model_number_is_valid(bartok->impact_gain, 0.0, 8.0) ||
      !wg_double_bass_model_number_is_valid(
          bartok->impact_alternating_mix, 0.0, 1.0)) {
    return 0;
  }
  battuto = &exciters->battuto;
  if (!wg_double_bass_model_number_is_valid(
          battuto->collision_seconds, 1.0e-6, 0.1) ||
      !wg_double_bass_model_number_is_valid(battuto->noise_gain, 0.0, 8.0) ||
      !wg_double_bass_model_number_is_valid(battuto->speed_base, 0.0, 8.0) ||
      !wg_double_bass_model_number_is_valid(battuto->speed_force, 0.0, 8.0) ||
      !wg_double_bass_model_number_is_valid(
          battuto->stiffness_base, 0.01, 1.0e9) ||
      !wg_double_bass_model_number_is_valid(
          battuto->stiffness_force, 0.0, 1.0e9) ||
      !wg_double_bass_model_number_is_valid(battuto->damping_base, 0.0, 1.0e6) ||
      !wg_double_bass_model_number_is_valid(
          battuto->damping_force, 0.0, 1.0e6) ||
      !wg_double_bass_model_number_is_valid(battuto->mass_kg, 1.0e-6, 10.0)) {
    return 0;
  }
  tratto = &exciters->tratto;
  return wg_double_bass_model_number_is_valid(tratto->speed_scale, 0.0, 2.0) &&
      wg_double_bass_model_number_is_valid(tratto->transition_min, 1.0e-6, 2.0) &&
      wg_double_bass_model_number_is_valid(tratto->transition_range, 0.0, 2.0) &&
      wg_double_bass_model_number_is_valid(tratto->force_gain, 0.0, 8.0) &&
      wg_double_bass_model_number_is_valid(tratto->grain_gain, 0.0, 8.0);
}

static int32_t wg_double_bass_coupling_model_is_valid(
    const WG_DOUBLE_BASS_COUPLING_MODEL *coupling)
{
  uint32_t target_index;

  if (coupling == NULL ||
      !wg_double_bass_model_number_is_valid(
          coupling->sympathetic_open_scale, 0.0, 100.0)) {
    return 0;
  }
  for (target_index = 0U; target_index < WG_DOUBLE_BASS_STRINGS;
       target_index++) {
    uint32_t source_index;
    for (source_index = 0U; source_index < WG_DOUBLE_BASS_STRINGS;
         source_index++) {
      const double value =
          coupling->bridge_coefficients[target_index][source_index];
      if (!wg_double_bass_model_number_is_valid(value, 0.0, 0.1) ||
          (target_index == source_index && value != 0.0) ||
          value != coupling->bridge_coefficients[source_index][target_index]) {
        return 0;
      }
    }
  }
  return 1;
}

static int32_t wg_double_bass_body_model_is_valid(
    const WG_DOUBLE_BASS_BODY_MODEL *body)
{
  uint32_t mode_index;
  double previous_frequency = 0.0;

  if (body == NULL ||
      !wg_double_bass_model_number_is_valid(body->wet_gain, 0.0, 4.0) ||
      !wg_double_bass_model_number_is_valid(
          body->gain_smoothing_seconds, 1.0e-5, 10.0) ||
      !wg_double_bass_model_number_is_valid(
          body->bridge_radiation_cutoff_hz, 100.0, 100000.0) ||
      !wg_double_bass_model_number_is_valid(
          body->bridge_radiation_gain, 0.0, 1.0) ||
      !wg_double_bass_model_number_is_valid(body->body_decay_base, 0.01, 10.0) ||
      !wg_double_bass_model_number_is_valid(body->body_decay_range, 0.0, 10.0) ||
      body->body_decay_range >= body->body_decay_base ||
      !wg_double_bass_model_number_is_valid(body->mute_decay_scale, 0.0, 20.0) ||
      !wg_double_bass_model_number_is_valid(body->body_tone_depth, 0.0, 4.0) ||
      !wg_double_bass_model_number_is_valid(
          body->mute_low_attenuation, 0.0, 1.0) ||
      !wg_double_bass_model_number_is_valid(
          body->mute_high_attenuation, 0.0, 1.0) ||
      !wg_double_bass_model_number_is_valid(
          body->mute_level_attenuation, 0.0, 1.0)) {
    return 0;
  }
  for (mode_index = 0U; mode_index < WG_DOUBLE_BASS_BODY_MODES; mode_index++) {
    const WG_DOUBLE_BASS_BODY_MODE_SPEC *mode = &body->modes[mode_index];
    if (!wg_double_bass_model_number_is_valid(mode->frequency, 20.0, 100000.0) ||
        mode->frequency <= previous_frequency ||
        !wg_double_bass_model_number_is_valid(mode->bandwidth, 0.01, 100000.0) ||
        mode->bandwidth > 2.0 * mode->frequency ||
        !wg_double_bass_model_number_is_valid(mode->gain, 0.0, 100.0) ||
        !wg_double_bass_model_number_is_valid(mode->pan, -1.0, 1.0)) {
      return 0;
    }
    previous_frequency = mode->frequency;
  }
  return 1;
}

static int32_t wg_double_bass_model_is_valid(
    const WG_DOUBLE_BASS_MODEL *model)
{
  uint32_t string_index;

  if (model == NULL ||
      model->schema_version != WG_DOUBLE_BASS_MODEL_SCHEMA_VERSION ||
      !wg_double_bass_model_id_is_valid(model->id) ||
      strcmp(model->id, "double_bass_v1") != 0 ||
      model->display_name == NULL || model->display_name[0] == '\0' ||
      strlen(model->display_name) > 127U ||
      model->evidence_status == NULL ||
      strcmp(model->evidence_status, "violin-derived") != 0 ||
      !wg_double_bass_model_hash_is_valid(model->source_sha256) ||
      !wg_double_bass_bow_model_is_valid(&model->bow) ||
      !wg_double_bass_gesture_model_is_valid(&model->gestures) ||
      !wg_double_bass_release_model_is_valid(&model->release_t60_seconds) ||
      !wg_double_bass_harmonics_model_is_valid(&model->harmonics) ||
      !wg_double_bass_exciter_model_is_valid(&model->exciters) ||
      !wg_double_bass_coupling_model_is_valid(&model->coupling) ||
      !wg_double_bass_body_model_is_valid(&model->body)) {
    return 0;
  }
  for (string_index = 0U; string_index < WG_DOUBLE_BASS_STRINGS; string_index++) {
    if (!wg_double_bass_string_model_is_valid(&model->strings[string_index])) {
      return 0;
    }
  }
  return 1;
}

static double wg_double_bass_model_friction_mu(
    const WG_DOUBLE_BASS_MODEL *model, double speed)
{
  const WG_DOUBLE_BASS_FRICTION_MODEL *friction = &model->bow.friction;

  speed = fabs(speed);
  return friction->floor_mu +
      friction->slow_gain * exp(-speed / friction->slow_speed) +
      friction->fast_gain * exp(-speed / friction->fast_speed);
}

static double wg_double_bass_model_static_mu(
    const WG_DOUBLE_BASS_MODEL *model)
{
  const WG_DOUBLE_BASS_FRICTION_MODEL *friction = &model->bow.friction;
  return friction->static_mu;
}

static double wg_double_bass_model_thermal_drop(
    const WG_DOUBLE_BASS_MODEL *model)
{
  /* The test-only build sets WG_DOUBLE_BASS_THERMAL_DROP to zero. */
  return model->bow.contact.thermal_drop *
      (WG_DOUBLE_BASS_THERMAL_DROP / 0.08);
}

static const WG_DOUBLE_BASS_MODEL *wg_double_bass_bound_model(
    const WG_DOUBLE_BASS_STRING_STATE *string)
{
  return string != NULL && string->model != NULL
      ? string->model : wg_double_bass_fixed_model();
}

static const WG_DOUBLE_BASS_STRING_MODEL *wg_double_bass_bound_string_model(
    const WG_DOUBLE_BASS_STRING_STATE *string)
{
  const WG_DOUBLE_BASS_MODEL *model = wg_double_bass_bound_model(string);
  const uint32_t string_index = string != NULL &&
          string->string_index < WG_DOUBLE_BASS_STRINGS
      ? string->string_index : 0U;
  return &model->strings[string_index];
}

static double wg_double_bass_harmonic_touch_level(
    const WG_DOUBLE_BASS_STRING_STATE *string, uint32_t order)
{
  if (order < WG_DOUBLE_BASS_HARMONIC_MIN_ORDER ||
      order > WG_DOUBLE_BASS_HARMONIC_MAX_ORDER) {
    return 0.0;
  }
  return wg_double_bass_bound_model(string)->harmonics.touch_level[
      order - WG_DOUBLE_BASS_HARMONIC_MIN_ORDER];
}

static uint32_t wg_double_bass_engine_ksmps(CSOUND *csound)
{
  const double sample_rate = (double)csound->GetEngineSr(csound);
  const double control_rate = (double)csound->GetEngineKr(csound);
  double value;

  if (!isfinite(sample_rate) || !isfinite(control_rate) ||
      sample_rate <= 0.0 || control_rate <= 0.0) {
    return 0U;
  }
  value = sample_rate / control_rate;
  if (!isfinite(value) || value < 1.0 || value > (double)UINT32_MAX) {
    return 0U;
  }
  return (uint32_t)floor(value + 0.5);
}

static int32_t wg_double_bass_read_handle(const MYFLT *value)
{
  const double handle = value != NULL ? (double)*value : 0.0;
  if (!isfinite(handle) || handle < 1.0 ||
      handle > (double)WG_DOUBLE_BASS_MAX_HANDLE || floor(handle) != handle) {
    return -1;
  }
  return (int32_t)handle;
}

static uint64_t wg_double_bass_add_samples(uint64_t total, uint64_t samples)
{
  return UINT64_MAX - total < samples ? UINT64_MAX : total + samples;
}

static int32_t wg_double_bass_checked_size_multiply(
    size_t left, size_t right, size_t *product)
{
  if (product == NULL || (left != 0U && right > SIZE_MAX / left)) {
    return 0;
  }
  *product = left * right;
  return 1;
}

static double wg_double_bass_decay(uint64_t samples, double sample_rate,
                              double decay_seconds)
{
  if (samples == 0U || !(sample_rate > 0.0) || !(decay_seconds > 0.0)) {
    return 1.0;
  }
  return exp(-(double)samples / (sample_rate * decay_seconds));
}

static double wg_double_bass_smoothing_coefficient(double seconds,
                                              double sample_rate)
{
  return 1.0 - exp(-1.0 / fmax(1.0, seconds * sample_rate));
}

static int32_t wg_double_bass_fundamental_is_in_range(
    const WG_DOUBLE_BASS_STATE *state, double frequency)
{
  return state != NULL && isfinite(frequency) &&
      frequency >= state->strings[0].open_frequency &&
      frequency <= state->normal_max_frequency;
}

static int32_t wg_double_bass_string_can_play(
    const WG_DOUBLE_BASS_STATE *state, uint32_t string_index, double frequency)
{
  return state != NULL && string_index < WG_DOUBLE_BASS_STRINGS &&
      wg_double_bass_fundamental_is_in_range(state, frequency) &&
      frequency >= state->strings[string_index].open_frequency;
}

static uint32_t wg_double_bass_harmonic_order(double value)
{
  if (!isfinite(value) || value < (double)WG_DOUBLE_BASS_HARMONIC_MIN_ORDER ||
      value > (double)WG_DOUBLE_BASS_HARMONIC_MAX_ORDER || floor(value) != value) {
    return 0U;
  }
  return (uint32_t)value;
}

static int32_t wg_double_bass_pitch_is_within_snap_guard(
    double frequency, double boundary)
{
  const double guard = WG_DOUBLE_BASS_PITCH_SNAP_EPSILONS * DBL_EPSILON *
      fmax(1.0, fabs(boundary));

  return fabs(frequency - boundary) <= guard;
}

static double wg_double_bass_canonical_sounding_frequency(
    const WG_DOUBLE_BASS_STATE *state, double sounding_frequency,
    double harmonic_request)
{
  const uint32_t order = wg_double_bass_harmonic_order(harmonic_request);
  const double multiplier = order != 0U ? (double)order : 1.0;
  uint32_t string_index;

  if (state == NULL || !isfinite(sounding_frequency)) {
    return sounding_frequency;
  }
  for (string_index = 0U; string_index < WG_DOUBLE_BASS_STRINGS;
       string_index++) {
    const double boundary =
        multiplier * state->strings[string_index].open_frequency;

    if (wg_double_bass_pitch_is_within_snap_guard(
            sounding_frequency, boundary)) {
      return boundary;
    }
  }
  {
    const double boundary = multiplier * state->normal_max_frequency;

    if (wg_double_bass_pitch_is_within_snap_guard(
            sounding_frequency, boundary)) {
      return boundary;
    }
  }
  return sounding_frequency;
}

static int32_t wg_double_bass_sounded_harmonic_exceeds_dsp_range(
    const WG_DOUBLE_BASS_STATE *state, double sounding_frequency,
    double harmonic_request)
{
  const uint32_t order = wg_double_bass_harmonic_order(harmonic_request);
  double multiplier;
  double low_sounding;
  double high_sounding;

  if (state == NULL || order == 0U || !isfinite(sounding_frequency)) {
    return 0;
  }
  multiplier = (double)order;
  low_sounding = multiplier * state->strings[0].open_frequency;
  high_sounding = multiplier * state->normal_max_frequency;
  return sounding_frequency >= low_sounding &&
      sounding_frequency <= high_sounding &&
      sounding_frequency > state->sounded_max_frequency;
}

static int32_t wg_double_bass_pitch_request_is_valid(
    const WG_DOUBLE_BASS_STATE *state, double sounding_frequency,
    double harmonic_request, uint32_t *order_out, double *fundamental_out)
{
  const uint32_t order = wg_double_bass_harmonic_order(harmonic_request);
  double fundamental = sounding_frequency;

  if (state == NULL || !isfinite(sounding_frequency)) {
    return 0;
  }
  if (order != 0U) {
    const double multiplier = (double)order;
    const double low_sounding =
        multiplier * state->strings[0].open_frequency;
    const double high_sounding = multiplier * state->normal_max_frequency;
    uint32_t string_index;

    if (sounding_frequency < low_sounding ||
        sounding_frequency > high_sounding ||
        sounding_frequency > state->sounded_max_frequency) {
      return 0;
    }
    fundamental = sounding_frequency / multiplier;
    if (sounding_frequency == high_sounding) {
      fundamental = state->normal_max_frequency;
    }
    for (string_index = 0U; string_index < WG_DOUBLE_BASS_STRINGS;
         string_index++) {
      if (sounding_frequency ==
          multiplier * state->strings[string_index].open_frequency) {
        fundamental = state->strings[string_index].open_frequency;
        break;
      }
    }
  }
  if (!wg_double_bass_fundamental_is_in_range(state, fundamental)) {
    return 0;
  }
  if (order_out != NULL) {
    *order_out = order;
  }
  if (fundamental_out != NULL) {
    *fundamental_out = fundamental;
  }
  return 1;
}

static double wg_double_bass_stop_position(double open_frequency,
                                      double frequency)
{
  if (!(frequency > open_frequency)) {
    return 0.0;
  }
  return 1.0 - open_frequency / frequency;
}

static double wg_double_bass_signed_noise(uint32_t *state)
{
  uint32_t value = *state;

  if (value == 0U) {
    value = 0x6d2b79f5U;
  }
  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  *state = value;
  return (double)(value >> 8) * (2.0 / 16777215.0) - 1.0;
}

static double wg_double_bass_linear_delay_read(const double *data, uint32_t size,
                                          uint32_t write_index, double delay)
{
  double fraction;
  uint32_t whole;
  uint32_t index0;
  uint32_t index1;

  if (data == NULL || size < 4U) {
    return 0.0;
  }
  delay = wg_double_bass_clamp(
      delay, WG_DOUBLE_BASS_MIN_BRANCH_DELAY, (double)size - 2.0);
  whole = (uint32_t)floor(delay);
  fraction = delay - (double)whole;
  index0 = (uint32_t)(
      ((uint64_t)write_index + (uint64_t)size - (uint64_t)whole) %
      (uint64_t)size);
  index1 = index0 == 0U ? size - 1U : index0 - 1U;
  return (1.0 - fraction) * data[index0] + fraction * data[index1];
}

static void wg_double_bass_cubic_delay_coefficients(
    double fraction, double *c0, double *c1, double *c2, double *c3)
{
  *c0 = -(fraction - 1.0) * (fraction - 2.0) *
      (fraction - 3.0) / 6.0;
  *c1 = fraction * (fraction - 2.0) * (fraction - 3.0) / 2.0;
  *c2 = -fraction * (fraction - 1.0) * (fraction - 3.0) / 2.0;
  *c3 = fraction * (fraction - 1.0) * (fraction - 2.0) / 6.0;
}

static double wg_double_bass_cubic_delay_read(const double *data, uint32_t size,
                                         uint32_t write_index, double delay)
{
  double fraction;
  double offset;
  double c0;
  double c1;
  double c2;
  double c3;
  uint32_t whole;
  uint32_t index0;
  uint32_t index1;
  uint32_t index2;
  uint32_t index3;

  if (data == NULL || size < 4U) {
    return 0.0;
  }
  delay = wg_double_bass_clamp(
      delay, WG_DOUBLE_BASS_MIN_BRANCH_DELAY, (double)size - 2.0);
  whole = (uint32_t)floor(delay);
  fraction = delay - (double)whole;
  if (whole < 2U) {
    return wg_double_bass_linear_delay_read(data, size, write_index, delay);
  }
  offset = 1.0 + fraction;
  index0 = (uint32_t)(
      ((uint64_t)write_index + (uint64_t)size -
       (uint64_t)(whole - 1U)) %
      (uint64_t)size);
  index1 = index0 == 0U ? size - 1U : index0 - 1U;
  index2 = index1 == 0U ? size - 1U : index1 - 1U;
  index3 = index2 == 0U ? size - 1U : index2 - 1U;
  wg_double_bass_cubic_delay_coefficients(
      offset, &c0, &c1, &c2, &c3);
  return c0 * data[index0] + c1 * data[index1] +
      c2 * data[index2] + c3 * data[index3];
}

static void wg_double_bass_split_delay(double total, double position,
                                  double *bridge, double *nut)
{
  total = fmax(2.0 * WG_DOUBLE_BASS_MIN_BRANCH_DELAY, total);
  position = wg_double_bass_clamp(position, 0.01, 0.49);
  *bridge = wg_double_bass_clamp(
      position * total, WG_DOUBLE_BASS_MIN_BRANCH_DELAY,
      total - WG_DOUBLE_BASS_MIN_BRANCH_DELAY);
  *nut = total - *bridge;
}

static double wg_double_bass_cubic_phase_delay(double delay, double omega)
{
  const double whole = floor(delay);
  const double fraction = delay - whole;
  double offset;
  double c0;
  double c1;
  double c2;
  double c3;
  double real;
  double negative_imaginary;

  if (!(omega > 0.0)) {
    return delay;
  }
  if (whole < 2.0) {
    return whole + atan2(
        fraction * sin(omega),
        1.0 - fraction + fraction * cos(omega)) / omega;
  }
  offset = 1.0 + fraction;
  wg_double_bass_cubic_delay_coefficients(
      offset, &c0, &c1, &c2, &c3);
  real = c0 + c1 * cos(omega) + c2 * cos(2.0 * omega) +
      c3 * cos(3.0 * omega);
  negative_imaginary = c1 * sin(omega) + c2 * sin(2.0 * omega) +
      c3 * sin(3.0 * omega);
  return whole - 1.0 + atan2(negative_imaginary, real) / omega;
}

static double wg_double_bass_reflection_phase_delay(double pole, double omega)
{
  if (!(omega > 0.0)) {
    return pole / fmax(1.0e-12, 1.0 - pole);
  }
  return atan2(pole * sin(omega), 1.0 - pole * cos(omega)) / omega;
}

static double wg_double_bass_loop_phase_samples(
    const WG_DOUBLE_BASS_STRING_STATE *string, double delay,
    double position, double frequency, double sample_rate)
{
  const double omega = WG_DOUBLE_BASS_TWO_PI * frequency / sample_rate;
  double bridge_delay;
  double nut_delay;

  wg_double_bass_split_delay(delay, position, &bridge_delay, &nut_delay);
  return wg_double_bass_cubic_phase_delay(bridge_delay, omega) +
      wg_double_bass_cubic_phase_delay(nut_delay, omega) +
      wg_double_bass_reflection_phase_delay(string->nut_pole, omega) +
      wg_double_bass_reflection_phase_delay(string->bridge_pole, omega);
}

static double wg_double_bass_solve_delay(
    const WG_DOUBLE_BASS_STRING_STATE *string, double frequency, double position,
    double sample_rate)
{
  const double wanted = sample_rate / frequency;
  double low = 2.0;
  double high = (double)string->rail_size - 2.0;
  uint32_t iteration;

  if (wg_double_bass_loop_phase_samples(
          string, low, position, frequency, sample_rate) >= wanted) {
    return low;
  }
  if (wg_double_bass_loop_phase_samples(
          string, high, position, frequency, sample_rate) <= wanted) {
    return high;
  }
  for (iteration = 0U; iteration < 48U; iteration++) {
    const double middle = 0.5 * (low + high);
    if (wg_double_bass_loop_phase_samples(
            string, middle, position, frequency, sample_rate) < wanted) {
      low = middle;
    } else {
      high = middle;
    }
  }
  return 0.5 * (low + high);
}

static double wg_double_bass_solve_harmonic_delay(
    const WG_DOUBLE_BASS_STRING_STATE *string, double fundamental_frequency,
    double tuning_frequency, double position, double sample_rate)
{
  const double wanted = sample_rate / fundamental_frequency;
  double low = 2.0;
  double high = (double)string->rail_size - 2.0;
  uint32_t iteration;

  if (wg_double_bass_loop_phase_samples(
          string, low, position, tuning_frequency, sample_rate) >= wanted) {
    return low;
  }
  if (wg_double_bass_loop_phase_samples(
          string, high, position, tuning_frequency, sample_rate) <= wanted) {
    return high;
  }
  for (iteration = 0U; iteration < 48U; iteration++) {
    const double middle = 0.5 * (low + high);
    if (wg_double_bass_loop_phase_samples(
            string, middle, position, tuning_frequency, sample_rate) < wanted) {
      low = middle;
    } else {
      high = middle;
    }
  }
  return 0.5 * (low + high);
}

static double wg_double_bass_reflection_tick(double input, double pole,
                                        double gain, double *state)
{
  *state = pole * *state + (1.0 - pole) * input;
  return -gain * *state;
}

static void wg_double_bass_reset_gesture_live(WG_DOUBLE_BASS_STRING_STATE *string)
{
  string->gesture_phase = 0.0;
  string->gesture_force_target = wg_double_bass_clamp(
      string->force, 0.0, 1.0);
  string->gesture_speed_target = wg_double_bass_clamp(
      string->speed, -1.0, 1.0);
  string->gesture_position_target = wg_double_bass_clamp(
      string->position, 0.01, 0.49);
  string->gesture_force_output = 0.0;
  string->gesture_speed_output = 0.0;
  string->gesture_contact_output = 0.0;
  string->gesture_position_output = string->gesture_position_target;
  string->gesture_transition_force = 0.0;
  string->gesture_transition_speed = 0.0;
  string->gesture_transition_contact = 0.0;
  string->gesture_transition_position = string->gesture_position_output;
  string->gesture_transition_mix = 1.0;
  string->gesture_bow_change_start_speed = 0.0;
  string->gesture_bow_change_target_speed = 0.0;
  string->gesture_bow_change_mix = 1.0;
  string->gesture_tremolo_phase = 0.25;
  string->gesture_tremolo_rate = 10.0;
  string->gesture_articulation = string->articulation;
  string->gesture_age = 0U;
  string->gesture_stroke_age = 0U;
  string->gesture_onset_samples = 0U;
  string->gesture_stroke_samples = 0U;
  string->gesture_active = 0;
  string->gesture_initialized = 0;
  string->gesture_bow_change_active = 0;
  string->gesture_command_direction = 0;
}

static void wg_double_bass_reset_bow_mechanics(
    WG_DOUBLE_BASS_STRING_STATE *string)
{
  string->contact_memory = 0.0;
  string->contact_memory_velocity = 0.0;
  string->contact_adhesion = 0.0;
  string->contact_steady_displacement = 0.0;
  string->contact_force = 0.0;
  string->contact_temperature = 0.0;
  string->contact_thermal_scale = 1.0;
  string->hair_displacement = 0.0;
  string->hair_velocity = 0.0;
  string->hair_effective_speed = string->bow_speed;
  string->board_displacement = 0.0;
  string->board_compression = 0.0;
  string->board_force = 0.0;
  string->board_previous_compression = 0.0;
  string->bow_noise_highpass = 0.0;
  string->bow_noise_previous_white = 0.0;
  string->bow_noise_last = 0.0;
  string->mechanical_envelope = 0.0;
  string->coupling_input = 0.0;
  string->coupling_source_mask = 0U;
  string->contact_solver_iterations = 0U;
  string->mechanical_last_bow_direction = 0;
  string->mechanical_contact_positive = 0;
}

static void wg_double_bass_reset_strange_live(
    WG_DOUBLE_BASS_STRING_STATE *string)
{
  string->strange_air_state = 0.0;
  string->strange_air_previous_white = 0.0;
  string->strange_dispersion_input = 0.0;
  string->strange_dispersion_output = 0.0;
  string->strange_subharmonic_phase = 0.0;
  string->strange_subharmonic_envelope = 0.0;
  string->strange_squeal_phase = 0.0;
  string->strange_squeal_state = 0.0;
  string->strange_coupling_mix = 0.0;
  string->strange_last_output = 0.0;
  string->strange_last_bow_state = WG_DOUBLE_BASS_BOW_OFF;
}

static int32_t wg_double_bass_strange_live_is_finite(
    const WG_DOUBLE_BASS_STRING_STATE *string)
{
  return isfinite(string->strange_air_state) &&
      isfinite(string->strange_air_previous_white) &&
      isfinite(string->strange_dispersion_input) &&
      isfinite(string->strange_dispersion_output) &&
      isfinite(string->strange_subharmonic_phase) &&
      isfinite(string->strange_subharmonic_envelope) &&
      isfinite(string->strange_squeal_phase) &&
      isfinite(string->strange_squeal_state) &&
      isfinite(string->strange_coupling_mix) &&
      isfinite(string->strange_last_output) &&
      isfinite(string->strange_energy) &&
      isfinite(string->strange_peak) &&
      fabs(string->strange_air_state) <= 1.0 &&
      fabs(string->strange_dispersion_input) <= WG_DOUBLE_BASS_WAVE_LIMIT &&
      fabs(string->strange_dispersion_output) <= WG_DOUBLE_BASS_WAVE_LIMIT &&
      string->strange_subharmonic_phase >= 0.0 &&
      string->strange_subharmonic_phase < 1.0 &&
      string->strange_squeal_phase >= 0.0 &&
      string->strange_squeal_phase < 1.0 &&
      fabs(string->strange_subharmonic_envelope) <=
          WG_DOUBLE_BASS_STRANGE_SUBHARMONIC_LIMIT &&
      fabs(string->strange_squeal_state) <=
          WG_DOUBLE_BASS_STRANGE_SQUEAL_LIMIT &&
      string->strange_coupling_mix >= 0.0 &&
      string->strange_coupling_mix <= 1.0 &&
      string->strange_energy >= 0.0 &&
      string->strange_energy <= 1.0e12 &&
      string->strange_peak >= 0.0 &&
      string->strange_peak <= WG_DOUBLE_BASS_STRANGE_OUTPUT_LIMIT &&
      fabs(string->strange_last_output) <= WG_DOUBLE_BASS_STRANGE_OUTPUT_LIMIT;
}

static void wg_double_bass_recover_strange(WG_DOUBLE_BASS_STRING_STATE *string)
{
  string->strange_recoveries = wg_double_bass_add_samples(
      string->strange_recoveries, 1U);
  if (!isfinite(string->strange_energy) ||
      string->strange_energy < 0.0 || string->strange_energy > 1.0e12) {
    string->strange_energy = 0.0;
  }
  if (!isfinite(string->strange_peak) || string->strange_peak < 0.0 ||
      string->strange_peak > WG_DOUBLE_BASS_STRANGE_OUTPUT_LIMIT) {
    string->strange_peak = 0.0;
  }
  wg_double_bass_reset_strange_live(string);
}

static int32_t wg_double_bass_sympathetic_state_is_finite(
    const WG_DOUBLE_BASS_STRING_STATE *string)
{
  return wg_double_bass_strange_live_is_finite(string) &&
      isfinite(string->passive_open_level) &&
      isfinite(string->passive_open_energy) &&
      isfinite(string->passive_open_peak) &&
      string->passive_open_level >= 0.0 &&
      string->passive_open_level <= WG_DOUBLE_BASS_WAVE_LIMIT &&
      string->passive_open_energy >= 0.0 &&
      string->passive_open_peak >= 0.0 &&
      string->passive_open_peak <= WG_DOUBLE_BASS_WAVE_LIMIT &&
      (string->passive_open_active == 0 ||
       string->passive_open_active == 1);
}

static void wg_double_bass_recover_sympathetic_state(
    WG_DOUBLE_BASS_STRING_STATE *string)
{
  if (!wg_double_bass_strange_live_is_finite(string)) {
    wg_double_bass_recover_strange(string);
  } else {
    string->strange_recoveries = wg_double_bass_add_samples(
        string->strange_recoveries, 1U);
  }
  if (!isfinite(string->passive_open_energy) ||
      string->passive_open_energy < 0.0) {
    string->passive_open_energy = 0.0;
  }
  if (!isfinite(string->passive_open_peak) ||
      string->passive_open_peak < 0.0 ||
      string->passive_open_peak > WG_DOUBLE_BASS_WAVE_LIMIT) {
    string->passive_open_peak = 0.0;
  }
  string->passive_open_level = 0.0;
  string->passive_open_active = 0;
}

static int32_t wg_double_bass_bow_mechanics_is_finite(
    const WG_DOUBLE_BASS_STRING_STATE *string)
{
  const double thermal_drop = wg_double_bass_model_thermal_drop(
      wg_double_bass_bound_model(string));
  return isfinite(string->contact_memory) &&
      isfinite(string->contact_memory_velocity) &&
      isfinite(string->contact_adhesion) &&
      isfinite(string->contact_steady_displacement) &&
      isfinite(string->contact_force) &&
      isfinite(string->contact_temperature) &&
      isfinite(string->contact_thermal_scale) &&
      isfinite(string->contact_thermal_work) &&
      isfinite(string->contact_memory_release) &&
      isfinite(string->hair_displacement) &&
      isfinite(string->hair_velocity) &&
      isfinite(string->hair_effective_speed) &&
      isfinite(string->board_displacement) &&
      isfinite(string->board_compression) &&
      isfinite(string->board_force) &&
      isfinite(string->board_energy) &&
      isfinite(string->bow_noise_highpass) &&
      isfinite(string->bow_noise_previous_white) &&
      isfinite(string->bow_noise_last) &&
      isfinite(string->bow_noise_energy) &&
      isfinite(string->bow_noise_peak) &&
      isfinite(string->mechanical_envelope) &&
      isfinite(string->mechanical_decay) &&
      isfinite(string->board_active_decay) &&
      isfinite(string->board_inactive_decay) &&
#if defined(HLOLLI_WG_DOUBLE_BASS_TEST_API)
      isfinite(string->diagnostic_finger_gain) &&
      isfinite(string->diagnostic_board_gain) &&
      isfinite(string->diagnostic_rosin_gain) &&
      isfinite(string->diagnostic_mechanical_gain) &&
      isfinite(string->diagnostic_nut_cutoff_scale) &&
      isfinite(string->diagnostic_bridge_cutoff_scale) &&
#endif
      isfinite(string->coupling_input) &&
      isfinite(string->coupling_energy) &&
      isfinite(string->coupling_peak) &&
      fabs(string->contact_memory) <= WG_DOUBLE_BASS_CONTACT_MEMORY_LIMIT &&
      string->contact_temperature >= 0.0 &&
      string->contact_temperature <= 1.0 &&
      string->contact_thermal_scale >= 1.0 - thermal_drop &&
      string->contact_thermal_scale <= 1.0 &&
      fabs(string->hair_displacement) <=
          WG_DOUBLE_BASS_HAIR_DISPLACEMENT_LIMIT &&
      fabs(string->hair_velocity) <= WG_DOUBLE_BASS_HAIR_SPEED_LIMIT &&
      fabs(string->board_displacement) <=
          WG_DOUBLE_BASS_BOARD_DISPLACEMENT_LIMIT &&
      string->board_compression >= 0.0 &&
      fabs(string->board_force) <=
          2.0 * string->characteristic_impedance *
              WG_DOUBLE_BASS_BOARD_VELOCITY_LIMIT
#if defined(HLOLLI_WG_DOUBLE_BASS_TEST_API)
      &&
      string->diagnostic_finger_gain >= 0.0 &&
      string->diagnostic_finger_gain <= 1.0 &&
      string->diagnostic_board_gain >= 0.0 &&
      string->diagnostic_board_gain <= 1.0 &&
      string->diagnostic_rosin_gain >= 0.0 &&
      string->diagnostic_rosin_gain <= 1.0 &&
      string->diagnostic_mechanical_gain >= 0.0 &&
      string->diagnostic_mechanical_gain <= 1.0 &&
      string->diagnostic_nut_cutoff_scale >= 0.125 &&
      string->diagnostic_nut_cutoff_scale <= 4.0 &&
      string->diagnostic_bridge_cutoff_scale >= 0.125 &&
      string->diagnostic_bridge_cutoff_scale <= 4.0
#endif
      ;
}

static void wg_double_bass_recover_bow_mechanics(
    WG_DOUBLE_BASS_STRING_STATE *string)
{
  string->bow_mechanics_recoveries = wg_double_bass_add_samples(
      string->bow_mechanics_recoveries, 1U);
  if (!isfinite(string->contact_thermal_work)) {
    string->contact_thermal_work = 0.0;
  }
  if (!isfinite(string->board_energy)) {
    string->board_energy = 0.0;
  }
  if (!isfinite(string->bow_noise_energy)) {
    string->bow_noise_energy = 0.0;
  }
  if (!isfinite(string->bow_noise_peak)) {
    string->bow_noise_peak = 0.0;
  }
  if (!isfinite(string->coupling_energy)) {
    string->coupling_energy = 0.0;
  }
  if (!isfinite(string->coupling_peak)) {
    string->coupling_peak = 0.0;
  }
  wg_double_bass_reset_bow_mechanics(string);
}

static void wg_double_bass_clear_contact_response(
    WG_DOUBLE_BASS_STRING_STATE *string)
{
  string->contact_force = 0.0;
  string->contact_solver_iterations = 0U;
  string->contact_adhesion = 0.0;
  string->contact_steady_displacement = 0.0;
}

static void wg_double_bass_reset_bow_live(WG_DOUBLE_BASS_STRING_STATE *string)
{
  string->bow_position_target = wg_double_bass_clamp(
      string->position, 0.01, 0.49);
  string->bow_position = string->bow_position_target;
  string->bow_speed_target = 0.0;
  string->bow_speed = 0.0;
  string->bow_contact_target = 0.0;
  string->bow_contact = 0.0;
  string->bow_requested_force = 0.0;
  string->bow_effective_force = 0.0;
  string->bow_min_force = 0.0;
  string->bow_max_force = 0.0;
  string->bow_free_velocity = 0.0;
  string->bow_string_velocity = 0.0;
  string->bow_relative_velocity = 0.0;
  string->bow_friction_force = 0.0;
  string->bow_junction_increment = 0.0;
  string->bow_previous_slip_speed = 0.0;
  string->bow_solver_residual = 0.0;
  string->bow_solver_bracket_width = 0.0;
  string->bow_scratch_score = 0.0;
  string->bow_root_count = 0U;
  string->bow_solver_iterations = 0U;
  string->bow_failure_streak = 0U;
  string->bow_clip_streak = 0U;
  string->bow_recovery_remaining = 0U;
  string->bow_direction = 0;
  string->bow_state = WG_DOUBLE_BASS_BOW_OFF;
}

static void wg_double_bass_reset_exciter_live(WG_DOUBLE_BASS_STRING_STATE *string)
{
  string->exciter_position = wg_double_bass_clamp(
      string->position, 0.01, 0.49);
  string->exciter_amplitude = 0.0;
  string->exciter_noise_level = 0.0;
  string->exciter_impact_amplitude = 0.0;
  string->exciter_contact_target = 0.0;
  string->exciter_contact = 0.0;
  string->exciter_speed_target = 0.0;
  string->exciter_speed = 0.0;
  string->exciter_noise_highpass = 0.0;
  string->exciter_noise_previous_white = 0.0;
  string->exciter_last_output = 0.0;
  string->collision_compression = 0.0;
  string->collision_stick_speed = 0.0;
  string->collision_stiffness = 0.0;
  string->collision_damping = 0.0;
  string->collision_mass = 0.0;
  string->collision_force = 0.0;
  string->exciter_mode = 0U;
  string->exciter_gate_mode = 0U;
  string->exciter_age = 0U;
  string->exciter_total_samples = 0U;
  string->exciter_pulse_samples = 0U;
  string->exciter_notch_delay = 0U;
  string->exciter_impact_delay = 0U;
  string->exciter_impact_samples = 0U;
  string->collision_sample_limit = 0U;
  string->exciter_armed = 1;
  string->exciter_impact_counted = 0;
}

static void wg_double_bass_zero_waveguide(WG_DOUBLE_BASS_STRING_STATE *string,
                                     int32_t count_reset)
{
  if (string->toward_bridge != NULL && string->rail_size > 0U) {
    memset(string->toward_bridge, 0,
           (size_t)string->rail_size * sizeof(double));
  }
  if (string->toward_nut != NULL && string->rail_size > 0U) {
    memset(string->toward_nut, 0,
           (size_t)string->rail_size * sizeof(double));
  }
  if (string->second_toward_bridge != NULL && string->rail_size > 0U) {
    memset(string->second_toward_bridge, 0,
           (size_t)string->rail_size * sizeof(double));
  }
  if (string->second_toward_nut != NULL && string->rail_size > 0U) {
    memset(string->second_toward_nut, 0,
           (size_t)string->rail_size * sizeof(double));
  }
  if (string->harmonic_history != NULL && string->rail_size > 0U) {
    memset(string->harmonic_history, 0,
           (size_t)string->rail_size * sizeof(double));
  }
  string->bridge_state = 0.0;
  string->nut_state = 0.0;
  string->second_bridge_state = 0.0;
  string->second_nut_state = 0.0;
  string->bridge_dynamic_input = 0.0;
  string->bridge_dynamic_output = 0.0;
  string->last_bridge_output = 0.0;
  string->delay_velocity = 0.0;
  string->finger_noise_envelope = 0.0;
  string->finger_noise_highpass = 0.0;
  string->finger_noise_previous_white = 0.0;
  string->finger_last_noise = 0.0;
  string->finger_motion_kind = WG_DOUBLE_BASS_FINGER_STILL;
  string->harmonic_write_index = 0U;
  string->harmonic_touch_pressure = 0.0;
  string->harmonic_order_mix = 0.0;
  string->harmonic_touch_period = 0.0;
  string->harmonic_projection_input = 0.0;
  string->harmonic_projection_output = 0.0;
  string->release_loop_gain = 1.0;
  string->release_loop_gain_target = 1.0;
  string->release_model_articulation = string->articulation;
  string->release_model_speed = string->speed;
  string->release_gate_positive = 0;
  string->passive_open_level = 0.0;
  string->passive_open_active = 0;
  wg_double_bass_reset_gesture_live(string);
  wg_double_bass_reset_bow_live(string);
  wg_double_bass_reset_exciter_live(string);
  wg_double_bass_reset_bow_mechanics(string);
  wg_double_bass_reset_strange_live(string);
  if (count_reset) {
    string->wave_resets = wg_double_bass_add_samples(string->wave_resets, 1U);
  }
}

static void wg_double_bass_refresh_release_gain_target(
    WG_DOUBLE_BASS_STRING_STATE *string);

static void wg_double_bass_prepare_harmonic(
    const WG_DOUBLE_BASS_STATE *state, WG_DOUBLE_BASS_STRING_STATE *string,
    double request, double sounding_frequency)
{
  uint32_t requested_order = 0U;
  double requested_fundamental = sounding_frequency;
  const int32_t range_valid = wg_double_bass_pitch_request_is_valid(
      state, sounding_frequency, request,
      &requested_order, &requested_fundamental);
  const int32_t normal_request = isfinite(request) && fabs(request) <= 1.0e-12;
  const int32_t playable = range_valid && requested_order != 0U &&
      wg_double_bass_string_can_play(
          state, string->string_index, requested_fundamental);
  const uint32_t active_order = playable ? requested_order : 0U;
  const int32_t valid = (normal_request && range_valid) || playable;
  const int32_t rejected = !normal_request && !playable;
  const double fundamental = active_order != 0U
      ? requested_fundamental : sounding_frequency;
  double stop_position = wg_double_bass_stop_position(
      string->open_frequency, fundamental);
  double touch_position = 0.0;
  int32_t natural = 0;

  if (active_order != 0U) {
    const double speaking_length = string->open_frequency / fundamental;

    natural = fundamental == string->open_frequency;
    if (natural) {
      stop_position = 0.0;
    }
    touch_position = natural
        ? 1.0 / (double)active_order
        : stop_position + speaking_length / (double)active_order;
    touch_position = wg_double_bass_clamp(touch_position, 0.0, 1.0);
  }
  if (string->harmonic_initialized &&
      (string->harmonic != active_order ||
       string->harmonic_valid != valid ||
       string->harmonic_natural != natural)) {
    string->harmonic_transitions = wg_double_bass_add_samples(
        string->harmonic_transitions, 1U);
  }
  if (rejected && !string->harmonic_rejected_state) {
    string->harmonic_rejections = wg_double_bass_add_samples(
        string->harmonic_rejections, 1U);
  }
  string->harmonic_request = isfinite(request) ? request : 0.0;
  string->sounding_frequency = sounding_frequency;
  string->harmonic_fundamental_frequency = fundamental;
  string->harmonic_stop_position = stop_position;
  string->harmonic_touch_position = touch_position;
  string->harmonic = active_order;
  string->harmonic_valid = valid;
  string->harmonic_natural = natural;
  string->harmonic_rejected_state = rejected;
  if (!string->harmonic_initialized) {
    string->harmonic_filter_order = active_order;
    string->harmonic_pending_order = active_order;
    string->harmonic_order_mix = 0.0;
  } else if (active_order == 0U) {
    string->harmonic_pending_order = 0U;
    string->harmonic_order_mix = 0.0;
  } else if (string->harmonic_filter_order == 0U ||
             string->harmonic_touch_pressure <= 1.0e-5) {
    string->harmonic_filter_order = active_order;
    string->harmonic_pending_order = active_order;
    string->harmonic_order_mix = 0.0;
  } else if (active_order == string->harmonic_filter_order) {
    string->harmonic_pending_order = active_order;
    string->harmonic_order_mix = 0.0;
  } else if (active_order != string->harmonic_pending_order) {
    if (string->harmonic_order_mix >= 0.5 &&
        string->harmonic_pending_order != 0U) {
      string->harmonic_filter_order = string->harmonic_pending_order;
    }
    string->harmonic_pending_order = active_order;
    string->harmonic_order_mix = 0.0;
  }
  if (active_order != 0U) {
    string->harmonic_touch_target = wg_double_bass_harmonic_touch_level(
        string, active_order);
  } else {
    string->harmonic_touch_target = 0.0;
  }
  string->harmonic_initialized = 1;
}

static void wg_double_bass_prepare_waveguide(WG_DOUBLE_BASS_STRING_STATE *string,
                                        double frequency,
                                        double sample_rate)
{
  const WG_DOUBLE_BASS_STRING_MODEL *model =
      wg_double_bass_bound_string_model(string);
  const WG_DOUBLE_BASS_POLARIZATION_MODEL *second =
      &model->second_polarization;
  double nut_cutoff = model->nut_cutoff_hz;
  double bridge_cutoff = model->bridge_cutoff_hz;
  const double nut_loss_fraction = model->nut_loss_fraction;
  const double bridge_loss_fraction = 1.0 - nut_loss_fraction;
  double loop_gain;

  const double position = wg_double_bass_clamp(string->position, 0.01, 0.49);
  double tuning_frequency;

#if defined(HLOLLI_WG_DOUBLE_BASS_TEST_API)
  nut_cutoff *= string->diagnostic_nut_cutoff_scale;
  bridge_cutoff *= string->diagnostic_bridge_cutoff_scale;
#endif
  nut_cutoff = fmin(nut_cutoff, 0.45 * sample_rate);
  bridge_cutoff = fmin(bridge_cutoff, 0.45 * sample_rate);

  tuning_frequency = string->harmonic != 0U && string->harmonic_valid
      ? string->sounding_frequency : frequency;
  if (string->delay_initialized &&
      frequency == string->target_frequency &&
      tuning_frequency == string->delay_tuning_frequency &&
      position == string->delay_position) {
    wg_double_bass_refresh_release_gain_target(string);
    return;
  }
  string->target_frequency = frequency;
  string->delay_tuning_frequency = tuning_frequency;
  string->delay_position = position;
  string->bow_position_target = position;
  string->nut_pole = exp(-WG_DOUBLE_BASS_TWO_PI * nut_cutoff / sample_rate);
  string->bridge_pole = exp(
      -WG_DOUBLE_BASS_TWO_PI * bridge_cutoff / sample_rate);
  loop_gain = exp(
      -1.0 / (frequency * model->loss_time_constant_seconds));
  string->nut_gain_target = wg_double_bass_clamp(
      pow(loop_gain, nut_loss_fraction), 0.0, 0.99995);
  string->bridge_gain_target = wg_double_bass_clamp(
      pow(loop_gain, bridge_loss_fraction), 0.0, 0.99995);
  string->target_delay = tuning_frequency == frequency
      ? wg_double_bass_solve_delay(string, frequency, position, sample_rate)
      : wg_double_bass_solve_harmonic_delay(
            string, frequency, tuning_frequency, position, sample_rate);
  wg_double_bass_split_delay(
      string->target_delay, position,
      &string->target_bridge_delay, &string->target_nut_delay);
  {
    const double second_ratio = exp(
        0.0005776226504666211 * second->detune_cents);
    const double second_loop_gain = exp(
        -1.0 / (frequency * second->loss_time_constant_seconds));

    string->second_delay = wg_double_bass_clamp(
        string->target_delay / second_ratio, 2.0,
        (double)string->rail_size - 2.0);
    wg_double_bass_split_delay(
        string->second_delay, position,
        &string->second_bridge_delay, &string->second_nut_delay);
    string->second_nut_pole = string->nut_pole;
    string->second_bridge_pole = exp(
        -WG_DOUBLE_BASS_TWO_PI *
        fmin(second->bridge_cutoff_hz, 0.45 * sample_rate) / sample_rate);
    string->second_nut_gain_target = wg_double_bass_clamp(
        pow(second_loop_gain, nut_loss_fraction), 0.0, 0.99995);
    string->second_bridge_gain_target = wg_double_bass_clamp(
        pow(second_loop_gain, bridge_loss_fraction), 0.0, 0.99995);
  }
  if (!string->delay_initialized || !isfinite(string->delay)) {
    string->delay = string->target_delay;
    string->delay_velocity = 0.0;
    string->bridge_delay = string->target_bridge_delay;
    string->nut_delay = string->target_nut_delay;
    string->nut_gain = string->nut_gain_target;
    string->bridge_gain = string->bridge_gain_target;
    string->second_nut_gain = string->second_nut_gain_target;
    string->second_bridge_gain = string->second_bridge_gain_target;
    string->effective_frequency = frequency;
    string->bow_position = position;
    string->delay_initialized = 1;
  }
  wg_double_bass_refresh_release_gain_target(string);
}

static double wg_double_bass_friction_mu(
    const WG_DOUBLE_BASS_STRING_STATE *string, double speed)
{
  const WG_DOUBLE_BASS_MODEL *model = wg_double_bass_bound_model(string);
  const WG_DOUBLE_BASS_FRICTION_MODEL *friction = &model->bow.friction;
  const double thermal_drop = wg_double_bass_model_thermal_drop(model);
  const double thermal_scale = isfinite(string->contact_thermal_scale)
      ? wg_double_bass_clamp(
            string->contact_thermal_scale,
            1.0 - thermal_drop, 1.0)
      : 1.0;
  double mu;
  double scaled;
  uint32_t index;
  double fraction;

  speed = fabs(speed);
  if (!isfinite(speed)) {
    return thermal_scale * wg_double_bass_model_static_mu(model);
  }
  if (speed >= WG_DOUBLE_BASS_FRICTION_MAX_SPEED) {
    return thermal_scale * friction->floor_mu;
  }
  if (string->friction_lut == NULL) {
    mu = wg_double_bass_model_friction_mu(model, speed);
    return thermal_scale * mu;
  }
  scaled = speed * (double)(WG_DOUBLE_BASS_FRICTION_LUT_SIZE - 1U) /
      WG_DOUBLE_BASS_FRICTION_MAX_SPEED;
  index = (uint32_t)floor(scaled);
  fraction = scaled - (double)index;
  mu = (1.0 - fraction) * string->friction_lut[index] +
      fraction * string->friction_lut[index + 1U];
  return thermal_scale * mu;
}

static double wg_double_bass_friction_mu_slope(
    const WG_DOUBLE_BASS_STRING_STATE *string, double speed)
{
  const WG_DOUBLE_BASS_MODEL *model = wg_double_bass_bound_model(string);
  const WG_DOUBLE_BASS_FRICTION_MODEL *friction = &model->bow.friction;
  const double thermal_drop = wg_double_bass_model_thermal_drop(model);
  const double thermal_scale = isfinite(string->contact_thermal_scale)
      ? wg_double_bass_clamp(
            string->contact_thermal_scale,
            1.0 - thermal_drop, 1.0)
      : 1.0;
  double scaled;
  uint32_t index;

  speed = fabs(speed);
  if (!isfinite(speed) || speed >= WG_DOUBLE_BASS_FRICTION_MAX_SPEED) {
    return 0.0;
  }
  if (string->friction_lut == NULL) {
    return thermal_scale *
        (-friction->slow_gain / friction->slow_speed *
             exp(-speed / friction->slow_speed) -
         friction->fast_gain / friction->fast_speed *
             exp(-speed / friction->fast_speed));
  }
  scaled = speed * (double)(WG_DOUBLE_BASS_FRICTION_LUT_SIZE - 1U) /
      WG_DOUBLE_BASS_FRICTION_MAX_SPEED;
  index = (uint32_t)floor(scaled);
  return thermal_scale *
      (string->friction_lut[index + 1U] -
          string->friction_lut[index]) *
      (double)(WG_DOUBLE_BASS_FRICTION_LUT_SIZE - 1U) /
      WG_DOUBLE_BASS_FRICTION_MAX_SPEED;
}

static double wg_double_bass_release_tail_seconds(
    const WG_DOUBLE_BASS_STRING_STATE *string, uint32_t articulation,
    double speed)
{
  const WG_DOUBLE_BASS_RELEASE_T60_MODEL *release =
      &wg_double_bass_bound_model(string)->release_t60_seconds;
  /* Total time from note-off to -60 dB, including passive string loss. */
  switch (articulation) {
    case WG_DOUBLE_BASS_ARTICULATION_ARCO:
      return release->arco;
    case WG_DOUBLE_BASS_ARTICULATION_DETACHE:
      return release->detache;
    case WG_DOUBLE_BASS_ARTICULATION_MARTELE:
      return release->martele;
    case WG_DOUBLE_BASS_ARTICULATION_SPICCATO:
      return release->spiccato;
    case WG_DOUBLE_BASS_ARTICULATION_TREMOLO:
      return release->tremolo;
    case WG_DOUBLE_BASS_ARTICULATION_PIZZICATO:
      return speed < 0.0
          ? release->pizzicato_left : release->pizzicato_right;
    case WG_DOUBLE_BASS_ARTICULATION_BARTOK:
      return release->bartok;
    case WG_DOUBLE_BASS_ARTICULATION_BATTUTO:
      return release->battuto;
    case WG_DOUBLE_BASS_ARTICULATION_TRATTO:
      return release->tratto;
    default:
      return release->battuto;
  }
}

static double wg_double_bass_release_loop_gain(
    const WG_DOUBLE_BASS_STRING_STATE *string, uint32_t articulation,
    double speed)
{
  const double frequency = isfinite(string->target_frequency) &&
          string->target_frequency > 0.0
      ? string->target_frequency : string->open_frequency;

  /* Lifting an ordinary arco bow removes forcing; it does not mute the
     mechanical string. Deliberately damped releases remain separate gestures. */
  if (articulation == WG_DOUBLE_BASS_ARTICULATION_ARCO) {
    return 1.0;
  }
  const double tail_seconds = wg_double_bass_release_tail_seconds(
      string, articulation, speed);
  const double wanted_rate = -log(WG_DOUBLE_BASS_RELEASE_LEVEL) / tail_seconds;
  const double passive_rate = 1.0 /
      wg_double_bass_bound_string_model(string)->loss_time_constant_seconds;
  const double added_rate = fmax(0.0, wanted_rate - passive_rate);

  return wg_double_bass_clamp(exp(-added_rate / frequency), 0.05, 1.0);
}

static void wg_double_bass_refresh_release_gain_target(
    WG_DOUBLE_BASS_STRING_STATE *string)
{
  double target;

  if (string->release_gate_positive != 0 &&
      string->release_gate_positive != 1) {
    string->release_gate_positive = string->trigger > 1.0e-7 ? 1 : 0;
  }
  if (string->release_gate_positive) {
    return;
  }
  if (string->release_model_articulation >
      WG_DOUBLE_BASS_ARTICULATION_MAX) {
    string->release_model_articulation = WG_DOUBLE_BASS_ARTICULATION_ARCO;
  }
  if (!isfinite(string->release_model_speed)) {
    string->release_model_speed = 0.55;
  } else {
    string->release_model_speed = wg_double_bass_clamp(
        string->release_model_speed, -1.0, 1.0);
  }
  target = wg_double_bass_release_loop_gain(
      string, string->release_model_articulation,
      string->release_model_speed);
  if (!isfinite(target) || target < 0.05 || target > 1.0) {
    string->release_model_articulation = WG_DOUBLE_BASS_ARTICULATION_ARCO;
    string->release_model_speed = 0.55;
    target = wg_double_bass_release_loop_gain(
        string, WG_DOUBLE_BASS_ARTICULATION_ARCO, 0.55);
  }
  string->release_loop_gain_target = wg_double_bass_clamp(
      target, 0.05, 1.0);
}

static void wg_double_bass_begin_release(WG_DOUBLE_BASS_STRING_STATE *string)
{
  string->release_gate_positive = 0;
  wg_double_bass_refresh_release_gain_target(string);
  string->exciter_contact_target = 0.0;
}

static void wg_double_bass_begin_attack(WG_DOUBLE_BASS_STRING_STATE *string)
{
  string->release_loop_gain = 1.0;
  string->release_loop_gain_target = 1.0;
  string->release_model_articulation = string->articulation;
  string->release_model_speed = string->speed;
  string->release_gate_positive = 1;
  string->exciter_armed =
      string->articulation >= WG_DOUBLE_BASS_ARTICULATION_PIZZICATO &&
      string->articulation <= WG_DOUBLE_BASS_ARTICULATION_TRATTO;
  if (string->articulation < WG_DOUBLE_BASS_ARTICULATION_PIZZICATO) {
    string->trigger_armed = 1;
  }
}

static void wg_double_bass_release_bow(WG_DOUBLE_BASS_STRING_STATE *string)
{
  if (string->release_gate_positive || string->trigger > 1.0e-7) {
    wg_double_bass_begin_release(string);
  } else {
    string->release_gate_positive = 0;
  }
  string->trigger = 0.0;
  string->trigger_armed = 1;
  string->exciter_armed = 1;
  string->exciter_contact_target = 0.0;
  wg_double_bass_reset_gesture_live(string);
  wg_double_bass_reset_bow_live(string);
}

static uint32_t wg_double_bass_gesture_samples(double seconds,
                                           double sample_rate)
{
  const double samples = floor(seconds * sample_rate + 0.5);

  return (uint32_t)wg_double_bass_clamp(
      samples, 1.0, (double)UINT32_MAX);
}

static double wg_double_bass_smooth_step(double value)
{
  value = wg_double_bass_clamp(value, 0.0, 1.0);
  return value * value * (3.0 - 2.0 * value);
}

static int32_t wg_double_bass_gesture_direction(double speed)
{
  if (speed > WG_DOUBLE_BASS_GESTURE_SPEED_EPSILON) {
    return 1;
  }
  if (speed < -WG_DOUBLE_BASS_GESTURE_SPEED_EPSILON) {
    return -1;
  }
  return 0;
}

static void wg_double_bass_set_gesture_timing(
    WG_DOUBLE_BASS_STRING_STATE *string, double sample_rate)
{
  const WG_DOUBLE_BASS_GESTURE_MODEL *gesture =
      &wg_double_bass_bound_model(string)->gestures;
  const double speed = fabs(string->gesture_speed_target);

  switch (string->gesture_articulation) {
    case WG_DOUBLE_BASS_ARTICULATION_DETACHE:
      string->gesture_onset_samples = wg_double_bass_gesture_samples(
          gesture->detache.onset_seconds, sample_rate);
      string->gesture_stroke_samples = wg_double_bass_gesture_samples(
          gesture->detache.stroke_seconds, sample_rate);
      break;
    case WG_DOUBLE_BASS_ARTICULATION_MARTELE:
      string->gesture_onset_samples = wg_double_bass_gesture_samples(
          gesture->martele.preload_seconds, sample_rate);
      string->gesture_stroke_samples = wg_double_bass_gesture_samples(
          gesture->martele.stroke_seconds, sample_rate);
      break;
    case WG_DOUBLE_BASS_ARTICULATION_SPICCATO:
      string->gesture_stroke_samples = wg_double_bass_gesture_samples(
          gesture->spiccato.stroke_fast_seconds +
              gesture->spiccato.stroke_range_seconds * (1.0 - speed),
          sample_rate);
      string->gesture_onset_samples =
          string->gesture_stroke_samples / 2U;
      break;
    case WG_DOUBLE_BASS_ARTICULATION_TREMOLO:
      string->gesture_tremolo_rate = gesture->tremolo.rate_min_hz +
          (gesture->tremolo.rate_max_hz - gesture->tremolo.rate_min_hz) *
              speed;
      string->gesture_onset_samples = wg_double_bass_gesture_samples(
          gesture->tremolo.onset_seconds, sample_rate);
      string->gesture_stroke_samples = wg_double_bass_gesture_samples(
          0.5 / string->gesture_tremolo_rate, sample_rate);
      break;
    default:
      string->gesture_onset_samples = 0U;
      string->gesture_stroke_samples = 0U;
      break;
  }
}

static void wg_double_bass_start_gesture_transition(
    WG_DOUBLE_BASS_STRING_STATE *string)
{
  string->gesture_transition_force = string->gesture_force_output;
  string->gesture_transition_speed = string->gesture_speed_output;
  string->gesture_transition_contact = string->gesture_contact_output;
  string->gesture_transition_position = string->gesture_position_output;
  string->gesture_transition_mix = 0.0;
}

static void wg_double_bass_start_gesture(
    WG_DOUBLE_BASS_STRING_STATE *string, double sample_rate)
{
  string->gesture_active = 1;
  string->gesture_age = 0U;
  string->gesture_stroke_age = 0U;
  string->gesture_phase = 0.0;
  string->gesture_bow_change_active = 0;
  string->gesture_bow_change_mix = 1.0;
  if (string->gesture_articulation == WG_DOUBLE_BASS_ARTICULATION_TREMOLO) {
    string->gesture_tremolo_phase = 0.25;
  }
  wg_double_bass_set_gesture_timing(string, sample_rate);
  string->gesture_strokes = wg_double_bass_add_samples(
      string->gesture_strokes, 1U);
}

static void wg_double_bass_prepare_gesture(
    WG_DOUBLE_BASS_STRING_STATE *string, double sample_rate)
{
  const uint32_t articulation = string->articulation;
  const int32_t bowed = articulation <= WG_DOUBLE_BASS_ARTICULATION_TREMOLO;
  const int32_t active = bowed && string->trigger > 1.0e-7 &&
      string->force > 1.0e-7;
  const double force = wg_double_bass_clamp(string->force, 0.0, 1.0);
  const double speed = wg_double_bass_clamp(string->speed, -1.0, 1.0);
  const double position = wg_double_bass_clamp(string->position, 0.01, 0.49);
  const int32_t direction = wg_double_bass_gesture_direction(speed);
  const int32_t had_state = string->gesture_initialized;
  const uint32_t old_articulation = string->gesture_articulation;
  const int32_t mode_changed = had_state &&
      old_articulation != articulation;

  if (had_state && !mode_changed && string->gesture_active &&
      articulation >= WG_DOUBLE_BASS_ARTICULATION_DETACHE &&
      articulation <= WG_DOUBLE_BASS_ARTICULATION_TREMOLO &&
      (fabs(force - string->gesture_force_target) > 1.0e-7 ||
       fabs(speed - string->gesture_speed_target) > 1.0e-7 ||
       fabs(position - string->gesture_position_target) > 1.0e-7)) {
    string->gesture_direct_overrides = wg_double_bass_add_samples(
        string->gesture_direct_overrides, 1U);
  }
  if (mode_changed) {
    if (old_articulation <= WG_DOUBLE_BASS_ARTICULATION_TREMOLO && bowed) {
      wg_double_bass_start_gesture_transition(string);
      string->gesture_preset_changes = wg_double_bass_add_samples(
          string->gesture_preset_changes, 1U);
    } else {
      string->gesture_transition_mix = 1.0;
    }
    string->gesture_articulation = articulation;
    string->gesture_age = 0U;
    string->gesture_stroke_age = 0U;
    string->gesture_phase = 0.0;
    string->gesture_bow_change_active = 0;
    if (articulation == WG_DOUBLE_BASS_ARTICULATION_TREMOLO) {
      string->gesture_tremolo_phase = 0.25;
    }
  } else if (!had_state) {
    string->gesture_articulation = articulation;
    string->gesture_transition_mix = 1.0;
  }
  string->gesture_force_target = force;
  string->gesture_speed_target = speed;
  string->gesture_position_target = position;
  string->gesture_initialized = 1;
  if (active) {
    string->release_model_articulation = articulation;
    string->release_model_speed = speed;
  }
  if (mode_changed) {
    wg_double_bass_set_gesture_timing(string, sample_rate);
  }

  if (active && string->gesture_active && !mode_changed &&
      articulation != WG_DOUBLE_BASS_ARTICULATION_TREMOLO && direction != 0 &&
      string->gesture_command_direction != 0 &&
      direction != string->gesture_command_direction) {
    string->gesture_bow_change_start_speed =
        string->gesture_speed_output;
    string->gesture_bow_change_target_speed = speed;
    string->gesture_bow_change_mix = 0.0;
    string->gesture_bow_change_active = 1;
  }
  if (direction != 0) {
    string->gesture_command_direction = direction;
  }

  if (!active) {
    string->gesture_active = 0;
    string->gesture_contact_output = 0.0;
    string->trigger_armed = 1;
    return;
  }
  if (string->trigger_armed) {
    string->trigger_armed = 0;
    string->release_model_articulation = string->articulation;
    string->release_model_speed = string->speed;
    string->excitation_count = wg_double_bass_add_samples(
        string->excitation_count, 1U);
    wg_double_bass_start_gesture(string, sample_rate);
  } else {
    string->gesture_active = 1;
  }
}

static int32_t wg_double_bass_gesture_live_is_finite(
    const WG_DOUBLE_BASS_STRING_STATE *string)
{
  return isfinite(string->gesture_phase) &&
      isfinite(string->gesture_force_target) &&
      isfinite(string->gesture_speed_target) &&
      isfinite(string->gesture_position_target) &&
      isfinite(string->gesture_force_output) &&
      isfinite(string->gesture_speed_output) &&
      isfinite(string->gesture_contact_output) &&
      isfinite(string->gesture_position_output) &&
      isfinite(string->gesture_transition_force) &&
      isfinite(string->gesture_transition_speed) &&
      isfinite(string->gesture_transition_contact) &&
      isfinite(string->gesture_transition_position) &&
      isfinite(string->gesture_transition_mix) &&
      isfinite(string->gesture_transition_step) &&
      isfinite(string->gesture_bow_change_start_speed) &&
      isfinite(string->gesture_bow_change_target_speed) &&
      isfinite(string->gesture_bow_change_mix) &&
      isfinite(string->gesture_bow_change_step) &&
      isfinite(string->gesture_tremolo_phase) &&
      isfinite(string->gesture_tremolo_rate) &&
      string->gesture_articulation <= WG_DOUBLE_BASS_ARTICULATION_MAX &&
      string->gesture_phase >= 0.0 && string->gesture_phase <= 1.0 &&
      string->gesture_force_target >= 0.0 &&
      string->gesture_force_target <= 1.0 &&
      string->gesture_speed_target >= -1.0 &&
      string->gesture_speed_target <= 1.0 &&
      string->gesture_position_target >= 0.01 &&
      string->gesture_position_target <= 0.49 &&
      string->gesture_force_output >= 0.0 &&
      string->gesture_force_output <= 1.0 &&
      string->gesture_speed_output >= -1.0 &&
      string->gesture_speed_output <= 1.0 &&
      string->gesture_contact_output >= 0.0 &&
      string->gesture_contact_output <= 1.0 &&
      string->gesture_position_output >= 0.01 &&
      string->gesture_position_output <= 0.49 &&
      string->gesture_transition_force >= 0.0 &&
      string->gesture_transition_force <= 1.0 &&
      string->gesture_transition_speed >= -1.0 &&
      string->gesture_transition_speed <= 1.0 &&
      string->gesture_transition_contact >= 0.0 &&
      string->gesture_transition_contact <= 1.0 &&
      string->gesture_transition_position >= 0.01 &&
      string->gesture_transition_position <= 0.49 &&
      string->gesture_transition_mix >= 0.0 &&
      string->gesture_transition_mix <= 1.0 &&
      string->gesture_transition_step > 0.0 &&
      string->gesture_transition_step <= 1.0 &&
      string->gesture_bow_change_start_speed >= -1.0 &&
      string->gesture_bow_change_start_speed <= 1.0 &&
      string->gesture_bow_change_target_speed >= -1.0 &&
      string->gesture_bow_change_target_speed <= 1.0 &&
      string->gesture_bow_change_mix >= 0.0 &&
      string->gesture_bow_change_mix <= 1.0 &&
      string->gesture_bow_change_step > 0.0 &&
      string->gesture_bow_change_step <= 1.0 &&
      string->gesture_tremolo_phase >= 0.0 &&
      string->gesture_tremolo_phase < 1.0 &&
      string->gesture_tremolo_rate >= 0.0 &&
      string->gesture_tremolo_rate <= 20.0 &&
      (string->gesture_active == 0 || string->gesture_active == 1) &&
      (string->gesture_bow_change_active == 0 ||
       string->gesture_bow_change_active == 1) &&
      string->gesture_command_direction >= -1 &&
      string->gesture_command_direction <= 1;
}

static void wg_double_bass_gesture_tick(
    WG_DOUBLE_BASS_STRING_STATE *string, double sample_rate)
{
  const WG_DOUBLE_BASS_MODEL *model = wg_double_bass_bound_model(string);
  const WG_DOUBLE_BASS_GESTURE_MODEL *gesture = &model->gestures;
  const double gate = string->gesture_active ?
      wg_double_bass_clamp(string->trigger, 0.0, 1.0) : 0.0;
  const double raw_force = string->gesture_force_target;
  const double raw_speed = string->gesture_speed_target;
  const double raw_position = string->gesture_position_target;
  const uint32_t mode = string->gesture_articulation;
  double force = raw_force;
  double speed = raw_speed;
  double contact = gate;
  double position = raw_position;
  double phase;

  if (!wg_double_bass_gesture_live_is_finite(string) ||
      !(sample_rate > 0.0) || !isfinite(sample_rate)) {
    string->gesture_recoveries = wg_double_bass_add_samples(
        string->gesture_recoveries, 1U);
    wg_double_bass_reset_gesture_live(string);
    string->gesture_transition_step = 1.0 / fmax(
        1.0, gesture->transition_seconds * sample_rate);
    string->gesture_bow_change_step = 1.0 / fmax(
        1.0, gesture->bow_change_seconds * sample_rate);
    string->gesture_initialized = 1;
    string->gesture_active =
        string->articulation <= WG_DOUBLE_BASS_ARTICULATION_TREMOLO &&
        string->trigger > 1.0e-7 && string->force > 1.0e-7;
    wg_double_bass_set_gesture_timing(string, sample_rate);
    string->bow_speed_target = 0.0;
    string->bow_contact_target = 0.0;
    string->bow_position_target = wg_double_bass_clamp(
        string->position, 0.01, 0.49);
    return;
  }

  if (!string->gesture_active ||
      mode > WG_DOUBLE_BASS_ARTICULATION_TREMOLO) {
    contact = 0.0;
    phase = 0.0;
  } else if (mode == WG_DOUBLE_BASS_ARTICULATION_DETACHE) {
    const double onset = fmax(
        1.0, (double)string->gesture_onset_samples);
    const double stroke = fmax(
        onset + 1.0, (double)string->gesture_stroke_samples);
    const double attack = wg_double_bass_smooth_step(
        (double)string->gesture_age / onset);
    const double settle = wg_double_bass_smooth_step(
        ((double)string->gesture_age - onset) / (stroke - onset));
    const double contact_attack = wg_double_bass_smooth_step(
        gesture->detache.contact_attack_scale *
            (double)string->gesture_age / onset);

    force = raw_force * (gesture->detache.force_base +
        gesture->detache.force_attack * attack +
        gesture->detache.force_settle * settle);
    speed = raw_speed * (gesture->detache.speed_base +
        gesture->detache.speed_attack * attack +
        gesture->detache.speed_settle * settle);
    contact = gate * contact_attack;
    phase = wg_double_bass_clamp(
        (double)string->gesture_age / stroke, 0.0, 1.0);
  } else if (mode == WG_DOUBLE_BASS_ARTICULATION_MARTELE) {
    const double preload_samples = fmax(
        1.0, (double)string->gesture_onset_samples);
    const double acceleration_samples = fmax(
        1.0, gesture->martele.acceleration_seconds * sample_rate);
    const double stroke = fmax(
        preload_samples + acceleration_samples + 1.0,
        (double)string->gesture_stroke_samples);
    const double preload = wg_double_bass_smooth_step(
        (double)string->gesture_age / preload_samples);
    const double acceleration = wg_double_bass_smooth_step(
        ((double)string->gesture_age - preload_samples) /
        acceleration_samples);
    const double settle = wg_double_bass_smooth_step(
        ((double)string->gesture_age - preload_samples -
         acceleration_samples) /
        (stroke - preload_samples - acceleration_samples));

    force = raw_force * (gesture->martele.force_base +
        gesture->martele.force_preload * preload +
        gesture->martele.force_acceleration * acceleration +
        gesture->martele.force_settle * settle);
    speed = raw_speed * (
        gesture->martele.speed_preload * preload +
        gesture->martele.speed_acceleration * acceleration +
        gesture->martele.speed_settle * settle);
    contact = gate * wg_double_bass_smooth_step(
        gesture->martele.contact_preload_scale *
            (double)string->gesture_age / preload_samples);
    phase = wg_double_bass_clamp(
        (double)string->gesture_age / stroke, 0.0, 1.0);
  } else if (mode == WG_DOUBLE_BASS_ARTICULATION_SPICCATO) {
    const double stroke = fmax(
        1.0, (double)string->gesture_stroke_samples);
    const double stroke_phase = ((double)string->gesture_age + 0.5) /
        stroke;
    const double pulse = stroke_phase < 1.0 ?
        fmax(0.0, sin(0.5 * WG_DOUBLE_BASS_TWO_PI * stroke_phase)) : 0.0;
    const double bounce = sqrt(pulse);

    force = raw_force * (gesture->spiccato.force_base +
        gesture->spiccato.force_bounce * bounce);
    speed = raw_speed * (gesture->spiccato.speed_base +
        gesture->spiccato.speed_bounce * bounce);
    contact = gate * bounce;
    phase = wg_double_bass_clamp(stroke_phase, 0.0, 1.0);
  } else if (mode == WG_DOUBLE_BASS_ARTICULATION_TREMOLO) {
    const double magnitude = fabs(raw_speed);
    const double polarity = raw_speed < 0.0 ? -1.0 : 1.0;
    const double onset = fmax(
        1.0, (double)string->gesture_onset_samples);
    const double onset_mix = wg_double_bass_smooth_step(
        (double)string->gesture_age / onset);
    const double motion = sin(
        WG_DOUBLE_BASS_TWO_PI * string->gesture_tremolo_phase);
    const double motion_magnitude = fabs(motion);
    const uint32_t old_half =
        (uint32_t)floor(2.0 * string->gesture_tremolo_phase);
    uint32_t new_half;

    string->gesture_tremolo_rate = gesture->tremolo.rate_min_hz +
        (gesture->tremolo.rate_max_hz - gesture->tremolo.rate_min_hz) *
            magnitude;
    string->gesture_stroke_samples = wg_double_bass_gesture_samples(
        0.5 / string->gesture_tremolo_rate, sample_rate);
    force = raw_force * onset_mix * (gesture->tremolo.force_base +
        gesture->tremolo.force_motion * motion_magnitude);
    speed = polarity * magnitude * onset_mix * motion;
    contact = gate * onset_mix * (gesture->tremolo.contact_base +
        gesture->tremolo.contact_motion * motion_magnitude);
    phase = string->gesture_tremolo_phase;
    string->gesture_tremolo_phase +=
        string->gesture_tremolo_rate / sample_rate;
    string->gesture_tremolo_phase -= floor(
        string->gesture_tremolo_phase);
    new_half = (uint32_t)floor(
        2.0 * string->gesture_tremolo_phase);
    if (new_half != old_half) {
      string->gesture_strokes = wg_double_bass_add_samples(
          string->gesture_strokes, 1U);
      string->gesture_stroke_age = 0U;
    }
  } else {
    phase = 1.0;
  }

  if (string->gesture_transition_mix < 1.0) {
    const double mix = wg_double_bass_smooth_step(
        string->gesture_transition_mix);

    force = string->gesture_transition_force + mix *
        (force - string->gesture_transition_force);
    speed = string->gesture_transition_speed + mix *
        (speed - string->gesture_transition_speed);
    contact = string->gesture_transition_contact + mix *
        (contact - string->gesture_transition_contact);
    position = string->gesture_transition_position + mix *
        (position - string->gesture_transition_position);
    string->gesture_transition_mix = fmin(
        1.0, string->gesture_transition_mix +
        string->gesture_transition_step);
  }
  if (string->gesture_bow_change_active) {
    const double mix = wg_double_bass_smooth_step(
        string->gesture_bow_change_mix);
    const double unload = 1.0 - gesture->bow_change_contact_dip * sin(
        0.5 * WG_DOUBLE_BASS_TWO_PI * string->gesture_bow_change_mix);

    string->gesture_bow_change_target_speed = speed;
    speed = string->gesture_bow_change_start_speed + mix *
        (speed - string->gesture_bow_change_start_speed);
    contact *= unload;
    force *= gesture->bow_change_force_floor +
        (1.0 - gesture->bow_change_force_floor) * unload;
    string->gesture_bow_change_mix = fmin(
        1.0, string->gesture_bow_change_mix +
        string->gesture_bow_change_step);
    if (string->gesture_bow_change_mix >= 1.0) {
      string->gesture_bow_change_active = 0;
    }
  }

  string->gesture_force_output = wg_double_bass_clamp(force, 0.0, 1.0);
  string->gesture_speed_output = wg_double_bass_clamp(speed, -1.0, 1.0);
  string->gesture_contact_output = wg_double_bass_clamp(contact, 0.0, 1.0);
  string->gesture_position_output = wg_double_bass_clamp(
      position, 0.01, 0.49);
  string->gesture_phase = wg_double_bass_clamp(phase, 0.0, 1.0);
  string->bow_speed_target = model->bow.speed_scale *
      string->gesture_speed_output;
  string->bow_contact_target = string->gesture_contact_output;
  string->bow_position_target = string->gesture_position_output;
  if (string->gesture_active) {
    if (string->gesture_age < UINT32_MAX) {
      string->gesture_age++;
    }
    if (string->gesture_stroke_age < UINT32_MAX) {
      string->gesture_stroke_age++;
    }
  }
}

static double wg_double_bass_smooth(double value, double target,
                               double coefficient)
{
  return value + coefficient * (target - value);
}

static void wg_double_bass_prepare_finger(WG_DOUBLE_BASS_STRING_STATE *string,
                                     double frequency)
{
  const double target = string->harmonic != 0U && string->harmonic_natural
      ? 0.0
      : wg_double_bass_stop_position(string->open_frequency, frequency);
  const double pressure = target > WG_DOUBLE_BASS_FINGER_STOP_EPSILON ? 1.0 : 0.0;

  if (!string->finger_initialized) {
    string->finger_target_position = target;
    string->finger_position = target;
    string->finger_pressure_target = pressure;
    string->finger_pressure = pressure;
    string->finger_last_command_frequency = frequency;
    string->finger_initialized = 1;
    return;
  }
  if (fabs(target - string->finger_target_position) <=
      1.0e-12 * (1.0 + fabs(target))) {
    string->finger_last_command_frequency = frequency;
    return;
  }
  {
    const double old_target = string->finger_target_position;
    const int32_t was_stopped =
        old_target > WG_DOUBLE_BASS_FINGER_STOP_EPSILON;
    const int32_t is_stopped = target > WG_DOUBLE_BASS_FINGER_STOP_EPSILON;
    double noise = 0.0;

    if (!was_stopped && is_stopped) {
      string->finger_arrivals = wg_double_bass_add_samples(
          string->finger_arrivals, 1U);
      string->finger_motion_kind = WG_DOUBLE_BASS_FINGER_ARRIVING;
      noise = 0.00055 + 0.0014 * fmin(1.0, 4.0 * fabs(target - old_target));
    } else if (was_stopped && !is_stopped) {
      string->finger_releases = wg_double_bass_add_samples(
          string->finger_releases, 1U);
      string->finger_motion_kind = WG_DOUBLE_BASS_FINGER_RELEASING;
      noise = 0.00038 + 0.0009 * fmin(1.0, 4.0 * fabs(target - old_target));
    } else {
      if (string->finger_motion_kind == WG_DOUBLE_BASS_FINGER_STILL) {
        /* The event burst starts one continuous stopped-finger move.
           finger_tick supplies its continuing, ksmps-independent noise. */
        string->finger_shifts = wg_double_bass_add_samples(
            string->finger_shifts, 1U);
        noise = 0.00016 + 0.00048 * fmin(
            1.0, 8.0 * fabs(target - old_target));
      }
      string->finger_motion_kind = WG_DOUBLE_BASS_FINGER_SHIFTING;
    }
    if (string->trigger > 1.0e-7 || string->activity > 1.0e-9) {
      string->finger_noise_envelope = wg_double_bass_clamp(
          string->finger_noise_envelope + noise,
          0.0, WG_DOUBLE_BASS_FINGER_NOISE_LIMIT);
    }
  }
  string->finger_target_position = target;
  string->finger_pressure_target = pressure;
  string->finger_last_command_frequency = frequency;
}

static double wg_double_bass_finger_tick(WG_DOUBLE_BASS_STRING_STATE *string)
{
  const double old_position = string->finger_position;
  const double pressure_smoothing =
      string->finger_pressure_target > string->finger_pressure
          ? string->finger_pressure_attack_smoothing
          : string->finger_pressure_release_smoothing;
  const int32_t moving =
      fabs(string->finger_target_position - old_position) >
          WG_DOUBLE_BASS_FINGER_SETTLE_EPSILON;
  double movement_level;
  double result = 0.0;

  string->finger_position = wg_double_bass_smooth(
      string->finger_position, string->finger_target_position,
      string->finger_position_smoothing);
  string->finger_velocity = string->finger_position - old_position;
  string->finger_pressure = wg_double_bass_smooth(
      string->finger_pressure, string->finger_pressure_target,
      pressure_smoothing);
  if (moving) {
    string->finger_movement_samples = wg_double_bass_add_samples(
        string->finger_movement_samples, 1U);
  } else {
    string->finger_position = string->finger_target_position;
    string->finger_velocity = 0.0;
    string->finger_motion_kind = WG_DOUBLE_BASS_FINGER_STILL;
  }
  movement_level = fmin(
      0.00024, 0.055 * fabs(string->finger_velocity)) *
      fmax(0.15, string->finger_pressure);
  if (string->finger_noise_envelope > 1.0e-12 || movement_level > 1.0e-12) {
    const double white = wg_double_bass_signed_noise(&string->finger_rng);
    const double highpass = 0.93 * (
        string->finger_noise_highpass + white -
        string->finger_noise_previous_white);

    string->finger_noise_previous_white = white;
    string->finger_noise_highpass = highpass;
    result = wg_double_bass_clamp(
        (string->finger_noise_envelope + movement_level) * highpass,
        -WG_DOUBLE_BASS_FINGER_NOISE_LIMIT, WG_DOUBLE_BASS_FINGER_NOISE_LIMIT);
    string->finger_noise_envelope *= string->finger_noise_decay;
    if (string->finger_noise_envelope < 1.0e-12) {
      string->finger_noise_envelope = 0.0;
    }
    string->finger_noise_samples = wg_double_bass_add_samples(
        string->finger_noise_samples, 1U);
    string->finger_noise_energy += result * result;
    string->finger_noise_peak = fmax(
        string->finger_noise_peak, fabs(result));
  }
  string->finger_last_noise = result;
#if defined(HLOLLI_WG_DOUBLE_BASS_TEST_API)
  return string->diagnostic_finger_gain * result;
#else
  return result;
#endif
}

static double wg_double_bass_harmonic_projection(
    const WG_DOUBLE_BASS_STRING_STATE *string, double input,
    uint32_t order, double period)
{
  double projection = input;
  uint32_t node;

  if (order < WG_DOUBLE_BASS_HARMONIC_MIN_ORDER ||
      order > WG_DOUBLE_BASS_HARMONIC_MAX_ORDER) {
    return input;
  }
  for (node = 1U; node < order; node++) {
    projection += wg_double_bass_linear_delay_read(
        string->harmonic_history, string->rail_size,
        string->harmonic_write_index, period * (double)node);
  }
  return projection / (double)order;
}

static double wg_double_bass_harmonic_tick(WG_DOUBLE_BASS_STRING_STATE *string,
                                      double input,
                                      double sample_rate)
{
  uint32_t order = string->harmonic_filter_order;
  const uint32_t pending_order = string->harmonic_pending_order;
  double pressure_smoothing;
  double projection = input;
  double output = input;

  if (string->harmonic_history == NULL || string->rail_size < 8U ||
      !isfinite(input) || !(sample_rate > 0.0)) {
    return input;
  }
  pressure_smoothing = string->harmonic_touch_target >
          string->harmonic_touch_pressure
      ? string->harmonic_touch_attack_smoothing
      : string->harmonic_touch_release_smoothing;
  string->harmonic_touch_pressure = wg_double_bass_clamp(
      wg_double_bass_smooth(
          string->harmonic_touch_pressure,
          string->harmonic_touch_target,
          pressure_smoothing),
      0.0, 1.0);
  if (order >= WG_DOUBLE_BASS_HARMONIC_MIN_ORDER &&
      order <= WG_DOUBLE_BASS_HARMONIC_MAX_ORDER &&
      string->target_frequency > 0.0) {
    const double loop_period = string->delay +
        sample_rate / string->target_frequency - string->target_delay;
    const double maximum_period = ((double)string->rail_size - 2.0) /
        (double)(order - 1U);
    const double period = wg_double_bass_clamp(
        loop_period / (double)order,
        WG_DOUBLE_BASS_MIN_BRANCH_DELAY, maximum_period);

    projection = wg_double_bass_harmonic_projection(
        string, input, order, period);
    if (pending_order >= WG_DOUBLE_BASS_HARMONIC_MIN_ORDER &&
        pending_order <= WG_DOUBLE_BASS_HARMONIC_MAX_ORDER &&
        pending_order != order) {
      const double pending_maximum =
          ((double)string->rail_size - 2.0) /
          (double)(pending_order - 1U);
      const double pending_period = wg_double_bass_clamp(
          loop_period / (double)pending_order,
          WG_DOUBLE_BASS_MIN_BRANCH_DELAY, pending_maximum);
      const double pending_projection = wg_double_bass_harmonic_projection(
          string, input, pending_order, pending_period);

      string->harmonic_order_mix = fmin(
          1.0, string->harmonic_order_mix + string->harmonic_order_mix_step);
      projection += string->harmonic_order_mix *
          (pending_projection - projection);
      string->harmonic_touch_period = pending_period;
      if (string->harmonic_order_mix >= 1.0) {
        string->harmonic_filter_order = pending_order;
        string->harmonic_order_mix = 0.0;
      }
    } else {
      string->harmonic_order_mix = 0.0;
      string->harmonic_touch_period = period;
    }
    output = input + string->harmonic_touch_pressure * (projection - input);
  } else {
    string->harmonic_touch_period = 0.0;
    string->harmonic_order_mix = 0.0;
  }
  if (pending_order == 0U &&
      string->harmonic_touch_pressure <= 1.0e-4) {
    string->harmonic_filter_order = 0U;
  }
  if (!isfinite(projection) || !isfinite(output) ||
      !isfinite(string->harmonic_touch_pressure) ||
      !isfinite(string->harmonic_order_mix)) {
    wg_double_bass_zero_waveguide(string, 1);
    return 0.0;
  }
  string->harmonic_history[string->harmonic_write_index] = input;
  string->harmonic_write_index++;
  if (string->harmonic_write_index >= string->rail_size) {
    string->harmonic_write_index = 0U;
  }
  string->harmonic_projection_input = input;
  string->harmonic_projection_output = output;
  if (string->harmonic_touch_pressure > 1.0e-7) {
    string->harmonic_touch_samples = wg_double_bass_add_samples(
        string->harmonic_touch_samples, 1U);
  }
  return wg_double_bass_clamp(
      output, -WG_DOUBLE_BASS_WAVE_LIMIT, WG_DOUBLE_BASS_WAVE_LIMIT);
}

static uint32_t wg_double_bass_exciter_duration(double seconds,
                                           double sample_rate,
                                           uint32_t minimum)
{
  const double maximum = fmax(
      (double)minimum,
      floor(WG_DOUBLE_BASS_EXCITER_MAX_PULSE_SECONDS * sample_rate + 0.5));
  const double samples = floor(seconds * sample_rate + 0.5);

  return (uint32_t)wg_double_bass_clamp(
      samples, (double)minimum, maximum);
}

static double wg_double_bass_exciter_pulse(uint32_t age, uint32_t length)
{
  const double phase = length > 0U ?
      ((double)age + 0.5) / (double)length : 1.0;

  if (length == 0U || age >= length) {
    return 0.0;
  }
  return 0.5 - 0.5 * cos(WG_DOUBLE_BASS_TWO_PI * phase);
}

static double wg_double_bass_pizzicato_release(uint32_t age, uint32_t length)
{
  const double phase = length > 0U ?
      ((double)age + 0.5) / (double)length : 1.0;

  if (length == 0U || age >= length) {
    return 0.0;
  }
  return 0.5 + 0.5 * cos(0.5 * WG_DOUBLE_BASS_TWO_PI * phase);
}

static double wg_double_bass_exciter_noise(WG_DOUBLE_BASS_STRING_STATE *string)
{
  const WG_DOUBLE_BASS_EXCITER_MODEL *model =
      &wg_double_bass_bound_model(string)->exciters;
  const double white = wg_double_bass_signed_noise(&string->exciter_rng);
  const double highpass = model->noise_highpass * (
      string->exciter_noise_highpass + white -
      string->exciter_noise_previous_white);

  string->exciter_noise_previous_white = white;
  string->exciter_noise_highpass = highpass;
  return highpass;
}

static int32_t wg_double_bass_exciter_live_is_finite(
    const WG_DOUBLE_BASS_STRING_STATE *string)
{
  return isfinite(string->exciter_position) &&
      isfinite(string->exciter_amplitude) &&
      isfinite(string->exciter_noise_level) &&
      isfinite(string->exciter_impact_amplitude) &&
      isfinite(string->exciter_contact_target) &&
      isfinite(string->exciter_contact) &&
      isfinite(string->exciter_speed_target) &&
      isfinite(string->exciter_speed) &&
      isfinite(string->exciter_noise_highpass) &&
      isfinite(string->exciter_noise_previous_white) &&
      isfinite(string->exciter_last_output) &&
      isfinite(string->collision_compression) &&
      isfinite(string->collision_stick_speed) &&
      isfinite(string->collision_stiffness) &&
      isfinite(string->collision_damping) &&
      isfinite(string->collision_mass) &&
      isfinite(string->collision_force);
}

static int32_t wg_double_bass_add_pizzicato_history(
    double *rail, uint32_t size, uint32_t write_index,
    double delay, double value)
{
  const uint32_t wanted = (uint32_t)wg_double_bass_clamp(
      floor(delay) + 3.0, 1.0, (double)size);
  int32_t clipped = 0;
  uint32_t offset;

  for (offset = 0U; offset < wanted; offset++) {
    const uint32_t index = (uint32_t)(
        ((uint64_t)write_index + (uint64_t)size - (uint64_t)offset) %
        (uint64_t)size);
    const double sum = rail[index] + value;

    if (fabs(sum) > WG_DOUBLE_BASS_WAVE_LIMIT) {
      clipped = 1;
    }
    rail[index] = wg_double_bass_clamp(
        sum, -WG_DOUBLE_BASS_WAVE_LIMIT, WG_DOUBLE_BASS_WAVE_LIMIT);
  }
  return clipped;
}

static void wg_double_bass_initialize_pizzicato_displacement(
    WG_DOUBLE_BASS_STRING_STATE *string, double holding_increment)
{
  const double position = wg_double_bass_clamp(
      string->exciter_position, 0.01, 0.49);
  const double bridge_level = holding_increment * (1.0 - position);
  const double nut_level = holding_increment * position;
  int32_t clipped;

  /*
   * Mansour, Woodhouse, and Scavone (2016), section 2.4: a pluck
   * starts with nonzero displacement and zero velocity. In velocity-wave
   * coordinates the held triangular displacement is a constant history in
   * each round-trip branch. Their levels sum to F / (2 Z0); the branch
   * weights follow the two slopes of the triangle. The short raised-cosine
   * force release in wg_double_bass_exciter_tick rounds the corner and
   * band-limits this initial condition. Adding the histories makes retriggers
   * linear and independent; it does not leave a persistent force in the live
   * model.
   */
  clipped = wg_double_bass_add_pizzicato_history(
      string->toward_bridge, string->rail_size,
      string->bridge_write_index, string->bridge_delay, bridge_level);
  clipped |= wg_double_bass_add_pizzicato_history(
      string->toward_nut, string->rail_size,
      string->nut_write_index, string->nut_delay, nut_level);

  /* Add the matching DC state so the endpoint filters do not turn
     the initial displacement into an unrelated filter-start transient. */
  string->bridge_state = wg_double_bass_clamp(
      string->bridge_state + bridge_level,
      -WG_DOUBLE_BASS_WAVE_LIMIT, WG_DOUBLE_BASS_WAVE_LIMIT);
  string->nut_state = wg_double_bass_clamp(
      string->nut_state + nut_level,
      -WG_DOUBLE_BASS_WAVE_LIMIT, WG_DOUBLE_BASS_WAVE_LIMIT);
  if (clipped) {
    string->wave_clips = wg_double_bass_add_samples(
        string->wave_clips, 1U);
  }
}

static void wg_double_bass_start_exciter(WG_DOUBLE_BASS_STRING_STATE *string,
                                    double sample_rate)
{
  const WG_DOUBLE_BASS_EXCITER_MODEL *exciters =
      &wg_double_bass_bound_model(string)->exciters;
  const uint32_t mode = string->articulation;
  const double trigger = wg_double_bass_clamp(string->trigger, 0.0, 1.0);
  const double force = wg_double_bass_clamp(string->force, 0.0, 1.0);
  const double speed = wg_double_bass_clamp(string->speed, -1.0, 1.0);
  const double speed_magnitude = fabs(speed);
  const double polarity = speed < 0.0 ? -1.0 : 1.0;
  const double impact_strength = trigger * sqrt(force) * speed_magnitude;
  const double frequency = isfinite(string->target_frequency) &&
          string->target_frequency > 0.0
      ? string->target_frequency : string->open_frequency;
  const double notch_delay = sample_rate *
      wg_double_bass_clamp(string->position, 0.01, 0.49) / frequency;
  double pulse_seconds = 0.0;

  string->release_model_articulation = mode;
  string->release_model_speed = speed;
  string->exciter_mode = mode;
  string->exciter_position = wg_double_bass_clamp(
      string->position, 0.01, 0.49);
  string->exciter_age = 0U;
  string->exciter_total_samples = 0U;
  string->exciter_pulse_samples = 0U;
  string->exciter_impact_delay = 0U;
  string->exciter_impact_samples = 0U;
  string->collision_sample_limit = 0U;
  string->exciter_amplitude = 0.0;
  string->exciter_noise_level = 0.0;
  string->exciter_impact_amplitude = 0.0;
  string->collision_compression = 0.0;
  string->collision_stick_speed = 0.0;
  string->collision_stiffness = 0.0;
  string->collision_damping = 0.0;
  string->collision_mass = 0.0;
  string->collision_force = 0.0;
  string->exciter_impact_counted = 0;
  string->exciter_notch_delay = (uint32_t)wg_double_bass_clamp(
      floor(notch_delay + 0.5), 1.0,
      fmax(1.0, (double)string->rail_size - 2.0));

  if (mode == WG_DOUBLE_BASS_ARTICULATION_PIZZICATO) {
    const int32_t left_hand = speed < 0.0;
    const WG_DOUBLE_BASS_PIZZICATO_MODEL *pizzicato = left_hand
        ? &exciters->pizzicato_left : &exciters->pizzicato_right;

    pulse_seconds = pizzicato->pulse_min_seconds +
        pizzicato->pulse_range_seconds * (1.0 - speed_magnitude);
    string->exciter_pulse_samples = wg_double_bass_exciter_duration(
        pulse_seconds, sample_rate, WG_DOUBLE_BASS_EXCITER_MIN_PULSE_SAMPLES);
    string->exciter_total_samples = string->exciter_pulse_samples;
    string->exciter_amplitude = polarity * pizzicato->amplitude * force /
        (2.0 * string->characteristic_impedance);
    string->exciter_noise_level = pizzicato->noise_gain *
        fabs(string->exciter_amplitude) * speed_magnitude;
    wg_double_bass_initialize_pizzicato_displacement(
        string, string->exciter_amplitude);
    if (left_hand) {
      string->left_hand_pizzicato_attacks = wg_double_bass_add_samples(
          string->left_hand_pizzicato_attacks, 1U);
    } else {
      string->normal_pizzicato_attacks = wg_double_bass_add_samples(
          string->normal_pizzicato_attacks, 1U);
    }
  } else if (mode == WG_DOUBLE_BASS_ARTICULATION_BARTOK) {
    const WG_DOUBLE_BASS_BARTOK_MODEL *bartok = &exciters->bartok;

    pulse_seconds = bartok->pulse_min_seconds +
        bartok->pulse_range_seconds * (1.0 - speed_magnitude);
    string->exciter_pulse_samples = wg_double_bass_exciter_duration(
        pulse_seconds, sample_rate, WG_DOUBLE_BASS_EXCITER_MIN_PULSE_SAMPLES);
    string->exciter_impact_delay = wg_double_bass_exciter_duration(
        bartok->impact_delay_min_seconds +
            bartok->impact_delay_range_seconds * (1.0 - force),
        sample_rate, 1U);
    string->exciter_impact_samples = wg_double_bass_exciter_duration(
        bartok->impact_min_seconds +
            bartok->impact_range_seconds * (1.0 - speed_magnitude),
        sample_rate,
        WG_DOUBLE_BASS_EXCITER_MIN_PULSE_SAMPLES);
    string->exciter_total_samples = string->exciter_impact_delay +
        string->exciter_impact_samples;
    if (string->exciter_total_samples < string->exciter_pulse_samples) {
      string->exciter_total_samples = string->exciter_pulse_samples;
    }
    string->exciter_amplitude =
        polarity * bartok->amplitude * impact_strength;
    string->exciter_noise_level = bartok->noise_gain * impact_strength;
    string->exciter_impact_amplitude =
        bartok->impact_gain * impact_strength;
    string->bartok_pizzicato_attacks = wg_double_bass_add_samples(
        string->bartok_pizzicato_attacks, 1U);
  } else if (mode == WG_DOUBLE_BASS_ARTICULATION_BATTUTO) {
    const WG_DOUBLE_BASS_BATTUTO_MODEL *battuto = &exciters->battuto;

    string->collision_sample_limit = (uint32_t)wg_double_bass_clamp(
        floor(battuto->collision_seconds * sample_rate + 0.5),
        (double)WG_DOUBLE_BASS_EXCITER_MIN_PULSE_SAMPLES,
        fmax((double)WG_DOUBLE_BASS_EXCITER_MIN_PULSE_SAMPLES,
             WG_DOUBLE_BASS_EXCITER_MAX_PULSE_SECONDS * sample_rate));
    string->exciter_total_samples = string->collision_sample_limit;
    string->exciter_amplitude = polarity;
    string->exciter_noise_level = battuto->noise_gain *
        sqrt(force) * speed_magnitude;
    string->collision_stick_speed =
        sqrt(force) * speed_magnitude *
            (battuto->speed_base + battuto->speed_force * force);
    string->collision_stiffness = battuto->stiffness_base +
        battuto->stiffness_force * force;
    string->collision_damping = battuto->damping_base +
        battuto->damping_force * force;
    string->collision_mass = battuto->mass_kg;
    string->wood_strikes = wg_double_bass_add_samples(
        string->wood_strikes, 1U);
  } else if (mode == WG_DOUBLE_BASS_ARTICULATION_TRATTO) {
    string->wood_tratto_attacks = wg_double_bass_add_samples(
        string->wood_tratto_attacks, 1U);
  }
  string->excitation_count = wg_double_bass_add_samples(
      string->excitation_count, 1U);
}

static void wg_double_bass_prepare_exciter(WG_DOUBLE_BASS_STRING_STATE *string,
                                      double sample_rate)
{
  const WG_DOUBLE_BASS_TRATTO_MODEL *tratto =
      &wg_double_bass_bound_model(string)->exciters.tratto;
  const uint32_t mode = string->articulation;
  const int32_t valid_mode =
      mode >= WG_DOUBLE_BASS_ARTICULATION_PIZZICATO &&
      mode <= WG_DOUBLE_BASS_ARTICULATION_TRATTO;
  const int32_t active = valid_mode && string->trigger > 1.0e-7;

  string->exciter_contact_target =
      active && mode == WG_DOUBLE_BASS_ARTICULATION_TRATTO
          ? wg_double_bass_clamp(string->trigger, 0.0, 1.0)
          : 0.0;
  string->exciter_speed_target =
      active && mode == WG_DOUBLE_BASS_ARTICULATION_TRATTO
          ? tratto->speed_scale *
              wg_double_bass_clamp(string->speed, -1.0, 1.0)
          : 0.0;
  if (!active) {
    if (string->trigger <= 1.0e-7) {
      string->exciter_armed = 1;
    }
    string->exciter_gate_mode = mode;
    return;
  }
  if (string->exciter_armed) {
    wg_double_bass_reset_bow_live(string);
    wg_double_bass_start_exciter(string, sample_rate);
    string->exciter_armed = 0;
  }
  string->exciter_gate_mode = mode;
}

static double wg_double_bass_exciter_tick(WG_DOUBLE_BASS_STRING_STATE *string,
                                     double free_velocity,
                                     int32_t controls_enabled,
                                     double sample_rate)
{
  const WG_DOUBLE_BASS_EXCITER_MODEL *exciters =
      &wg_double_bass_bound_model(string)->exciters;
  const int32_t one_shot_active =
      string->exciter_age < string->exciter_total_samples;
  const double contact_target =
      controls_enabled &&
              string->articulation == WG_DOUBLE_BASS_ARTICULATION_TRATTO
          ? string->exciter_contact_target
          : 0.0;
  const double speed_target =
      controls_enabled &&
              string->articulation == WG_DOUBLE_BASS_ARTICULATION_TRATTO
          ? string->exciter_speed_target
          : 0.0;
  double result = 0.0;

  if (!wg_double_bass_exciter_live_is_finite(string) ||
      !isfinite(free_velocity) || !(sample_rate > 0.0)) {
    string->exciter_recoveries = wg_double_bass_add_samples(
        string->exciter_recoveries, 1U);
    if (!isfinite(string->exciter_energy)) {
      string->exciter_energy = 0.0;
    }
    if (!isfinite(string->exciter_peak)) {
      string->exciter_peak = 0.0;
    }
    wg_double_bass_reset_exciter_live(string);
    string->exciter_armed = string->trigger <= 1.0e-7;
    return 0.0;
  }

  string->exciter_contact = wg_double_bass_smooth(
      string->exciter_contact, contact_target,
      contact_target > string->exciter_contact
          ? string->exciter_contact_attack_smoothing
          : string->exciter_contact_release_smoothing);
  string->exciter_speed = wg_double_bass_smooth(
      string->exciter_speed, speed_target,
      string->exciter_speed_smoothing);

  if (one_shot_active &&
      string->exciter_mode == WG_DOUBLE_BASS_ARTICULATION_PIZZICATO) {
    const double release = wg_double_bass_pizzicato_release(
        string->exciter_age, string->exciter_pulse_samples);
    const double pulse = wg_double_bass_exciter_pulse(
        string->exciter_age, string->exciter_pulse_samples);

    result += string->exciter_amplitude * release;
    if (pulse > 0.0 && string->exciter_noise_level > 0.0) {
      result += string->exciter_noise_level * pulse *
          wg_double_bass_exciter_noise(string);
    }
    string->exciter_age++;
  } else if (one_shot_active &&
             string->exciter_mode == WG_DOUBLE_BASS_ARTICULATION_BARTOK) {
    const double pulse = wg_double_bass_exciter_pulse(
        string->exciter_age, string->exciter_pulse_samples);

    result += string->exciter_amplitude * pulse;
    if (pulse > 0.0 && string->exciter_noise_level > 0.0) {
      result += string->exciter_noise_level * pulse *
          wg_double_bass_exciter_noise(string);
    }
    if (string->exciter_age >= string->exciter_impact_delay &&
        string->exciter_age < string->exciter_impact_delay +
            string->exciter_impact_samples) {
      const uint32_t impact_age =
          string->exciter_age - string->exciter_impact_delay;
      const double impact_pulse = wg_double_bass_exciter_pulse(
          impact_age, string->exciter_impact_samples);
      const double alternating = (impact_age & 1U) != 0U ? -1.0 : 1.0;
      const double grain = wg_double_bass_exciter_noise(string);
      const double alternating_mix =
          exciters->bartok.impact_alternating_mix;

      if (!string->exciter_impact_counted &&
          string->exciter_impact_amplitude > 0.0) {
        string->fingerboard_impacts = wg_double_bass_add_samples(
            string->fingerboard_impacts, 1U);
        string->exciter_impact_counted = 1;
      }
      result += string->exciter_impact_amplitude * impact_pulse *
          (alternating_mix * alternating +
           (1.0 - alternating_mix) * grain);
    }
    string->exciter_age++;
  } else if (one_shot_active &&
             string->exciter_mode == WG_DOUBLE_BASS_ARTICULATION_BATTUTO) {
    const double polarity = string->exciter_amplitude < 0.0 ? -1.0 : 1.0;
    const double relative =
        string->collision_stick_speed - polarity * free_velocity;

    string->collision_compression += relative / sample_rate;
    if (string->collision_compression > 0.0 &&
        string->collision_mass > 0.0) {
      const double elastic = string->collision_stiffness * pow(
          string->collision_compression, 1.5);
      const double damping = 1.0 + string->collision_damping *
          fmax(relative, 0.0);
      const double maximum_force = 2.0 *
          string->characteristic_impedance * 0.75;

      string->collision_force = wg_double_bass_clamp(
          elastic * damping, 0.0, maximum_force);
      string->collision_stick_speed -= string->collision_force /
          (string->collision_mass * sample_rate);
      result += polarity * string->collision_force /
          (2.0 * string->characteristic_impedance);
      if (string->collision_force > 0.0) {
        result += string->exciter_noise_level * fabs(result) *
            wg_double_bass_exciter_noise(string);
        string->collision_samples = wg_double_bass_add_samples(
            string->collision_samples, 1U);
      }
    } else {
      string->collision_compression = 0.0;
      string->collision_force = 0.0;
      string->exciter_age = string->exciter_total_samples;
    }
    if (string->exciter_age < string->exciter_total_samples) {
      string->exciter_age++;
    }
    if (string->exciter_age >= string->collision_sample_limit) {
      string->exciter_age = string->exciter_total_samples;
      string->collision_compression = 0.0;
      string->collision_force = 0.0;
    }
  }

  if (controls_enabled &&
      string->articulation == WG_DOUBLE_BASS_ARTICULATION_TRATTO &&
      contact_target > 1.0e-7) {
    const WG_DOUBLE_BASS_TRATTO_MODEL *tratto = &exciters->tratto;
    const double force_control = wg_double_bass_clamp(
        string->force, 0.0, 1.0);
    const double relative = string->exciter_speed - free_velocity;
    const double transition = tratto->transition_min +
        tratto->transition_range * (1.0 - force_control);
    const double maximum_force =
        tratto->force_gain * force_control * string->exciter_contact;
    const double friction_force = maximum_force * tanh(
        relative / transition);
    double increment = friction_force /
        (2.0 * string->characteristic_impedance);

    if (force_control > 0.0 && fabs(string->exciter_speed) > 1.0e-7) {
      const double grain_level = tratto->grain_gain * string->exciter_contact *
          sqrt(force_control) * fabs(string->exciter_speed);
      increment += grain_level * wg_double_bass_exciter_noise(string);
    }
    result += increment;
    string->wood_tratto_samples = wg_double_bass_add_samples(
        string->wood_tratto_samples, 1U);
  }

  result = wg_double_bass_clamp(
      result, -WG_DOUBLE_BASS_EXCITER_LIMIT, WG_DOUBLE_BASS_EXCITER_LIMIT);
  if (!isfinite(result)) {
    string->exciter_recoveries = wg_double_bass_add_samples(
        string->exciter_recoveries, 1U);
    wg_double_bass_reset_exciter_live(string);
    string->exciter_armed = string->trigger <= 1.0e-7;
    return 0.0;
  }
  string->exciter_last_output = result;
  if (fabs(result) > 1.0e-12) {
    string->exciter_samples = wg_double_bass_add_samples(
        string->exciter_samples, 1U);
    string->exciter_energy += result * result;
    string->exciter_peak = fmax(string->exciter_peak, fabs(result));
  }
  return result;
}

static double wg_double_bass_body_brightness(double frequency)
{
  const double low = 265.0;
  const double high = 4700.0;
  const double position = log(fmax(low, frequency) / low) / log(high / low);
  return -0.35 + 1.35 * wg_double_bass_clamp(position, 0.0, 1.0);
}

static void wg_double_bass_initialize_body(WG_DOUBLE_BASS_STATE *state)
{
  const WG_DOUBLE_BASS_MODEL *model = state->model != NULL
      ? state->model : wg_double_bass_fixed_model();
  uint32_t mode_index;

  for (mode_index = 0U; mode_index < WG_DOUBLE_BASS_BODY_MODES; mode_index++) {
    const WG_DOUBLE_BASS_BODY_MODE_SPEC *spec =
        &model->body.modes[mode_index];
    WG_DOUBLE_BASS_BODY_MODE_STATE *mode = &state->body_modes[mode_index];
    const double angle =
        WG_DOUBLE_BASS_TWO_PI * spec->frequency / state->sample_rate;
    const double brightness = wg_double_bass_body_brightness(spec->frequency);
    const double spectral_position =
        2.0 * (brightness + 0.35) / 1.35 - 1.0;
    uint32_t string_index;

    mode->brightness = brightness;
    mode->active = spec->frequency < 0.45 * state->sample_rate;
    mode->cosine = cos(angle);
    mode->sine = sin(angle);
    mode->left_gain = sqrt(0.5 * (1.0 - spec->pan));
    mode->right_gain = sqrt(0.5 * (1.0 + spec->pan));
    for (string_index = 0U; string_index < WG_DOUBLE_BASS_STRINGS;
         string_index++) {
      const double height = ((double)string_index - 1.5) / 1.5;
      mode->source_gain[string_index] =
          1.0 + 0.10 * height * spectral_position;
    }
  }
}

static void wg_double_bass_prepare_body(WG_DOUBLE_BASS_STATE *state)
{
  const WG_DOUBLE_BASS_MODEL *model = state->model != NULL
      ? state->model : wg_double_bass_fixed_model();
  const WG_DOUBLE_BASS_BODY_MODEL *body = &model->body;
  const double sample_rate = state->sample_rate;
  const double decay_scale =
      (body->body_decay_base - body->body_decay_range * state->body) *
      (1.0 + body->mute_decay_scale * state->mute);
  uint32_t mode_index;

  state->body_wet_level_target = body->wet_gain * state->body *
      (1.0 - body->mute_level_attenuation * state->mute);
  for (mode_index = 0U; mode_index < WG_DOUBLE_BASS_BODY_MODES; mode_index++) {
    const WG_DOUBLE_BASS_BODY_MODE_SPEC *spec =
        &body->modes[mode_index];
    WG_DOUBLE_BASS_BODY_MODE_STATE *mode = &state->body_modes[mode_index];
    const double brightness = mode->brightness;
    const double tone = 1.0 + body->body_tone_depth *
        (2.0 * state->body - 1.0) * brightness;
    const double mute_tone = 1.0 - state->mute *
        (body->mute_low_attenuation +
         body->mute_high_attenuation * fmax(brightness, 0.0));
    mode->effective_bandwidth = spec->bandwidth * decay_scale;
    if (mode->active) {
      mode->radius = wg_double_bass_clamp(
          exp(-0.5 * WG_DOUBLE_BASS_TWO_PI * mode->effective_bandwidth /
              sample_rate),
          0.0, 1.0 - 1.0e-9);
      mode->target_gain = spec->gain * fmax(0.05, tone * mute_tone);
    } else {
      mode->radius = 0.0;
      mode->target_gain = 0.0;
    }
  }
}

static void wg_double_bass_clear_body_live(WG_DOUBLE_BASS_STATE *state,
                                      int32_t count_reset)
{
  uint32_t mode_index;

  for (mode_index = 0U; mode_index < WG_DOUBLE_BASS_BODY_MODES; mode_index++) {
    state->body_modes[mode_index].real = 0.0;
    state->body_modes[mode_index].imaginary = 0.0;
  }
  state->body_last_drive = 0.0;
  state->bridge_radiation_lowpass = 0.0;
  state->body_last_left = 0.0;
  state->body_last_right = 0.0;
  state->body_limit_streak = 0U;
  if (count_reset) {
    state->body_resets = wg_double_bass_add_samples(state->body_resets, 1U);
    state->body_recovery_remaining = WG_DOUBLE_BASS_BOW_RECOVERY_SAMPLES;
  }
}

static void wg_double_bass_body_current_output(const WG_DOUBLE_BASS_STATE *state,
                                          double *left, double *right)
{
  const double wet_level = state->body > 0.0 ?
      state->body_wet_level : 0.0;
  double sum_left = 0.0;
  double sum_right = 0.0;
  uint32_t mode_index;

  for (mode_index = 0U; mode_index < WG_DOUBLE_BASS_BODY_MODES; mode_index++) {
    const WG_DOUBLE_BASS_BODY_MODE_STATE *mode = &state->body_modes[mode_index];
    const double output = 2.0 * mode->gain * mode->imaginary;
    sum_left += mode->left_gain * output;
    sum_right += mode->right_gain * output;
  }
  *left = wet_level * sum_left;
  *right = wet_level * sum_right;
}

static int32_t wg_double_bass_body_state_is_finite(
    const WG_DOUBLE_BASS_STATE *state)
{
  uint32_t mode_index;

  if (!isfinite(state->body_wet_level) ||
      !isfinite(state->body_wet_level_target) ||
      !isfinite(state->body_last_drive) ||
      !isfinite(state->bridge_radiation_lowpass) ||
      fabs(state->bridge_radiation_lowpass) > WG_DOUBLE_BASS_BODY_DRIVE_LIMIT ||
      !isfinite(state->body_last_left) ||
      !isfinite(state->body_last_right)) {
    return 0;
  }
  for (mode_index = 0U; mode_index < WG_DOUBLE_BASS_BODY_MODES; mode_index++) {
    const WG_DOUBLE_BASS_BODY_MODE_STATE *mode = &state->body_modes[mode_index];
    uint32_t string_index;

    if (!isfinite(mode->real) || !isfinite(mode->imaginary) ||
        !isfinite(mode->radius) || !isfinite(mode->cosine) ||
        !isfinite(mode->sine) || !isfinite(mode->gain) ||
        !isfinite(mode->target_gain) || !isfinite(mode->left_gain) ||
        !isfinite(mode->right_gain) || !isfinite(mode->brightness) ||
        mode->radius < 0.0 ||
        mode->radius >= 1.0 ||
        fabs(mode->real) > WG_DOUBLE_BASS_BODY_STATE_LIMIT ||
        fabs(mode->imaginary) > WG_DOUBLE_BASS_BODY_STATE_LIMIT) {
      return 0;
    }
    for (string_index = 0U; string_index < WG_DOUBLE_BASS_STRINGS;
         string_index++) {
      if (!isfinite(mode->source_gain[string_index])) {
        return 0;
      }
    }
  }
  return 1;
}

static void wg_double_bass_body_tick(WG_DOUBLE_BASS_STATE *state,
                                const double input[WG_DOUBLE_BASS_STRINGS],
                                double *left, double *right,
                                int32_t count_output)
{
  double aggregate = 0.0;
  double output_left;
  double output_right;
  uint32_t mode_index;
  uint32_t string_index;
  int32_t limit_hit = 0;

  for (string_index = 0U; string_index < WG_DOUBLE_BASS_STRINGS;
       string_index++) {
    if (!isfinite(input[string_index])) {
      wg_double_bass_clear_body_live(state, 1);
      *left = 0.0;
      *right = 0.0;
      return;
    }
    aggregate += 0.25 * input[string_index];
  }
  aggregate = wg_double_bass_clamp(
      aggregate, -WG_DOUBLE_BASS_BODY_DRIVE_LIMIT, WG_DOUBLE_BASS_BODY_DRIVE_LIMIT);
  state->body_last_drive = aggregate;
  state->body_drive_energy += aggregate * aggregate;
  state->body_wet_level = wg_double_bass_smooth(
      state->body_wet_level, state->body_wet_level_target,
      state->body_gain_smoothing);

  for (mode_index = 0U; mode_index < WG_DOUBLE_BASS_BODY_MODES; mode_index++) {
    WG_DOUBLE_BASS_BODY_MODE_STATE *mode = &state->body_modes[mode_index];
    const double drive_gain = 1.0 - mode->radius;
    double drive = 0.0;
    double old_real;
    double old_imaginary;

    mode->gain = wg_double_bass_smooth(
        mode->gain, mode->target_gain, state->body_gain_smoothing);
    if (!mode->active) {
      mode->real = 0.0;
      mode->imaginary = 0.0;
      continue;
    }
    for (string_index = 0U; string_index < WG_DOUBLE_BASS_STRINGS;
         string_index++) {
      drive += 0.25 * input[string_index] *
          mode->source_gain[string_index];
    }
    drive = wg_double_bass_clamp(
        drive, -WG_DOUBLE_BASS_BODY_DRIVE_LIMIT, WG_DOUBLE_BASS_BODY_DRIVE_LIMIT);
    old_real = mode->real;
    old_imaginary = mode->imaginary;
    mode->real = mode->radius *
            (mode->cosine * old_real - mode->sine * old_imaginary) +
        drive_gain * drive;
    mode->imaginary = mode->radius *
        (mode->sine * old_real + mode->cosine * old_imaginary);
    if (!isfinite(mode->real) || !isfinite(mode->imaginary)) {
      wg_double_bass_clear_body_live(state, 1);
      *left = 0.0;
      *right = 0.0;
      return;
    }
    if (fabs(mode->real) > WG_DOUBLE_BASS_BODY_STATE_LIMIT ||
        fabs(mode->imaginary) > WG_DOUBLE_BASS_BODY_STATE_LIMIT) {
      limit_hit = 1;
    }
  }

  wg_double_bass_body_current_output(state, &output_left, &output_right);
  {
    const WG_DOUBLE_BASS_BODY_MODEL *body = state->model != NULL
        ? &state->model->body : &wg_double_bass_fixed_model()->body;
    const double radiation_pole = exp(
        -WG_DOUBLE_BASS_TWO_PI *
        fmin(body->bridge_radiation_cutoff_hz,
             0.45 * state->sample_rate) / state->sample_rate);
    const double radiation_pan_gain = 0.70710678118654752440;
    double radiation;

    state->bridge_radiation_lowpass =
        radiation_pole * state->bridge_radiation_lowpass +
        (1.0 - radiation_pole) * aggregate;
    radiation = state->body_wet_level * body->bridge_radiation_gain *
        (aggregate - state->bridge_radiation_lowpass);
    output_left += radiation_pan_gain * radiation;
    output_right += radiation_pan_gain * radiation;
  }
  if (!isfinite(output_left) || !isfinite(output_right)) {
    wg_double_bass_clear_body_live(state, 1);
    *left = 0.0;
    *right = 0.0;
    return;
  }
  if (fabs(output_left) > WG_DOUBLE_BASS_BODY_OUTPUT_LIMIT ||
      fabs(output_right) > WG_DOUBLE_BASS_BODY_OUTPUT_LIMIT) {
    state->body_clips = wg_double_bass_add_samples(state->body_clips, 1U);
    limit_hit = 1;
  }
  if (limit_hit) {
    if (state->body_limit_streak < UINT32_MAX) {
      state->body_limit_streak++;
    }
    if (state->body_limit_streak >= WG_DOUBLE_BASS_BOW_RECOVERY_FAILURES) {
      wg_double_bass_clear_body_live(state, 1);
      output_left = 0.0;
      output_right = 0.0;
    }
  } else {
    state->body_limit_streak = 0U;
  }
  output_left = wg_double_bass_clamp(
      output_left, -WG_DOUBLE_BASS_BODY_OUTPUT_LIMIT,
      WG_DOUBLE_BASS_BODY_OUTPUT_LIMIT);
  output_right = wg_double_bass_clamp(
      output_right, -WG_DOUBLE_BASS_BODY_OUTPUT_LIMIT,
      WG_DOUBLE_BASS_BODY_OUTPUT_LIMIT);
  if (state->body_recovery_remaining > 0U) {
    state->body_recovery_remaining--;
    output_left = 0.0;
    output_right = 0.0;
  }
  state->body_last_left = output_left;
  state->body_last_right = output_right;
  state->body_peak_left = fmax(state->body_peak_left, fabs(output_left));
  state->body_peak_right = fmax(state->body_peak_right, fabs(output_right));
  if (count_output) {
    state->body_output_energy_left += output_left * output_left;
    state->body_output_energy_right += output_right * output_right;
  }
  state->body_samples = wg_double_bass_add_samples(state->body_samples, 1U);
  *left = output_left;
  *right = output_right;
}

static void wg_double_bass_body_fast_forward(WG_DOUBLE_BASS_STATE *state,
                                        uint64_t samples)
{
  const WG_DOUBLE_BASS_MODEL *model = state->model != NULL
      ? state->model : wg_double_bass_fixed_model();
  const double smoothing_decay = pow(
      1.0 - state->body_gain_smoothing, (double)samples);
  uint32_t mode_index;
  int32_t reset_on_entry = 0;

  if (samples == 0U) {
    return;
  }
  if (!wg_double_bass_body_state_is_finite(state)) {
    wg_double_bass_clear_body_live(state, 1);
    reset_on_entry = 1;
  } else {
    state->body_limit_streak = 0U;
  }
  state->body_wet_level = state->body_wet_level_target +
      (state->body_wet_level - state->body_wet_level_target) *
          smoothing_decay;
  for (mode_index = 0U; mode_index < WG_DOUBLE_BASS_BODY_MODES; mode_index++) {
    WG_DOUBLE_BASS_BODY_MODE_STATE *mode = &state->body_modes[mode_index];
    const double magnitude = mode->active ?
        pow(mode->radius, (double)samples) : 0.0;
    const double angle = fmod(
        (double)samples * WG_DOUBLE_BASS_TWO_PI *
            model->body.modes[mode_index].frequency /
            state->sample_rate,
        WG_DOUBLE_BASS_TWO_PI);
    const double old_real = mode->real;
    const double old_imaginary = mode->imaginary;

    mode->gain = mode->target_gain +
        (mode->gain - mode->target_gain) * smoothing_decay;
    if (magnitude < 1.0e-15 || !mode->active) {
      mode->real = 0.0;
      mode->imaginary = 0.0;
    } else {
      mode->real = magnitude *
          (cos(angle) * old_real - sin(angle) * old_imaginary);
      mode->imaginary = magnitude *
          (sin(angle) * old_real + cos(angle) * old_imaginary);
    }
  }
  if (!reset_on_entry) {
    if (state->body_recovery_remaining > samples) {
      state->body_recovery_remaining -= (uint32_t)samples;
    } else {
      state->body_recovery_remaining = 0U;
    }
  }
  state->body_last_drive = 0.0;
  state->bridge_radiation_lowpass = 0.0;
  wg_double_bass_body_current_output(
      state, &state->body_last_left, &state->body_last_right);
  if (!wg_double_bass_body_state_is_finite(state)) {
    wg_double_bass_clear_body_live(state, 1);
  } else {
    if (fabs(state->body_last_left) > WG_DOUBLE_BASS_BODY_OUTPUT_LIMIT ||
        fabs(state->body_last_right) > WG_DOUBLE_BASS_BODY_OUTPUT_LIMIT) {
      state->body_clips = wg_double_bass_add_samples(state->body_clips, 1U);
    }
    state->body_last_left = wg_double_bass_clamp(
        state->body_last_left, -WG_DOUBLE_BASS_BODY_OUTPUT_LIMIT,
        WG_DOUBLE_BASS_BODY_OUTPUT_LIMIT);
    state->body_last_right = wg_double_bass_clamp(
        state->body_last_right, -WG_DOUBLE_BASS_BODY_OUTPUT_LIMIT,
        WG_DOUBLE_BASS_BODY_OUTPUT_LIMIT);
    if (state->body_recovery_remaining > 0U) {
      state->body_last_left = 0.0;
      state->body_last_right = 0.0;
    }
  }
  state->body_samples = wg_double_bass_add_samples(state->body_samples, samples);
  state->body_fast_forward_samples = wg_double_bass_add_samples(
      state->body_fast_forward_samples, samples);
}

static void wg_double_bass_advance_body_gap(WG_DOUBLE_BASS_STATE *state,
                                       uint64_t samples)
{
  wg_double_bass_body_fast_forward(state, samples);
}

static void wg_double_bass_prepare_bridge_send(
    WG_DOUBLE_BASS_STRING_STATE *string, uint64_t epoch, uint32_t ksmps)
{
  const uint32_t bank = (uint32_t)(epoch & 1U);

  if (!string->bridge_send_valid[bank] ||
      string->bridge_send_epoch[bank] != epoch) {
    memset(string->bridge_send[bank], 0, (size_t)ksmps * sizeof(double));
    string->bridge_send_epoch[bank] = epoch;
    string->bridge_send_valid[bank] = 1;
  }
  if (!string->strange_coupling_send_valid[bank] ||
      string->strange_coupling_send_epoch[bank] != epoch) {
    memset(string->strange_coupling_send[bank], 0,
           (size_t)ksmps * sizeof(double));
    string->strange_coupling_send_epoch[bank] = epoch;
    string->strange_coupling_send_valid[bank] = 1;
  }
}

static void wg_double_bass_write_bridge_send(
    WG_DOUBLE_BASS_STRING_STATE *string, uint64_t epoch, uint32_t sample,
    double force)
{
  const uint32_t bank = (uint32_t)(epoch & 1U);
  const double finite_force = isfinite(force) ? force : 0.0;

  string->bridge_send[bank][sample] = finite_force;
  string->strange_coupling_send[bank][sample] = wg_double_bass_clamp(
      string->strange_coupling_mix, 0.0, 1.0);
  string->bridge_send_samples = wg_double_bass_add_samples(
      string->bridge_send_samples, 1U);
  string->bridge_send_energy += finite_force * finite_force;
  string->bridge_send_peak = fmax(
      string->bridge_send_peak, fabs(finite_force));
}

static void wg_double_bass_prepare_sympathetic_control(
    WG_DOUBLE_BASS_STATE *state, uint64_t epoch)
{
  const uint32_t bank = (uint32_t)(epoch & 1U);

  if (!state->sympathetic_control_valid[bank] ||
      state->sympathetic_control_epoch[bank] != epoch) {
    memset(state->sympathetic_control[bank], 0,
           (size_t)state->ksmps * sizeof(double));
    state->sympathetic_control_epoch[bank] = epoch;
    state->sympathetic_control_valid[bank] = 1;
  }
}

static double wg_double_bass_sympathetic_control_at(
    const WG_DOUBLE_BASS_STATE *state, uint64_t epoch, uint32_t sample)
{
  const uint64_t source_epoch = epoch > 0U ? epoch - 1U : 0U;
  const uint32_t bank = (uint32_t)(source_epoch & 1U);

  if (epoch == 0U || sample >= state->ksmps ||
      !state->sympathetic_control_valid[bank] ||
      state->sympathetic_control_epoch[bank] != source_epoch) {
    return 0.0;
  }
  return wg_double_bass_clamp(
      state->sympathetic_control[bank][sample], 0.0, 1.0);
}

static double wg_double_bass_sympathetic_control_max(
    const WG_DOUBLE_BASS_STATE *state, uint64_t epoch,
    uint32_t offset, uint32_t limit)
{
  double maximum = 0.0;
  uint32_t sample;

  for (sample = offset; sample < limit; sample++) {
    maximum = fmax(
        maximum, wg_double_bass_sympathetic_control_at(state, epoch, sample));
  }
  return maximum;
}

static void wg_double_bass_set_bow_state(WG_DOUBLE_BASS_STRING_STATE *string,
                                    uint32_t state)
{
  if (string->bow_state != state) {
    string->bow_state_transitions = wg_double_bass_add_samples(
        string->bow_state_transitions, 1U);
  }
  string->bow_state = state;
  switch (state) {
    case WG_DOUBLE_BASS_BOW_NO_MOTION:
      string->bow_no_motion_samples = wg_double_bass_add_samples(
          string->bow_no_motion_samples, 1U);
      break;
    case WG_DOUBLE_BASS_BOW_STICK:
      string->bow_stick_samples = wg_double_bass_add_samples(
          string->bow_stick_samples, 1U);
      break;
    case WG_DOUBLE_BASS_BOW_SLIP:
      string->bow_slip_samples = wg_double_bass_add_samples(
          string->bow_slip_samples, 1U);
      break;
    case WG_DOUBLE_BASS_BOW_SCRATCH:
      string->bow_scratch_samples = wg_double_bass_add_samples(
          string->bow_scratch_samples, 1U);
      break;
    default:
      break;
  }
}

static void wg_double_bass_hair_tick(WG_DOUBLE_BASS_STRING_STATE *string,
                                double sample_rate)
{
  const WG_DOUBLE_BASS_BOW_CONTACT_MODEL *contact =
      &wg_double_bass_bound_model(string)->bow.contact;
  const double load = string->bow_contact > 1.0e-7
      ? string->contact_force : 0.0;
  const double velocity = (-load -
      contact->hair_stiffness * string->hair_displacement) /
      contact->hair_damping;

  string->hair_velocity = wg_double_bass_clamp(
      velocity, -WG_DOUBLE_BASS_HAIR_SPEED_LIMIT,
      WG_DOUBLE_BASS_HAIR_SPEED_LIMIT);
  string->hair_displacement = wg_double_bass_clamp(
      string->hair_displacement + string->hair_velocity / sample_rate,
      -WG_DOUBLE_BASS_HAIR_DISPLACEMENT_LIMIT,
      WG_DOUBLE_BASS_HAIR_DISPLACEMENT_LIMIT);
  string->hair_effective_speed = wg_double_bass_clamp(
      string->bow_speed +
          contact->hair_contact_mix * string->hair_velocity,
      -1.25, 1.25);
}

/* Portable elementary math for the nonlinear bow-feedback paths.
   Adapted from OpenLibm v0.8.7: src/k_cos.c, src/k_sin.c, src/e_exp.c, src/e_log.c.
   Keep their coefficients and evaluation order; see the parity receipt.
   Other DSP math continues to use the host library. */
/*!
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 *
 * Copyright (C) 2004 by Sun Microsystems, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 */
#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(__GNUC__)
#pragma GCC push_options
#pragma GCC optimize ("fp-contract=off")
#endif

/* Reduced-range kernels evaluate x + y on [-pi/4, pi/4]. */
static double wg_double_bass_kernel_cos(double x, double y)
{
  static const double C1 = 4.16666666666666019037e-02;
  static const double C2 = -1.38888888888741095749e-03;
  static const double C3 = 2.48015872894767294178e-05;
  static const double C4 = -2.75573143513906633035e-07;
  static const double C5 = 2.08757232129817482790e-09;
  static const double C6 = -1.13596475577881948265e-11;
  const double z = x * x;
  const double square = z * z;
  const double r = z * (C1 + z * (C2 + z * C3)) +
      square * square * (C4 + z * (C5 + z * C6));
  const double half_z = 0.5 * z;
  const double w = 1.0 - half_z;

  return w + (((1.0 - w) - half_z) + (z * r - x * y));
}

static double wg_double_bass_kernel_sin(double x, double y)
{
  static const double S1 = -1.66666666666666324348e-01;
  static const double S2 = 8.33333333332248946124e-03;
  static const double S3 = -1.98412698298579493134e-04;
  static const double S4 = 2.75573137070700676789e-06;
  static const double S5 = -2.50507602534068634195e-08;
  static const double S6 = 1.58969099521155010221e-10;
  const double z = x * x;
  const double w = z * z;
  const double r = S2 + z * (S3 + z * S4) + z * w * (S5 + z * S6);
  const double v = z * x;

  return x - ((z * (0.5 * y - v * r) - y) - v * S1);
}

/* The contact law requests only [0, pi]. Reduce to the kernels' interval
   using a two-part pi/2; no general-purpose large-angle reduction needed. */
static double wg_double_bass_contact_cos(double angle)
{
  const double half_pi = 1.57079632679489655800;
  const double half_pi_tail = 6.12323399573676603587e-17;
  const int32_t quadrant = angle <= 1.5 * half_pi ? 1 : 2;
  double high;
  double low;
  double reduced;
  double tail;

  if (angle <= 0.5 * half_pi) {
    return wg_double_bass_kernel_cos(angle, 0.0);
  }
  high = angle - (double)quadrant * half_pi;
  low = (double)quadrant * half_pi_tail;
  reduced = high - low;
  tail = (high - reduced) - low;
  return quadrant == 1 ? -wg_double_bass_kernel_sin(reduced, tail) :
      -wg_double_bass_kernel_cos(reduced, tail);
}

static uint64_t wg_double_bass_math_bits(double value)
{
  uint64_t bits;
  memcpy(&bits, &value, sizeof(bits));
  return bits;
}

static double wg_double_bass_math_from_bits(uint64_t bits)
{
  double value;
  memcpy(&value, &bits, sizeof(value));
  return value;
}

/* OpenLibm's exp: reduce by a two-part ln(2), evaluate the primary-range
   polynomial, then scale by an exact power of two. Preserve IEEE edge cases
   even though ordinary bow-force arguments are much smaller. */
static double wg_double_bass_bow_exp(double x)
{
  static const double half[2] = {0.5, -0.5};
  static const double huge = 1.0e300;
  static const double overflow_threshold = 7.09782712893383973096e02;
  static const double underflow_threshold = -7.45133219101941108420e02;
  static const double ln2_high[2] = {
    6.93147180369123816490e-01, -6.93147180369123816490e-01
  };
  static const double ln2_low[2] = {
    1.90821492927058770002e-10, -1.90821492927058770002e-10
  };
  static const double inverse_ln2 = 1.44269504088896338700;
  static const double P1 = 1.66666666666666019037e-01;
  static const double P2 = -2.77777777770155933842e-03;
  static const double P3 = 6.61375632143793436117e-05;
  static const double P4 = -1.65339022054652515390e-06;
  static const double P5 = 4.13813679705723846039e-08;
  static volatile double two_minus_1000 = 9.33263618503218878990e-302;
  double high = 0.0;
  double low = 0.0;
  double c;
  double t;
  double y;
  double power_of_two;
  int32_t k = 0;
  uint32_t high_word = (uint32_t)(wg_double_bass_math_bits(x) >> 32);
  const uint32_t sign = (high_word >> 31) & 1U;

  high_word &= 0x7fffffffU;
  if (high_word >= 0x40862e42U) {
    if (high_word >= 0x7ff00000U) {
      const uint32_t low_word = (uint32_t)wg_double_bass_math_bits(x);
      if (((high_word & 0xfffffU) | low_word) != 0U) {
        return x + x;
      }
      return sign == 0U ? x : 0.0;
    }
    if (x > overflow_threshold) {
      return huge * huge;
    }
    if (x < underflow_threshold) {
      return two_minus_1000 * two_minus_1000;
    }
  }
  /* OpenLibm's exact-argument correction chooses the closer exp(1). */
  if (x == 1.0) {
    return 2.718281828459045235360;
  }
  if (high_word > 0x3fd62e42U) {
    if (high_word < 0x3ff0a2b2U) {
      high = x - ln2_high[sign];
      low = ln2_low[sign];
      k = 1 - 2 * (int32_t)sign;
    } else {
      k = (int32_t)(inverse_ln2 * x + half[sign]);
      t = (double)k;
      high = x - t * ln2_high[0];
      low = t * ln2_low[0];
    }
    x = high - low;
  } else if (high_word < 0x3e300000U) {
    if (huge + x > 1.0) {
      return 1.0 + x;
    }
  }

  t = x * x;
  /* Unsigned shifts avoid the donor's signed-left-shift undefined behavior. */
  power_of_two = wg_double_bass_math_from_bits(
      (uint64_t)(uint32_t)(k >= -1021 ? k + 1023 : k + 2023) << 52);
  c = x - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
  if (k == 0) {
    return 1.0 - ((x * c) / (c - 2.0) - x);
  }
  y = 1.0 - ((low - (x * c) / (2.0 - c)) - high);
  if (k >= -1021) {
    if (k == 1024) {
      return y * 2.0 * 0x1p1023;
    }
    return y * power_of_two;
  }
  return y * power_of_two * two_minus_1000;
}

/* OpenLibm's log: normalize to sqrt(2)/2 < x < sqrt(2), then combine
   its log1p polynomial with a two-part ln(2). Coefficients/order are
   unchanged from v0.8.7 src/e_log.c; bit access uses the shared memcpy helpers. */
static double wg_double_bass_bow_log(double x)
{
  static const double ln2_high = 6.93147180369123816490e-01;
  static const double ln2_low = 1.90821492927058770002e-10;
  static const double two54 = 1.80143985094819840000e+16;
  static const double Lg1 = 6.666666666666735130e-01;
  static const double Lg2 = 3.999999999940941908e-01;
  static const double Lg3 = 2.857142874366239149e-01;
  static const double Lg4 = 2.222219843214978396e-01;
  static const double Lg5 = 1.818357216161805012e-01;
  static const double Lg6 = 1.531383769920937332e-01;
  static const double Lg7 = 1.479819860511658591e-01;
  static const double zero = 0.0;
  uint64_t bits = wg_double_bass_math_bits(x);
  uint32_t high_word = (uint32_t)(bits >> 32);
  int32_t k = 0;
  uint32_t normalize;
  double f;
  double s;
  double z;
  double w;
  double t1;
  double t2;
  double remainder;
  double dk;
  double half_square;

  if ((high_word & 0x7fffffffU) < 0x00100000U ||
      (high_word & 0x80000000U) != 0U) {
    if ((bits & UINT64_C(0x7fffffffffffffff)) == 0U) {
      return -two54 / zero;
    }
    if ((high_word & 0x80000000U) != 0U) {
      return (x - x) / zero;
    }
    k -= 54;
    x *= two54;
    bits = wg_double_bass_math_bits(x);
    high_word = (uint32_t)(bits >> 32);
  }
  if (high_word >= 0x7ff00000U) {
    return x + x;
  }
  k += (int32_t)(high_word >> 20) - 1023;
  high_word &= 0x000fffffU;
  normalize = (high_word + 0x95f64U) & 0x100000U;
  x = wg_double_bass_math_from_bits(
      (bits & UINT64_C(0xffffffff)) |
      ((uint64_t)(high_word | (normalize ^ 0x3ff00000U)) << 32));
  k += (int32_t)(normalize >> 20);
  f = x - 1.0;
  if ((0x000fffffU & (2U + high_word)) < 3U) {
    if (f == zero) {
      if (k == 0) {
        return zero;
      }
      dk = (double)k;
      return dk * ln2_high + dk * ln2_low;
    }
    remainder = f * f * (0.5 - 0.33333333333333333 * f);
    if (k == 0) {
      return f - remainder;
    }
    dk = (double)k;
    return dk * ln2_high - ((remainder - dk * ln2_low) - f);
  }
  s = f / (2.0 + f);
  dk = (double)k;
  z = s * s;
  w = z * z;
  t1 = w * (Lg2 + w * (Lg4 + w * Lg6));
  t2 = z * (Lg1 + w * (Lg3 + w * (Lg5 + w * Lg7)));
  remainder = t2 + t1;
  /* Equivalent to the donor's signed (hx - 0x6147a) | (0x6b851 - hx). */
  if (high_word >= 0x6147aU && high_word <= 0x6b851U) {
    half_square = 0.5 * f * f;
    if (k == 0) {
      return f - (half_square - s * (half_square + remainder));
    }
    return dk * ln2_high -
        ((half_square - (s * (half_square + remainder) + dk * ln2_low)) - f);
  }
  if (k == 0) {
    return f - s * (f - remainder);
  }
  return dk * ln2_high - ((s * (f - remainder) - dk * ln2_low) - f);
}

#if defined(__clang__)
#pragma STDC FP_CONTRACT DEFAULT
#elif defined(__GNUC__)
#pragma GCC pop_options
#endif

static double wg_double_bass_contact_memory_tick(
    WG_DOUBLE_BASS_STRING_STATE *string, double static_force,
    double relative_velocity, uint32_t bow_state, double sample_rate)
{
  const WG_DOUBLE_BASS_MODEL *model = wg_double_bass_bound_model(string);
  const WG_DOUBLE_BASS_BOW_CONTACT_MODEL *contact = &model->bow.contact;
  const double static_mu = wg_double_bass_model_static_mu(model);
  const double thermal_drop = wg_double_bass_model_thermal_drop(model);
  const double old_memory = string->contact_memory;
  const double force_limit = fmax(
      1.0e-9, static_mu *
          string->contact_thermal_scale *
          string->bow_effective_force);
  double dynamic_force = static_force;

  string->contact_solver_iterations = 1U;
  if (bow_state == WG_DOUBLE_BASS_BOW_STICK) {
    const double target = wg_double_bass_clamp(
        static_force / contact->stiffness,
        -WG_DOUBLE_BASS_CONTACT_MEMORY_LIMIT,
        WG_DOUBLE_BASS_CONTACT_MEMORY_LIMIT);

    string->contact_memory = target;
    string->contact_memory_velocity = 0.0;
    string->contact_steady_displacement = target;
    string->contact_adhesion = 0.0;
  } else if (bow_state == WG_DOUBLE_BASS_BOW_SLIP ||
             bow_state == WG_DOUBLE_BASS_BOW_SCRATCH) {
    const double friction = string->bow_effective_force *
        wg_double_bass_friction_mu(string, relative_velocity);
    const double direction = relative_velocity < 0.0 ? -1.0 : 1.0;
    const double steady = direction * friction /
        contact->stiffness;
    const double steady_abs = fmax(fabs(steady), 1.0e-12);
    const double breakaway = contact->breakaway * steady_abs;
    const double memory_abs = fabs(old_memory);
    double adhesion = 0.0;
    double velocity;

    if (relative_velocity * old_memory > 0.0) {
      if (memory_abs >= steady_abs) {
        adhesion = 1.0;
      } else if (memory_abs > breakaway) {
        const double position =
            (memory_abs - breakaway) /
            fmax(steady_abs - breakaway, 1.0e-12);
        adhesion = 0.5 - 0.5 * wg_double_bass_contact_cos(
            WG_DOUBLE_BASS_TWO_PI * 0.5 * position);
      }
    }
    velocity = relative_velocity *
        (1.0 - adhesion * old_memory / steady);
    velocity = wg_double_bass_clamp(
        velocity, -WG_DOUBLE_BASS_FRICTION_MAX_SPEED,
        WG_DOUBLE_BASS_FRICTION_MAX_SPEED);
    string->contact_memory = wg_double_bass_clamp(
        old_memory + velocity / sample_rate,
        -WG_DOUBLE_BASS_CONTACT_MEMORY_LIMIT,
        WG_DOUBLE_BASS_CONTACT_MEMORY_LIMIT);
    string->contact_memory_velocity = velocity;
    string->contact_steady_displacement = steady;
    string->contact_adhesion = adhesion;
    dynamic_force = contact->stiffness *
        string->contact_memory +
        contact->damping * velocity;
    dynamic_force = wg_double_bass_clamp(
        dynamic_force, -force_limit, force_limit);
    dynamic_force = (1.0 - contact->dynamic_mix) *
        static_force + contact->dynamic_mix * dynamic_force;
    dynamic_force = wg_double_bass_clamp(
        dynamic_force,
        fmax(-force_limit, static_force - 0.08 * force_limit),
        fmin(force_limit, static_force + 0.08 * force_limit));
  } else {
    string->contact_memory = wg_double_bass_smooth(
        old_memory, 0.0, string->contact_memory_release);
    string->contact_memory_velocity =
        (string->contact_memory - old_memory) * sample_rate;
    string->contact_steady_displacement = 0.0;
    string->contact_adhesion = 0.0;
    dynamic_force = 0.0;
  }
  string->contact_force = dynamic_force;
  {
    const double work = fabs(dynamic_force * relative_velocity);
    const double heat_target = wg_double_bass_clamp(
        work / contact->thermal_work_scale, 0.0, 1.0);
    const double coefficient = heat_target > string->contact_temperature
        ? string->contact_thermal_attack
        : string->contact_thermal_release;

    string->contact_temperature = wg_double_bass_clamp(
        wg_double_bass_smooth(
            string->contact_temperature, heat_target, coefficient),
        0.0, 1.0);
    string->contact_thermal_scale = 1.0 -
        thermal_drop * string->contact_temperature;
    string->contact_thermal_work = fmin(
        1.0e12, string->contact_thermal_work + work / sample_rate);
  }
  return dynamic_force;
}

static double wg_double_bass_bow_mechanics_tick(
    WG_DOUBLE_BASS_STRING_STATE *string, double local_velocity,
    int32_t controls_enabled, double sample_rate)
{
  const int32_t contact_positive = controls_enabled &&
      string->articulation < WG_DOUBLE_BASS_ARTICULATION_PIZZICATO &&
      string->bow_contact > 1.0e-5;
  const int32_t bowed = contact_positive &&
      fabs(string->bow_speed) > 1.0e-4;
  const int32_t board_active =
      string->harmonic == 0U &&
      string->finger_position > WG_DOUBLE_BASS_FINGER_STOP_EPSILON &&
      string->finger_pressure > 0.50;
  double result = 0.0;

  if (!wg_double_bass_bow_mechanics_is_finite(string) ||
      !isfinite(local_velocity) || !(sample_rate > 0.0)) {
    wg_double_bass_recover_bow_mechanics(string);
    return 0.0;
  }

  string->board_displacement = wg_double_bass_clamp(board_active
      ? (string->board_displacement + local_velocity / sample_rate) *
          string->board_active_decay
      : string->board_displacement *
          string->board_inactive_decay,
      -WG_DOUBLE_BASS_BOARD_DISPLACEMENT_LIMIT,
      WG_DOUBLE_BASS_BOARD_DISPLACEMENT_LIMIT);
  {
    const double force_control = wg_double_bass_clamp(
        string->force, 0.0, 1.0);
    const double force_squared = force_control * force_control;
    const double clearance = WG_DOUBLE_BASS_BOARD_CLEARANCE *
        (1.0 - 0.75 * force_squared * force_squared);
    const double compression = board_active ? fmax(
        0.0, string->board_displacement - clearance) : 0.0;
    double force = 0.0;

    if (compression > 0.0) {
      force = -WG_DOUBLE_BASS_BOARD_STIFFNESS *
          compression * compression -
          WG_DOUBLE_BASS_BOARD_DAMPING * compression *
              fmax(local_velocity, 0.0);
      force = wg_double_bass_clamp(
          force, -2.0 * string->characteristic_impedance *
              WG_DOUBLE_BASS_BOARD_VELOCITY_LIMIT,
          2.0 * string->characteristic_impedance *
              WG_DOUBLE_BASS_BOARD_VELOCITY_LIMIT);
#if defined(HLOLLI_WG_DOUBLE_BASS_TEST_API)
      result += string->diagnostic_board_gain * force /
          (2.0 * string->characteristic_impedance);
#else
      result += force / (2.0 * string->characteristic_impedance);
#endif
      string->board_energy = fmin(
          1.0e12, string->board_energy + force * force);
      if (string->board_previous_compression <= 0.0) {
        string->board_impacts = wg_double_bass_add_samples(
            string->board_impacts, 1U);
      }
    }
    string->board_compression = compression;
    string->board_force = force;
    string->board_previous_compression = compression;
  }

  {
    const int32_t direction = bowed ? string->bow_direction : 0;

    if ((bowed && !string->mechanical_contact_positive) ||
        (bowed && direction != 0 &&
         string->mechanical_last_bow_direction != 0 &&
         direction != string->mechanical_last_bow_direction)) {
      string->mechanical_envelope = fmax(
          string->mechanical_envelope, 0.008);
      string->mechanical_events = wg_double_bass_add_samples(
          string->mechanical_events, 1U);
    }
    string->mechanical_contact_positive = bowed;
    if (direction != 0) {
      string->mechanical_last_bow_direction = direction;
    }
  }

  if (bowed || string->mechanical_envelope > 1.0e-8) {
    const double white = wg_double_bass_signed_noise(&string->bow_noise_rng);
    const double work = fabs(
        string->bow_friction_force * string->bow_relative_velocity);
    const double noise_level = bowed ?
        0.0032 * string->bow_contact *
            (0.18 + 0.82 * string->bow_scratch_score) *
            work / (0.020 + work) : 0.0;
    double noise;

    string->bow_noise_highpass = 0.94 *
        (string->bow_noise_highpass + white -
         string->bow_noise_previous_white);
    string->bow_noise_previous_white = white;
#if defined(HLOLLI_WG_DOUBLE_BASS_TEST_API)
    noise = string->diagnostic_rosin_gain * noise_level *
        string->bow_noise_highpass +
        string->diagnostic_mechanical_gain * string->mechanical_envelope *
            (0.70 * white + 0.30 * string->bow_noise_highpass);
#else
    noise = noise_level * string->bow_noise_highpass +
        string->mechanical_envelope *
            (0.70 * white + 0.30 * string->bow_noise_highpass);
#endif
    string->mechanical_envelope *= string->mechanical_decay;
    noise = wg_double_bass_clamp(
        noise, -WG_DOUBLE_BASS_BOW_MECHANICS_LIMIT,
        WG_DOUBLE_BASS_BOW_MECHANICS_LIMIT);
    result += noise;
    string->bow_noise_last = noise;
    if (fabs(noise) > 1.0e-12) {
      string->bow_noise_samples = wg_double_bass_add_samples(
          string->bow_noise_samples, 1U);
      string->bow_noise_energy = fmin(
          1.0e12, string->bow_noise_energy + noise * noise);
      string->bow_noise_peak = fmax(
          string->bow_noise_peak, fabs(noise));
    }
  } else {
    string->bow_noise_last = 0.0;
  }
  return wg_double_bass_clamp(
      result, -WG_DOUBLE_BASS_BOW_MECHANICS_LIMIT,
      WG_DOUBLE_BASS_BOW_MECHANICS_LIMIT);
}

static void wg_double_bass_bow_mechanics_fast_forward_gap(
    WG_DOUBLE_BASS_STRING_STATE *string, uint64_t samples,
    double sample_rate)
{
  const WG_DOUBLE_BASS_MODEL *model = wg_double_bass_bound_model(string);
  const WG_DOUBLE_BASS_BOW_CONTACT_MODEL *contact = &model->bow.contact;
  const double thermal_drop = wg_double_bass_model_thermal_drop(model);
  double coefficient;
  double decay;
  double hair_step;

  if (samples == 0U) {
    return;
  }
  if (!(sample_rate > 0.0) ||
      !wg_double_bass_bow_mechanics_is_finite(string)) {
    wg_double_bass_recover_bow_mechanics(string);
    return;
  }

  coefficient = wg_double_bass_clamp(
      string->contact_memory_release, 0.0, 1.0);
  decay = pow(1.0 - coefficient, (double)samples);
  string->contact_memory *= decay;
  string->contact_memory_velocity = coefficient < 1.0
      ? -sample_rate * coefficient * string->contact_memory /
          fmax(1.0 - coefficient, 1.0e-12)
      : 0.0;
  string->contact_adhesion = 0.0;
  string->contact_steady_displacement = 0.0;
  string->contact_force = 0.0;

  coefficient = wg_double_bass_clamp(
      string->contact_thermal_release, 0.0, 1.0);
  string->contact_temperature *= pow(
      1.0 - coefficient, (double)samples);
  string->contact_thermal_scale = 1.0 -
      thermal_drop * string->contact_temperature;

  hair_step = wg_double_bass_clamp(
      1.0 - contact->hair_stiffness /
          (contact->hair_damping * sample_rate),
      0.0, 1.0);
  string->hair_displacement *= pow(hair_step, (double)samples);
  string->hair_velocity = hair_step > 1.0e-12
      ? wg_double_bass_clamp(
            -contact->hair_stiffness * string->hair_displacement /
                (contact->hair_damping * hair_step),
            -WG_DOUBLE_BASS_HAIR_SPEED_LIMIT,
            WG_DOUBLE_BASS_HAIR_SPEED_LIMIT)
      : 0.0;
  string->hair_effective_speed = wg_double_bass_clamp(
      string->bow_speed +
          contact->hair_contact_mix * string->hair_velocity,
      -1.25, 1.25);

  string->board_displacement *= pow(
      wg_double_bass_clamp(string->board_inactive_decay, 0.0, 1.0),
      (double)samples);
  string->board_compression = 0.0;
  string->board_force = 0.0;
  string->board_previous_compression = 0.0;

  string->mechanical_envelope *= pow(
      wg_double_bass_clamp(string->mechanical_decay, 0.0, 1.0),
      (double)samples);
  if (string->mechanical_envelope <= 1.0e-8) {
    string->mechanical_envelope = 0.0;
  }
  string->bow_noise_highpass *= pow(0.94, (double)samples);
  string->bow_noise_previous_white = 0.0;
  string->bow_noise_last = 0.0;
  string->mechanical_contact_positive = 0;
  string->coupling_input = 0.0;
  string->coupling_source_mask = 0U;
}

static void wg_double_bass_strange_fast_forward_gap(
    WG_DOUBLE_BASS_STRING_STATE *string, uint64_t samples)
{
  if (samples == 0U) {
    return;
  }
  if (!wg_double_bass_strange_live_is_finite(string)) {
    wg_double_bass_recover_strange(string);
    return;
  }
  string->strange_air_state *= pow(0.995, (double)samples);
  string->strange_air_previous_white = 0.0;
  string->strange_dispersion_input *= pow(0.9995, (double)samples);
  string->strange_dispersion_output *= pow(0.9995, (double)samples);
  string->strange_subharmonic_envelope *= pow(
      0.998, (double)samples);
  string->strange_squeal_state *= pow(0.96, (double)samples);
  string->strange_coupling_mix *= pow(0.9995, (double)samples);
  string->strange_last_output = 0.0;
  string->strange_last_bow_state = WG_DOUBLE_BASS_BOW_OFF;
}

static double wg_double_bass_bridge_coupling_tick(
    WG_DOUBLE_BASS_STATE *state, uint32_t target_index, uint64_t epoch,
    uint32_t sample)
{
  WG_DOUBLE_BASS_STRING_STATE *target = &state->strings[target_index];
  const uint64_t source_epoch = epoch > 0U ? epoch - 1U : 0U;
  const uint32_t bank = (uint32_t)(source_epoch & 1U);
  const double sympathetic = wg_double_bass_sympathetic_control_at(
      state, epoch, sample);
  const int32_t reserved_target = target->controller_owner != NULL &&
      target->articulation > WG_DOUBLE_BASS_ARTICULATION_TRATTO;
  const int32_t active_target = !reserved_target &&
      target->trigger > 1.0e-7;
  const int32_t open_target = target->harmonic == 0U &&
      target->finger_target_position <= WG_DOUBLE_BASS_FINGER_STOP_EPSILON;
  const double open_mix = !reserved_target && open_target
      ? sympathetic * (1.0 - wg_double_bass_clamp(
            target->finger_pressure, 0.0, 1.0))
      : 0.0;
  const double target_mix = (active_target ? 1.0 : 0.0) +
      state->model->coupling.sympathetic_open_scale * open_mix;
  double result = 0.0;
  uint32_t source_mask = 0U;
  uint32_t source_index;

  if (!wg_double_bass_bow_mechanics_is_finite(target)) {
    wg_double_bass_recover_bow_mechanics(target);
  }
  if (target_mix <= 1.0e-12) {
    target->coupling_input = 0.0;
    target->coupling_source_mask = 0U;
    return 0.0;
  }
  if (epoch > 0U) {
    for (source_index = 0U; source_index < WG_DOUBLE_BASS_STRINGS;
         source_index++) {
      WG_DOUBLE_BASS_STRING_STATE *source;
      double force;
      double strange_mix = 0.0;

      if (source_index == target_index) {
        continue;
      }
      source = &state->strings[source_index];
      if (!source->bridge_send_valid[bank] ||
          source->bridge_send_epoch[bank] != source_epoch) {
        continue;
      }
      force = source->bridge_send[bank][sample];
      if (!isfinite(force)) {
        continue;
      }
      if (source->strange_coupling_send_valid[bank] &&
          source->strange_coupling_send_epoch[bank] == source_epoch) {
        strange_mix = wg_double_bass_clamp(
            source->strange_coupling_send[bank][sample], 0.0, 1.0);
      }
      result += target_mix *
          (1.0 + WG_DOUBLE_BASS_STRANGE_COUPLING *
              strange_mix) *
          state->bridge_coupling_gain[target_index][source_index] * force;
      if (fabs(force) > 1.0e-12) {
        source_mask |= 1U << source_index;
      }
    }
  }
  result = wg_double_bass_clamp(
      result, -WG_DOUBLE_BASS_COUPLING_LIMIT,
      WG_DOUBLE_BASS_COUPLING_LIMIT);
  target->coupling_input = result;
  target->coupling_source_mask = source_mask;
  if (source_mask != 0U) {
    target->coupling_samples = wg_double_bass_add_samples(
        target->coupling_samples, 1U);
    target->coupling_energy = fmin(
        1.0e12, target->coupling_energy + result * result);
    target->coupling_peak = fmax(
        target->coupling_peak, fabs(result));
  }
  return result;
}

static double wg_double_bass_strange_squeal_frequency(
    double effective_frequency, double sample_rate)
{
  const double ceiling = fmin(7800.0, 0.40 * sample_rate);
  const double floor_frequency = fmin(4200.0, ceiling);

  return wg_double_bass_clamp(
      13.0 * effective_frequency, floor_frequency, ceiling);
}

static void wg_double_bass_map_bow_force(WG_DOUBLE_BASS_STRING_STATE *string)
{
  const WG_DOUBLE_BASS_MODEL *model = wg_double_bass_bound_model(string);
  const WG_DOUBLE_BASS_BOW_FORCE_MAP_MODEL *force_map =
      &model->bow.force_map;
  const double static_mu = wg_double_bass_model_static_mu(model);
  const double thermal_drop = wg_double_bass_model_thermal_drop(model);
  const double beta = fmax(
      string->bridge_delay /
          fmax(2.0, string->bridge_delay + string->nut_delay),
      0.01);
  const double velocity = fmax(fabs(string->bow_speed), 0.02);
  const double thermal_scale = wg_double_bass_clamp(
      string->contact_thermal_scale,
      1.0 - thermal_drop, 1.0);
  const double dynamic_mu = wg_double_bass_friction_mu(
      string, velocity / beta) / thermal_scale;
  const double mu_drop = fmax(
      force_map->mu_drop_floor, static_mu - dynamic_mu);
  const double maximum_raw =
      2.0 * string->characteristic_impedance * velocity /
      (beta * mu_drop);
  const double maximum = wg_double_bass_clamp(maximum_raw, 0.05, 4.0);
  const double minimum_raw =
      string->characteristic_impedance * velocity /
      (force_map->minimum_divisor * beta * beta * mu_drop);
  const double minimum = wg_double_bass_clamp(
      minimum_raw, 0.005, maximum / 1.5);
  const double control = wg_double_bass_clamp(
      string->gesture_force_output, 0.0, 1.0);
  double requested;

  if (control < force_map->control_low) {
    requested = minimum * control / force_map->control_low;
  } else if (control < force_map->control_high) {
    const double mix = (control - force_map->control_low) /
        force_map->control_middle_span;
    requested = wg_double_bass_bow_exp(
        (1.0 - mix) * wg_double_bass_bow_log(minimum) +
        mix * wg_double_bass_bow_log(force_map->normal_max_scale * maximum));
  } else {
    const double mix = (control - force_map->control_high) /
        force_map->control_high_span;
    const double high = fmin(
        4.0, force_map->extreme_max_scale * maximum);
    requested = wg_double_bass_bow_exp(
        (1.0 - mix) *
            wg_double_bass_bow_log(force_map->normal_max_scale * maximum) +
        mix * wg_double_bass_bow_log(high));
  }
  string->bow_min_force = minimum * string->bow_contact;
  string->bow_max_force = maximum * string->bow_contact;
  string->bow_requested_force = requested * string->bow_contact;
  string->bow_effective_force = fmin(
      string->bow_requested_force,
      2.0 * string->characteristic_impedance *
          WG_DOUBLE_BASS_FRICTION_MAX_SPEED / static_mu);
  if (string->strange < 0.0) {
    const double amount = -string->strange;
    const double weak = wg_double_bass_clamp(
        (1.0 - 0.45 * amount) *
            (1.0 + 0.16 * amount * string->strange_air_state),
        0.35, 1.0);

    string->bow_min_force *= weak;
    string->bow_max_force *= weak;
    string->bow_requested_force *= weak;
    string->bow_effective_force *= weak;
  }
}

static double wg_double_bass_strange_dispersion_tick(
    WG_DOUBLE_BASS_STRING_STATE *string, double input)
{
  const double amount = fmax(0.0, string->strange);

  if (!wg_double_bass_strange_live_is_finite(string) || !isfinite(input)) {
    wg_double_bass_recover_strange(string);
    return isfinite(input) ? input : 0.0;
  }
  if (amount <= 1.0e-12) {
    string->strange_dispersion_input *= 0.9995;
    string->strange_dispersion_output *= 0.9995;
    return input;
  }
  {
    const double coefficient = 0.12 + 0.54 * amount *
        (0.25 + 0.75 * tanh(2.0 * fabs(input)));
    const double dispersed = wg_double_bass_clamp(
        -coefficient * input + string->strange_dispersion_input +
            coefficient * string->strange_dispersion_output,
        -WG_DOUBLE_BASS_WAVE_LIMIT, WG_DOUBLE_BASS_WAVE_LIMIT);
    const double mix = 0.015 * amount * amount;

    string->strange_dispersion_input = input;
    string->strange_dispersion_output = dispersed;
    return wg_double_bass_clamp(
        input + mix * (dispersed - input),
        -WG_DOUBLE_BASS_WAVE_LIMIT, WG_DOUBLE_BASS_WAVE_LIMIT);
  }
}

static double wg_double_bass_strange_tick(
    WG_DOUBLE_BASS_STRING_STATE *string, int32_t controls_enabled,
    double sample_rate)
{
  const double negative = fmax(0.0, -string->strange);
  const double positive = fmax(0.0, string->strange);
  const int32_t bowed = controls_enabled &&
      string->articulation < WG_DOUBLE_BASS_ARTICULATION_PIZZICATO &&
      string->trigger > 1.0e-7 && string->force > 1.0e-7 &&
      fabs(string->speed) > 1.0e-4 &&
      string->bow_contact > 1.0e-5 &&
      fabs(string->bow_speed) > 1.0e-4;
  double air = 0.0;
  double subharmonic = 0.0;
  double squeal = 0.0;
  double output;

  if (!(sample_rate > 0.0) ||
      !wg_double_bass_strange_live_is_finite(string)) {
    wg_double_bass_recover_strange(string);
    return 0.0;
  }
  {
    const double coupling_target = bowed
        ? positive * string->bow_contact : 0.0;
    const double coupling_coefficient =
        coupling_target > string->strange_coupling_mix ? 0.0025 : 0.0005;

    string->strange_coupling_mix = wg_double_bass_clamp(
        wg_double_bass_smooth(
            string->strange_coupling_mix, coupling_target,
            coupling_coefficient),
        0.0, 1.0);
  }

  if (negative > 1.0e-12 && bowed) {
    const double white = wg_double_bass_signed_noise(&string->strange_rng);
    const double work = fabs(
        string->bow_friction_force * string->bow_relative_velocity);

    string->strange_air_state = wg_double_bass_clamp(
        0.92 * (string->strange_air_state + white -
            string->strange_air_previous_white), -1.0, 1.0);
    string->strange_air_previous_white = white;
    air = WG_DOUBLE_BASS_STRANGE_AIR_LIMIT * negative *
        string->bow_contact * work / (0.010 + work) *
        string->strange_air_state;
  } else {
    string->strange_air_state *= 0.995;
    string->strange_air_previous_white = 0.0;
  }

  if (positive > 1.0e-12 && bowed) {
    const double slip = wg_double_bass_clamp(
        fabs(string->bow_relative_velocity) / 0.30, 0.0, 1.0);
    const double force_gate = wg_double_bass_clamp(
        (string->gesture_force_output - 0.20) / 0.55, 0.0, 1.0);
    const double target = 0.35 * WG_DOUBLE_BASS_STRANGE_SUBHARMONIC_LIMIT *
        positive * positive * string->bow_contact * force_gate * slip *
        (0.35 + 0.65 * string->bow_scratch_score);
    const double coefficient = target > string->strange_subharmonic_envelope
        ? 0.0025 : 0.00035;
    const double phase_rate = 0.5 * (
        isfinite(string->effective_frequency) &&
            string->effective_frequency > 0.0
        ? string->effective_frequency : string->open_frequency);
    const double squeal_drive = tanh(
        3.0 * (string->strange_dispersion_output -
            string->strange_dispersion_input) +
        1.5 * string->bow_relative_velocity);
    const double squeal_target = WG_DOUBLE_BASS_STRANGE_SQUEAL_LIMIT *
        positive * positive * positive *
        string->bow_contact * force_gate * slip *
        (0.45 + 0.55 * string->bow_scratch_score) *
        (0.65 + 0.35 * fabs(squeal_drive));
    const double squeal_frequency = wg_double_bass_strange_squeal_frequency(
        string->effective_frequency, sample_rate);

    string->strange_subharmonic_envelope = wg_double_bass_smooth(
        string->strange_subharmonic_envelope, target, coefficient);
    string->strange_subharmonic_phase += phase_rate / sample_rate;
    string->strange_subharmonic_phase -=
        floor(string->strange_subharmonic_phase);
    subharmonic = string->strange_subharmonic_envelope * sin(
        WG_DOUBLE_BASS_TWO_PI * string->strange_subharmonic_phase);
    string->strange_squeal_state = wg_double_bass_smooth(
        string->strange_squeal_state, squeal_target, 0.0025);
    string->strange_squeal_phase += squeal_frequency / sample_rate;
    string->strange_squeal_phase -= floor(string->strange_squeal_phase);
    squeal = string->strange_squeal_state * sin(
        WG_DOUBLE_BASS_TWO_PI * string->strange_squeal_phase);
  } else {
    string->strange_subharmonic_envelope *= 0.998;
    string->strange_squeal_state *= 0.96;
  }
  string->strange_last_bow_state = string->bow_state;
  output = wg_double_bass_clamp(
      air + subharmonic + squeal,
      -WG_DOUBLE_BASS_STRANGE_OUTPUT_LIMIT,
      WG_DOUBLE_BASS_STRANGE_OUTPUT_LIMIT);
  string->strange_last_output = output;
  if (fabs(output) > 1.0e-12) {
    string->strange_samples = wg_double_bass_add_samples(
        string->strange_samples, 1U);
    string->strange_energy = fmin(
        1.0e12, string->strange_energy + output * output);
    string->strange_peak = fmax(string->strange_peak, fabs(output));
  }
  return output;
}

static double wg_double_bass_bow_root_value(
    const WG_DOUBLE_BASS_STRING_STATE *string, double speed,
    double force_scale, double free_speed)
{
  return speed + force_scale * wg_double_bass_friction_mu(string, speed) -
      free_speed;
}

static double wg_double_bass_bow_tick(WG_DOUBLE_BASS_STRING_STATE *string,
                                 double free_velocity,
                                 double sample_rate)
{
  const double static_mu = wg_double_bass_model_static_mu(
      wg_double_bass_bound_model(string));
  const double impedance = string->characteristic_impedance;
  double scratch_target = 0.0;
  double free_relative;
  double required_force;
  double force = 0.0;
  double increment = 0.0;
  double relative = 0.0;
  uint32_t state = WG_DOUBLE_BASS_BOW_OFF;
  int32_t failed = 0;
  int32_t capped = 0;
  int32_t recovering = 0;

  if (!wg_double_bass_bow_mechanics_is_finite(string)) {
    string->contact_solver_failures = wg_double_bass_add_samples(
        string->contact_solver_failures, 1U);
    wg_double_bass_recover_bow_mechanics(string);
  }
  wg_double_bass_hair_tick(string, sample_rate);
  string->bow_free_velocity = free_velocity;
  string->bow_solver_iterations = 0U;
  string->bow_root_count = 0U;
  string->bow_solver_residual = 0.0;
  string->bow_solver_bracket_width = 0.0;
  if (!(impedance > 0.0) || !isfinite(impedance) ||
      !isfinite(string->hair_effective_speed) ||
      !isfinite(string->bow_contact) ||
      !isfinite(string->bow_position) || !isfinite(free_velocity) ||
      !isfinite(string->bow_scratch_score) ||
      !isfinite(string->bow_previous_slip_speed) ||
      string->friction_lut == NULL) {
    string->bow_solver_failures = wg_double_bass_add_samples(
        string->bow_solver_failures, 1U);
    string->bow_solver_fallbacks = wg_double_bass_add_samples(
        string->bow_solver_fallbacks, 1U);
    string->bow_recoveries = wg_double_bass_add_samples(
        string->bow_recoveries, 1U);
    wg_double_bass_reset_bow_live(string);
    wg_double_bass_clear_contact_response(string);
    string->bow_recovery_remaining = WG_DOUBLE_BASS_BOW_RECOVERY_SAMPLES;
    string->bow_scratch_score = 1.0;
    wg_double_bass_set_bow_state(string, WG_DOUBLE_BASS_BOW_SCRATCH);
    return 0.0;
  }
  if (string->bow_contact <= 1.0e-7 ||
      string->gesture_force_output <= 1.0e-7) {
    string->bow_requested_force = 0.0;
    string->bow_effective_force = 0.0;
    string->bow_friction_force = 0.0;
    string->bow_junction_increment = 0.0;
    string->bow_string_velocity = free_velocity;
    string->bow_relative_velocity =
        string->hair_effective_speed - free_velocity;
    IGN(wg_double_bass_contact_memory_tick(
        string, 0.0, string->bow_relative_velocity,
        WG_DOUBLE_BASS_BOW_OFF, sample_rate));
    string->bow_scratch_score += string->bow_scratch_fall *
        (0.0 - string->bow_scratch_score);
    wg_double_bass_set_bow_state(string, WG_DOUBLE_BASS_BOW_OFF);
    return 0.0;
  }

  wg_double_bass_map_bow_force(string);
  capped = string->bow_effective_force + 1.0e-12 <
      string->bow_requested_force;
  free_relative = string->hair_effective_speed - free_velocity;
  required_force = 2.0 * impedance * free_relative;
  if (string->bow_recovery_remaining > 0U) {
    string->bow_recovery_remaining--;
    relative = free_relative;
    state = WG_DOUBLE_BASS_BOW_SCRATCH;
    scratch_target = 1.0;
    recovering = 1;
  } else if (fabs(string->hair_effective_speed) < 1.0e-4 &&
             fabs(free_velocity) < 1.0e-4) {
    state = WG_DOUBLE_BASS_BOW_NO_MOTION;
    string->bow_failure_streak = 0U;
  } else if (fabs(required_force) <=
             static_mu *
                 string->contact_thermal_scale *
                 string->bow_effective_force) {
    force = required_force;
    state = WG_DOUBLE_BASS_BOW_STICK;
    string->bow_failure_streak = 0U;
  } else if (string->bow_effective_force > 0.0) {
    double bracket_low[4];
    double bracket_high[4];
    double bracket_value_low[4];
    const double sign = free_relative < 0.0 ? -1.0 : 1.0;
    const double free_speed = fabs(free_relative);
    const double force_scale = string->bow_effective_force /
        (2.0 * impedance);
    double previous_x = 0.0;
    double previous_value = wg_double_bass_bow_root_value(
        string, 0.0, force_scale, free_speed);
    uint32_t root_count = 0U;
    uint32_t interval;
    uint32_t selected = 0U;
    double low;
    double high;
    double value_low;
    double root;
    double residual;

    string->bow_solver_calls = wg_double_bass_add_samples(
        string->bow_solver_calls, 1U);
    for (interval = 1U; interval <= WG_DOUBLE_BASS_BOW_SCAN_STEPS;
         interval++) {
      const double unit = (double)interval /
          (double)WG_DOUBLE_BASS_BOW_SCAN_STEPS;
      const double x = free_speed * unit * unit * unit;
      const double value = wg_double_bass_bow_root_value(
          string, x, force_scale, free_speed);
      if (root_count < 4U &&
          ((previous_value <= 0.0 && value >= 0.0) ||
           (previous_value >= 0.0 && value <= 0.0))) {
        bracket_low[root_count] = previous_x;
        bracket_high[root_count] = x;
        bracket_value_low[root_count] = previous_value;
        root_count++;
      }
      previous_x = x;
      previous_value = value;
    }
    string->bow_root_count = root_count;
    if (root_count == 0U) {
      failed = 1;
      root = free_speed;
      residual = fabs(previous_value);
      low = high = root;
    } else {
      if ((string->bow_state == WG_DOUBLE_BASS_BOW_SLIP ||
           string->bow_state == WG_DOUBLE_BASS_BOW_SCRATCH) &&
          string->bow_previous_slip_speed > 0.0) {
        double best = HUGE_VAL;
        for (interval = 0U; interval < root_count; interval++) {
          const double middle = 0.5 *
              (bracket_low[interval] + bracket_high[interval]);
          const double distance = fabs(
              middle - string->bow_previous_slip_speed);
          if (distance < best) {
            best = distance;
            selected = interval;
          }
        }
      } else {
        selected = root_count - 1U;
      }
      low = bracket_low[selected];
      high = bracket_high[selected];
      value_low = bracket_value_low[selected];
      root = 0.5 * (low + high);
      for (interval = 0U; interval < WG_DOUBLE_BASS_BOW_SOLVE_STEPS;
           interval++) {
        const double value = wg_double_bass_bow_root_value(
            string, root, force_scale, free_speed);
        const double derivative = 1.0 + force_scale *
            wg_double_bass_friction_mu_slope(string, root);
        double candidate;
        string->bow_solver_iterations = interval + 1U;
        if ((value_low <= 0.0 && value >= 0.0) ||
            (value_low >= 0.0 && value <= 0.0)) {
          high = root;
        } else {
          low = root;
          value_low = value;
        }
        candidate = fabs(derivative) > 1.0e-8 ?
            root - value / derivative : 0.5 * (low + high);
        if (!isfinite(candidate) ||
            candidate <= low + 0.05 * (high - low) ||
            candidate >= high - 0.05 * (high - low)) {
          candidate = 0.5 * (low + high);
        }
        root = candidate;
      }
      residual = fabs(wg_double_bass_bow_root_value(
          string, root, force_scale, free_speed));
      {
        const double value_high = wg_double_bass_bow_root_value(
            string, high, force_scale, free_speed);
        const double denominator = value_high - value_low;
        const double candidate = fabs(denominator) > 1.0e-15 ?
            low - value_low * (high - low) / denominator : root;
        if (isfinite(candidate) && candidate >= low && candidate <= high) {
          const double candidate_residual = fabs(wg_double_bass_bow_root_value(
              string, candidate, force_scale, free_speed));
          if (candidate_residual < residual) {
            root = candidate;
            residual = candidate_residual;
          }
        }
      }
      if (!isfinite(residual) ||
          residual > 1.0e-5 * (1.0 + free_speed) ||
          high - low > 1.0e-3 * (1.0 + free_speed)) {
        failed = 1;
      }
    }
    string->bow_solver_residual = residual;
    string->bow_solver_bracket_width = high - low;
    if (string->bow_solver_iterations >
        string->bow_solver_iterations_max) {
      string->bow_solver_iterations_max =
          string->bow_solver_iterations;
    }
    if (failed) {
      force = sign * string->bow_effective_force *
          wg_double_bass_friction_mu(string, free_speed);
      string->bow_solver_failures = wg_double_bass_add_samples(
          string->bow_solver_failures, 1U);
      string->bow_solver_fallbacks = wg_double_bass_add_samples(
          string->bow_solver_fallbacks, 1U);
      if (string->bow_failure_streak < UINT32_MAX) {
        string->bow_failure_streak++;
      }
      scratch_target = 1.0;
    } else {
      force = sign * string->bow_effective_force *
          wg_double_bass_friction_mu(string, root);
      string->bow_failure_streak = 0U;
    }
    increment = force / (2.0 * impedance);
    relative = free_relative - increment;
    string->bow_previous_slip_speed = fabs(relative);
    state = WG_DOUBLE_BASS_BOW_SLIP;
  }

  if (string->bow_failure_streak >= WG_DOUBLE_BASS_BOW_RECOVERY_FAILURES) {
    string->bow_failure_streak = 0U;
    string->bow_recovery_remaining = WG_DOUBLE_BASS_BOW_RECOVERY_SAMPLES;
    string->bow_recoveries = wg_double_bass_add_samples(
        string->bow_recoveries, 1U);
    force = 0.0;
    relative = free_relative;
    state = WG_DOUBLE_BASS_BOW_SCRATCH;
    scratch_target = 1.0;
    recovering = 1;
    wg_double_bass_clear_contact_response(string);
  }
  if (state == WG_DOUBLE_BASS_BOW_STICK) {
    relative = 0.0;
    string->bow_previous_slip_speed = 0.0;
  } else if (state == WG_DOUBLE_BASS_BOW_NO_MOTION) {
    force = 0.0;
    relative = 0.0;
    string->bow_previous_slip_speed = 0.0;
  }
  if ((fabs(string->hair_effective_speed) > 1.0e-4 &&
       string->bow_requested_force < 0.8 * string->bow_min_force) ||
      string->bow_requested_force > string->bow_max_force ||
      string->bow_root_count > 1U || failed || capped) {
    scratch_target = 1.0;
  }
  string->bow_scratch_score +=
      (scratch_target > string->bow_scratch_score ?
           string->bow_scratch_rise : string->bow_scratch_fall) *
      (scratch_target - string->bow_scratch_score);
  if (string->bow_scratch_score > 0.5 &&
      state != WG_DOUBLE_BASS_BOW_NO_MOTION) {
    state = WG_DOUBLE_BASS_BOW_SCRATCH;
  }
  if (recovering) {
    wg_double_bass_clear_contact_response(string);
  } else {
    force = wg_double_bass_contact_memory_tick(
        string, force, relative, state, sample_rate);
  }
  increment = force / (2.0 * impedance);
  relative = free_relative - increment;
  if (!wg_double_bass_bow_mechanics_is_finite(string) ||
      !isfinite(force) || !isfinite(relative)) {
    string->contact_solver_failures = wg_double_bass_add_samples(
        string->contact_solver_failures, 1U);
    wg_double_bass_recover_bow_mechanics(string);
    force = 0.0;
    increment = 0.0;
    relative = free_relative;
    state = WG_DOUBLE_BASS_BOW_SCRATCH;
  }
  string->bow_friction_force = force;
  string->bow_junction_increment = increment;
  string->bow_string_velocity = free_velocity + increment;
  string->bow_relative_velocity = relative;
  string->bow_max_abs_relative = fmax(
      string->bow_max_abs_relative, fabs(relative));
  wg_double_bass_set_bow_state(string, state);
  return increment;
}

static double wg_double_bass_waveguide_step(WG_DOUBLE_BASS_STRING_STATE *string,
                                       int32_t controls_enabled,
                                       double sample_rate,
                                       double bridge_coupling)
{
  const WG_DOUBLE_BASS_STRING_MODEL *string_model =
      wg_double_bass_bound_string_model(string);
  const WG_DOUBLE_BASS_POLARIZATION_MODEL *second_model =
      &string_model->second_polarization;
  const double nut_loss_fraction = string_model->nut_loss_fraction;
  double desired_delay;
  double desired_velocity;
  double delay_step;
  double finger_excitation;
  double bridge_in;
  double nut_in;
  double bridge_reflected;
  double nut_reflected;
  double second_bridge_in;
  double second_nut_in;
  double second_bridge_reflected;
  double second_nut_reflected;
  double toward_bridge;
  double toward_nut;
  double second_toward_bridge;
  double second_toward_nut;
  double second_output;
  double polarization_secondary;
  double polarization_primary;
  double projected_return;
  double release_loop_gain;
  double junction_increment = 0.0;
  double output;

  if (string->toward_bridge == NULL || string->toward_nut == NULL ||
      string->second_toward_bridge == NULL ||
      string->second_toward_nut == NULL ||
      string->rail_size < 8U || !(sample_rate > 0.0)) {
    return 0.0;
  }
  desired_delay = string->target_delay;
  string->effective_frequency = string->target_frequency;
  if (string->target_frequency > 0.0 &&
      string->vibrato_depth_cents > 0.0 &&
      string->vibrato_rate_hz > 0.0) {
    const double cents = string->vibrato_depth_cents * sin(
        WG_DOUBLE_BASS_TWO_PI * string->vibrato_phase);
    const double ratio = exp(0.0005776226504666211 * cents);

    string->effective_frequency = string->target_frequency * ratio;
    desired_delay += sample_rate / string->target_frequency *
        (1.0 / ratio - 1.0);
  }
  desired_delay = wg_double_bass_clamp(
      desired_delay, 2.0, (double)string->rail_size - 2.0);
  if (string->vibrato_rate_hz > 0.0) {
    string->vibrato_phase += string->vibrato_rate_hz / sample_rate;
    string->vibrato_phase -= floor(string->vibrato_phase);
  }
  desired_velocity = wg_double_bass_clamp(
      0.20 * (desired_delay - string->delay),
      -WG_DOUBLE_BASS_DELAY_SLEW, WG_DOUBLE_BASS_DELAY_SLEW);
  string->delay_velocity = wg_double_bass_smooth(
      string->delay_velocity, desired_velocity,
      string->delay_velocity_smoothing);
  delay_step = string->delay_velocity;
  if ((desired_delay - string->delay) * delay_step <= 0.0 ||
      fabs(delay_step) > fabs(desired_delay - string->delay)) {
    delay_step = desired_delay - string->delay;
    string->delay_velocity = 0.0;
  }
  string->delay += delay_step;
  string->delay = wg_double_bass_clamp(
      string->delay, 2.0, (double)string->rail_size - 2.0);
  string->nut_gain = wg_double_bass_smooth(
      string->nut_gain, string->nut_gain_target,
      string->string_loss_smoothing);
  string->bridge_gain = wg_double_bass_smooth(
      string->bridge_gain, string->bridge_gain_target,
      string->string_loss_smoothing);
  string->second_nut_gain = wg_double_bass_smooth(
      string->second_nut_gain, string->second_nut_gain_target,
      string->string_loss_smoothing);
  string->second_bridge_gain = wg_double_bass_smooth(
      string->second_bridge_gain, string->second_bridge_gain_target,
      string->string_loss_smoothing);
  if (string->release_gate_positive != 0 &&
      string->release_gate_positive != 1) {
    string->release_gate_positive = string->trigger > 1.0e-7 ? 1 : 0;
  }
  if (!isfinite(string->release_gain_smoothing) ||
      string->release_gain_smoothing <= 0.0 ||
      string->release_gain_smoothing > 1.0) {
    string->release_gain_smoothing = wg_double_bass_smoothing_coefficient(
        WG_DOUBLE_BASS_RELEASE_ENGAGE_SECONDS, sample_rate);
  }
  if (!isfinite(string->release_loop_gain) ||
      !isfinite(string->release_loop_gain_target) ||
      string->release_loop_gain < 0.0 || string->release_loop_gain > 1.0 ||
      string->release_loop_gain_target < 0.0 ||
      string->release_loop_gain_target > 1.0) {
    if (string->release_gate_positive) {
      string->release_loop_gain = 1.0;
      string->release_loop_gain_target = 1.0;
    } else {
      wg_double_bass_refresh_release_gain_target(string);
      string->release_loop_gain = string->release_loop_gain_target;
    }
  }
  if (string->release_gate_positive) {
    string->release_loop_gain = 1.0;
    string->release_loop_gain_target = 1.0;
  } else {
    string->release_loop_gain = wg_double_bass_clamp(
        wg_double_bass_smooth(
            string->release_loop_gain,
            string->release_loop_gain_target,
            string->release_gain_smoothing),
        0.0, 1.0);
  }
  release_loop_gain = string->release_loop_gain;
  finger_excitation = wg_double_bass_finger_tick(string);
  if (controls_enabled) {
    const int32_t old_direction = string->bow_direction;
    wg_double_bass_gesture_tick(string, sample_rate);
    string->bow_speed = wg_double_bass_smooth(
        string->bow_speed, string->bow_speed_target,
        string->bow_speed_smoothing);
    string->bow_contact = wg_double_bass_smooth(
        string->bow_contact, string->bow_contact_target,
        string->bow_contact_target > string->bow_contact ?
            string->bow_contact_attack_smoothing :
            string->bow_contact_release_smoothing);
    string->bow_position = wg_double_bass_smooth(
        string->bow_position, string->bow_position_target,
        string->bow_position_smoothing);
    if (string->bow_contact_target > 1.0e-7 &&
        fabs(string->bow_speed) > 1.0e-4) {
      string->bow_direction = string->bow_speed < 0.0 ? -1 : 1;
      if (old_direction != 0 && old_direction != string->bow_direction) {
        string->bow_direction_changes = wg_double_bass_add_samples(
            string->bow_direction_changes, 1U);
      }
    }
  }
  wg_double_bass_split_delay(
      string->delay, string->bow_position,
      &string->bridge_delay, &string->nut_delay);
  wg_double_bass_split_delay(
      string->target_delay, string->bow_position_target,
      &string->target_bridge_delay, &string->target_nut_delay);
  polarization_secondary =
      string->articulation < WG_DOUBLE_BASS_ARTICULATION_PIZZICATO &&
      string->harmonic == 0U
          ? second_model->mix : 0.0;
  polarization_primary = sqrt(fmax(
      0.0, 1.0 - polarization_secondary * polarization_secondary));
  string->second_delay = wg_double_bass_clamp(
      string->delay / exp(
          0.0005776226504666211 * second_model->detune_cents),
      2.0, (double)string->rail_size - 2.0);
  wg_double_bass_split_delay(
      string->second_delay, string->bow_position,
      &string->second_bridge_delay, &string->second_nut_delay);
  bridge_in = wg_double_bass_cubic_delay_read(
      string->toward_bridge, string->rail_size,
      string->bridge_write_index, string->bridge_delay);
  nut_in = wg_double_bass_cubic_delay_read(
      string->toward_nut, string->rail_size,
      string->nut_write_index, string->nut_delay);
  second_bridge_in = wg_double_bass_cubic_delay_read(
      string->second_toward_bridge, string->rail_size,
      string->bridge_write_index, string->second_bridge_delay);
  second_nut_in = wg_double_bass_cubic_delay_read(
      string->second_toward_nut, string->rail_size,
      string->nut_write_index, string->second_nut_delay);
  if (!isfinite(bridge_in) || !isfinite(nut_in) ||
      !isfinite(second_bridge_in) || !isfinite(second_nut_in)) {
    wg_double_bass_zero_waveguide(string, 1);
    return 0.0;
  }
  bridge_reflected = wg_double_bass_reflection_tick(
      bridge_in, string->bridge_pole,
      /* The two powers form one full-loop loss at every pitch. */
      string->bridge_gain *
          pow(release_loop_gain, 1.0 - nut_loss_fraction),
      &string->bridge_state);
  bridge_reflected = wg_double_bass_strange_dispersion_tick(
      string, bridge_reflected);
  bridge_reflected = wg_double_bass_clamp(
      bridge_reflected + bridge_coupling,
      -WG_DOUBLE_BASS_WAVE_LIMIT, WG_DOUBLE_BASS_WAVE_LIMIT);
  nut_reflected = wg_double_bass_reflection_tick(
      nut_in, string->nut_pole,
      string->nut_gain * pow(release_loop_gain, nut_loss_fraction),
      &string->nut_state);
  if (string->finger_position > WG_DOUBLE_BASS_FINGER_STOP_EPSILON) {
    nut_reflected *= 0.965 + 0.035 * string->finger_pressure;
  }
  nut_reflected = wg_double_bass_clamp(
      nut_reflected + finger_excitation,
      -WG_DOUBLE_BASS_WAVE_LIMIT, WG_DOUBLE_BASS_WAVE_LIMIT);
  nut_reflected = wg_double_bass_harmonic_tick(
      string, nut_reflected, sample_rate);
  second_bridge_reflected = wg_double_bass_reflection_tick(
      second_bridge_in, string->second_bridge_pole,
      string->second_bridge_gain *
          pow(release_loop_gain, 1.0 - nut_loss_fraction),
      &string->second_bridge_state);
  second_nut_reflected = wg_double_bass_reflection_tick(
      second_nut_in, string->second_nut_pole,
      string->second_nut_gain *
          pow(release_loop_gain, nut_loss_fraction),
      &string->second_nut_state);
  if (string->finger_position > WG_DOUBLE_BASS_FINGER_STOP_EPSILON) {
    second_nut_reflected *= 0.965 + 0.035 * string->finger_pressure;
  }
  second_bridge_reflected = wg_double_bass_clamp(
      second_bridge_reflected, -WG_DOUBLE_BASS_WAVE_LIMIT,
      WG_DOUBLE_BASS_WAVE_LIMIT);
  second_nut_reflected = wg_double_bass_clamp(
      second_nut_reflected, -WG_DOUBLE_BASS_WAVE_LIMIT,
      WG_DOUBLE_BASS_WAVE_LIMIT);
  projected_return = polarization_primary *
      (bridge_reflected + nut_reflected) +
      polarization_secondary *
      (second_bridge_reflected + second_nut_reflected);
  if (controls_enabled && string->articulation <
          WG_DOUBLE_BASS_ARTICULATION_PIZZICATO) {
    /* Point law: string velocity = returning velocity + F / (2 Z0). */
    junction_increment = wg_double_bass_bow_tick(
        string, projected_return, sample_rate);
  } else {
    if (!wg_double_bass_bow_mechanics_is_finite(string)) {
      wg_double_bass_recover_bow_mechanics(string);
    }
    IGN(wg_double_bass_contact_memory_tick(
        string, 0.0, 0.0, WG_DOUBLE_BASS_BOW_OFF, sample_rate));
    wg_double_bass_hair_tick(string, sample_rate);
  }
  junction_increment += wg_double_bass_exciter_tick(
      string, bridge_reflected + nut_reflected,
      controls_enabled, sample_rate);
  junction_increment += wg_double_bass_bow_mechanics_tick(
      string, projected_return + junction_increment,
      controls_enabled, sample_rate);
  junction_increment += wg_double_bass_strange_tick(
      string, controls_enabled, sample_rate);
  toward_bridge = nut_reflected +
      polarization_primary * junction_increment;
  toward_nut = bridge_reflected +
      polarization_primary * junction_increment;
  second_toward_bridge = second_nut_reflected +
      polarization_secondary * junction_increment;
  second_toward_nut = second_bridge_reflected +
      polarization_secondary * junction_increment;
  if (!isfinite(toward_bridge) || !isfinite(toward_nut) ||
      !isfinite(second_toward_bridge) ||
      !isfinite(second_toward_nut)) {
    wg_double_bass_zero_waveguide(string, 1);
    return 0.0;
  }
  if (fabs(toward_bridge) > WG_DOUBLE_BASS_WAVE_LIMIT ||
      fabs(toward_nut) > WG_DOUBLE_BASS_WAVE_LIMIT ||
      fabs(second_toward_bridge) > WG_DOUBLE_BASS_WAVE_LIMIT ||
      fabs(second_toward_nut) > WG_DOUBLE_BASS_WAVE_LIMIT) {
    string->wave_clips = wg_double_bass_add_samples(string->wave_clips, 1U);
    if (string->bow_clip_streak < UINT32_MAX) {
      string->bow_clip_streak++;
    }
    if (string->bow_clip_streak >= WG_DOUBLE_BASS_BOW_RECOVERY_FAILURES) {
      string->bow_clip_streak = 0U;
      string->bow_recovery_remaining = WG_DOUBLE_BASS_BOW_RECOVERY_SAMPLES;
      string->bow_recoveries = wg_double_bass_add_samples(
          string->bow_recoveries, 1U);
      string->bow_scratch_score = 1.0;
      wg_double_bass_clear_contact_response(string);
    }
  } else {
    string->bow_clip_streak = 0U;
  }
  toward_bridge = wg_double_bass_clamp(
      toward_bridge, -WG_DOUBLE_BASS_WAVE_LIMIT, WG_DOUBLE_BASS_WAVE_LIMIT);
  toward_nut = wg_double_bass_clamp(
      toward_nut, -WG_DOUBLE_BASS_WAVE_LIMIT, WG_DOUBLE_BASS_WAVE_LIMIT);
  second_toward_bridge = wg_double_bass_clamp(
      second_toward_bridge, -WG_DOUBLE_BASS_WAVE_LIMIT,
      WG_DOUBLE_BASS_WAVE_LIMIT);
  second_toward_nut = wg_double_bass_clamp(
      second_toward_nut, -WG_DOUBLE_BASS_WAVE_LIMIT,
      WG_DOUBLE_BASS_WAVE_LIMIT);
  string->toward_bridge[string->bridge_write_index] = toward_bridge;
  string->toward_nut[string->nut_write_index] = toward_nut;
  string->second_toward_bridge[string->bridge_write_index] =
      second_toward_bridge;
  string->second_toward_nut[string->nut_write_index] =
      second_toward_nut;
  output = 0.5 * (bridge_in - bridge_reflected);
  second_output = 0.5 *
      (second_bridge_in - second_bridge_reflected);
  output = polarization_primary * output +
      polarization_secondary * second_output;
  if (string->articulation < WG_DOUBLE_BASS_ARTICULATION_PIZZICATO) {
    string->bridge_dynamic_output = output - string->bridge_dynamic_input +
        string->bridge_dynamic_pole * string->bridge_dynamic_output;
    string->bridge_dynamic_input = output;
    output = string->bridge_dynamic_output;
  } else {
    string->bridge_dynamic_input = output;
    string->bridge_dynamic_output = 0.0;
  }
  if (!isfinite(output)) {
    wg_double_bass_zero_waveguide(string, 1);
    return 0.0;
  }
  string->last_bridge_output = wg_double_bass_clamp(
      output, -WG_DOUBLE_BASS_WAVE_LIMIT, WG_DOUBLE_BASS_WAVE_LIMIT);
  string->bridge_write_index++;
  if (string->bridge_write_index >= string->rail_size) {
    string->bridge_write_index = 0U;
  }
  string->nut_write_index++;
  if (string->nut_write_index >= string->rail_size) {
    string->nut_write_index = 0U;
  }
  string->wave_samples = wg_double_bass_add_samples(string->wave_samples, 1U);
  return string->last_bridge_output;
}

static void wg_double_bass_bridge_dynamic_fast_forward(
    WG_DOUBLE_BASS_STRING_STATE *string, uint64_t samples, double input_scale)
{
  double count;
  double pole;
  double pole_power;
  double input_ratio;
  double quotient;

  if (samples == 0U) {
    return;
  }
  if (string->articulation >= WG_DOUBLE_BASS_ARTICULATION_PIZZICATO) {
    string->bridge_dynamic_input *= input_scale;
    string->bridge_dynamic_output = 0.0;
    string->last_bridge_output *= input_scale;
    return;
  }
  count = (double)samples;
  pole = string->bridge_dynamic_pole;
  pole_power = pow(pole, count);
  input_ratio = input_scale > 0.0
      ? exp(log(input_scale) / count) : 0.0;
  if (fabs(pole - input_ratio) <=
      1.0e-12 * fmax(1.0, fmax(fabs(pole), fabs(input_ratio)))) {
    quotient = count * pow(pole, count - 1.0);
  } else {
    quotient = (pole_power - input_scale) / (pole - input_ratio);
  }
  string->bridge_dynamic_output =
      pole_power * string->bridge_dynamic_output +
      (input_ratio - 1.0) * string->bridge_dynamic_input * quotient;
  string->bridge_dynamic_input *= input_scale;
  if (!isfinite(string->bridge_dynamic_input) ||
      !isfinite(string->bridge_dynamic_output)) {
    string->bridge_dynamic_input = 0.0;
    string->bridge_dynamic_output = 0.0;
  }
  string->last_bridge_output = wg_double_bass_clamp(
      string->bridge_dynamic_output,
      -WG_DOUBLE_BASS_WAVE_LIMIT, WG_DOUBLE_BASS_WAVE_LIMIT);
}

static void wg_double_bass_advance_phase(double *phase, uint64_t samples,
                                    double rate, double sample_rate)
{
  double advanced;

  if (samples == 0U || !(sample_rate > 0.0) || !isfinite(rate)) {
    return;
  }
  advanced = *phase + (double)samples * rate / sample_rate;
  *phase = advanced - floor(advanced);
}

static void wg_double_bass_advance_string_gap(WG_DOUBLE_BASS_STRING_STATE *string,
                                         uint64_t start_sample,
                                         double sample_rate)
{
  const WG_DOUBLE_BASS_STRING_MODEL *string_model =
      wg_double_bass_bound_string_model(string);
  const double decay_seconds = string_model->loss_time_constant_seconds;
  const double second_decay_seconds =
      string_model->second_polarization.loss_time_constant_seconds;
  const double activity_decay_seconds =
      string_model->second_polarization.mix > 0.0
          ? fmax(decay_seconds, second_decay_seconds) : decay_seconds;
  uint64_t elapsed;
  uint64_t exact_samples;
  uint64_t remaining;
  uint64_t sample;
  uint64_t clear_samples;

  if (!string->last_update_valid || start_sample <= string->last_update_sample) {
    return;
  }
  elapsed = start_sample - string->last_update_sample;
  wg_double_bass_advance_phase(&string->phase, elapsed, string->frequency,
                          sample_rate);
  string->activity *= wg_double_bass_decay(
      elapsed, sample_rate, activity_decay_seconds);
  string->passive_open_level *= wg_double_bass_decay(
      elapsed, sample_rate, decay_seconds);
  if (string->passive_open_level <= 1.0e-9) {
    string->passive_open_level = 0.0;
    string->passive_open_active = 0;
  }
  wg_double_bass_release_bow(string);
  wg_double_bass_prepare_waveguide(
      string,
      string->harmonic_fundamental_frequency > 0.0
          ? string->harmonic_fundamental_frequency : string->frequency,
      sample_rate);
  clear_samples = (uint64_t)floor(
      sample_rate * WG_DOUBLE_BASS_GAP_CLEAR_SECONDS + 0.5);
  if (elapsed > clear_samples) {
    wg_double_bass_advance_phase(&string->vibrato_phase, elapsed,
                            string->vibrato_rate_hz, sample_rate);
    wg_double_bass_zero_waveguide(string, 0);
    string->wave_clears = wg_double_bass_add_samples(
        string->wave_clears, 1U);
    if (string->rail_size > 0U) {
      string->bridge_write_index = (uint32_t)(
          ((uint64_t)string->bridge_write_index + elapsed) %
          string->rail_size);
      string->nut_write_index = (uint32_t)(
          ((uint64_t)string->nut_write_index + elapsed) %
          string->rail_size);
      string->harmonic_write_index = (uint32_t)(elapsed % string->rail_size);
    }
    string->delay = string->target_delay;
    string->delay_velocity = 0.0;
    string->effective_frequency = string->target_frequency;
    string->nut_gain = string->nut_gain_target;
    string->bridge_gain = string->bridge_gain_target;
    string->second_nut_gain = string->second_nut_gain_target;
    string->second_bridge_gain = string->second_bridge_gain_target;
    string->finger_position = string->finger_target_position;
    string->finger_velocity = 0.0;
    string->finger_pressure = string->finger_pressure_target;
    string->finger_motion_kind = WG_DOUBLE_BASS_FINGER_STILL;
    string->finger_noise_envelope = 0.0;
    string->finger_last_noise = 0.0;
    string->harmonic_filter_order = string->harmonic_pending_order;
    string->harmonic_order_mix = 0.0;
    string->harmonic_touch_pressure = string->harmonic_filter_order != 0U
        ? wg_double_bass_harmonic_touch_level(
              string, string->harmonic_filter_order) : 0.0;
    string->harmonic_touch_target = string->harmonic_touch_pressure;
    wg_double_bass_split_delay(
        string->delay, string->bow_position,
        &string->bridge_delay, &string->nut_delay);
    string->second_delay = wg_double_bass_clamp(
        string->delay / exp(
            0.0005776226504666211 *
            string_model->second_polarization.detune_cents),
        2.0, (double)string->rail_size - 2.0);
    wg_double_bass_split_delay(
        string->second_delay, string->bow_position,
        &string->second_bridge_delay, &string->second_nut_delay);
    string->delay_initialized = 1;
    string->wave_samples = wg_double_bass_add_samples(
        string->wave_samples, elapsed);
  } else {
    const uint64_t exact_limit =
        (uint64_t)WG_DOUBLE_BASS_GAP_EXACT_RAILS * string->rail_size;
    exact_samples = elapsed < exact_limit ? elapsed : exact_limit;
    for (sample = 0U; sample < exact_samples; sample++) {
      IGN(wg_double_bass_waveguide_step(string, 0, sample_rate, 0.0));
    }
    remaining = elapsed - exact_samples;
    if (remaining > 0U) {
      double scale = wg_double_bass_decay(
          remaining, sample_rate, decay_seconds);
      double second_scale = wg_double_bass_decay(
          remaining, sample_rate, second_decay_seconds);
      const double maximum_delay_step =
          WG_DOUBLE_BASS_DELAY_SLEW * (double)remaining;
      const double gain_smoothing = 1.0 - pow(
          1.0 - string->string_loss_smoothing, (double)remaining);
      uint32_t index;

      wg_double_bass_bow_mechanics_fast_forward_gap(
          string, remaining, sample_rate);
      wg_double_bass_strange_fast_forward_gap(string, remaining);

      if (!string->release_gate_positive &&
          string->release_loop_gain_target < 1.0) {
        const double cycles = (double)remaining *
            string->target_frequency / sample_rate;

        scale *= pow(string->release_loop_gain_target, fmax(0.0, cycles));
        second_scale *= pow(
            string->release_loop_gain_target, fmax(0.0, cycles));
        string->release_loop_gain = string->release_loop_gain_target;
      }

      wg_double_bass_advance_phase(&string->vibrato_phase, remaining,
                              string->vibrato_rate_hz, sample_rate);
      for (index = 0U; index < string->rail_size; index++) {
        string->toward_bridge[index] *= scale;
        string->toward_nut[index] *= scale;
        string->second_toward_bridge[index] *= second_scale;
        string->second_toward_nut[index] *= second_scale;
        string->harmonic_history[index] = 0.0;
      }
      string->bridge_state *= scale;
      string->nut_state *= scale;
      string->second_bridge_state *= second_scale;
      string->second_nut_state *= second_scale;
      wg_double_bass_bridge_dynamic_fast_forward(string, remaining, scale);
      string->delay += wg_double_bass_clamp(
          string->target_delay - string->delay,
          -maximum_delay_step, maximum_delay_step);
      if (fabs(string->target_delay - string->delay) < 1.0e-12) {
        string->delay = string->target_delay;
        string->delay_velocity = 0.0;
      }
      string->effective_frequency = string->target_frequency;
      string->nut_gain = wg_double_bass_smooth(
          string->nut_gain, string->nut_gain_target, gain_smoothing);
      string->bridge_gain = wg_double_bass_smooth(
          string->bridge_gain, string->bridge_gain_target, gain_smoothing);
      string->second_nut_gain = wg_double_bass_smooth(
          string->second_nut_gain, string->second_nut_gain_target,
          gain_smoothing);
      string->second_bridge_gain = wg_double_bass_smooth(
          string->second_bridge_gain, string->second_bridge_gain_target,
          gain_smoothing);
      string->finger_position = string->finger_target_position;
      string->finger_velocity = 0.0;
      string->finger_pressure = string->finger_pressure_target;
      string->finger_motion_kind = WG_DOUBLE_BASS_FINGER_STILL;
      string->finger_noise_envelope *= pow(
          string->finger_noise_decay, (double)remaining);
      string->finger_last_noise = 0.0;
      string->harmonic_filter_order = string->harmonic_pending_order;
      string->harmonic_order_mix = 0.0;
      string->harmonic_touch_pressure =
          string->harmonic_filter_order != 0U
              ? wg_double_bass_harmonic_touch_level(
                    string, string->harmonic_filter_order) : 0.0;
      string->harmonic_touch_target = string->harmonic_touch_pressure;
      string->harmonic_projection_input = 0.0;
      string->harmonic_projection_output = 0.0;
      wg_double_bass_split_delay(
          string->delay, string->bow_position,
          &string->bridge_delay, &string->nut_delay);
      string->second_delay = wg_double_bass_clamp(
          string->delay / exp(
              0.0005776226504666211 *
              string_model->second_polarization.detune_cents),
          2.0, (double)string->rail_size - 2.0);
      wg_double_bass_split_delay(
          string->second_delay, string->bow_position,
          &string->second_bridge_delay, &string->second_nut_delay);
      if (string->rail_size > 0U) {
        string->bridge_write_index = (uint32_t)(
            ((uint64_t)string->bridge_write_index + remaining) %
            string->rail_size);
        string->nut_write_index = (uint32_t)(
            ((uint64_t)string->nut_write_index + remaining) %
            string->rail_size);
        string->harmonic_write_index = (uint32_t)(
            ((uint64_t)string->harmonic_write_index + remaining) %
            string->rail_size);
      }
      string->wave_samples = wg_double_bass_add_samples(
          string->wave_samples, remaining);
    }
  }
  string->gap_samples = wg_double_bass_add_samples(string->gap_samples, elapsed);
  string->last_update_sample = start_sample;
}

static void wg_double_bass_advance_renderer_gap(WG_DOUBLE_BASS_STATE *state,
                                           uint64_t start_sample)
{
  uint64_t elapsed;

  if (!state->last_render_valid || start_sample <= state->last_render_sample) {
    return;
  }
  elapsed = start_sample - state->last_render_sample;
  wg_double_bass_advance_body_gap(state, elapsed);
  state->resonance_activity *= wg_double_bass_decay(
      elapsed, state->sample_rate, WG_DOUBLE_BASS_RENDERER_DECAY_SECONDS);
  state->renderer_gap_samples = wg_double_bass_add_samples(
      state->renderer_gap_samples, elapsed);
  state->last_render_sample = start_sample;
}

static int32_t wg_double_bass_instance_precedes(OPDS *first, OPDS *second)
{
  INSDS *instance;

  if (first == NULL || second == NULL || first->insdshead == NULL ||
      second->insdshead == NULL) {
    return 0;
  }
  for (instance = first->insdshead; instance != NULL;
       instance = instance->nxtact) {
    if (instance == second->insdshead) {
      return 1;
    }
  }
  return 0;
}

static int32_t wg_double_bass_capture_instance_span(
    CSOUND *csound, OPDS *opcode, WG_DOUBLE_BASS_INSTANCE_SPAN *span)
{
  const double sample_rate = (double)csound->GetEngineSr(csound);
  const int64_t current_sample = csound->GetCurrentTimeSamples(csound);
  INSDS *instance = opcode != NULL ? opcode->insdshead : NULL;
  double duration;
  uint64_t start_sample;
  uint64_t duration_samples;

  if (span == NULL) {
    return 0;
  }
  memset(span, 0, sizeof(*span));
  if (instance == NULL || current_sample < 0 ||
      instance->ksmps_offset >= instance->ksmps ||
      !isfinite(sample_rate) || sample_rate <= 0.0) {
    return 0;
  }
  duration = (double)instance->p3.value * sample_rate;
  if (!isfinite(duration) || duration < 0.0 ||
      duration > (double)UINT64_MAX) {
    return 0;
  }
  start_sample = wg_double_bass_add_samples(
      (uint64_t)current_sample, instance->ksmps_offset);
  duration_samples = (uint64_t)duration;
  if (start_sample > UINT64_MAX - duration_samples) {
    return 0;
  }
  span->start_sample = start_sample;
  span->end_sample = start_sample + duration_samples;
  span->valid = 1;
  return 1;
}

static int32_t wg_double_bass_instances_can_queue(CSOUND *csound,
                                             OPDS *first,
                                             const WG_DOUBLE_BASS_INSTANCE_SPAN *
                                                 first_span,
                                             OPDS *second,
                                             const WG_DOUBLE_BASS_INSTANCE_SPAN *
                                                 second_span)
{
  const OPARMS *options = csound->GetOParms(csound);
  INSDS *first_instance = first != NULL ? first->insdshead : NULL;
  INSDS *second_instance = second != NULL ? second->insdshead : NULL;

  if (options == NULL || !options->sampleAccurate || options->realtime ||
      options->numThreads != 1 ||
      first_instance == NULL || second_instance == NULL ||
      first_span == NULL || second_span == NULL ||
      !first_span->valid || !second_span->valid ||
      first_instance->instr != second_instance->instr ||
      first_instance->p1.value != second_instance->p1.value ||
      first_instance->ksmps != second_instance->ksmps ||
      first_instance->esr != second_instance->esr ||
      first_instance->xtratim != 0 || first_instance->p3.value <= FL(0.0) ||
      first_instance->no_end >= first_instance->ksmps ||
      second_instance->ksmps_offset >= second_instance->ksmps ||
      !wg_double_bass_instance_precedes(first, second)) {
    return 0;
  }
  return first_span->end_sample <= second_span->start_sample;
}

#if !defined(__wasi__)
static int32_t wg_double_bass_manager_reset(CSOUND *csound, void *user_data)
{
  WG_DOUBLE_BASS_MANAGER **slot;
  WG_DOUBLE_BASS_MANAGER *manager;
  WG_DOUBLE_BASS_STATE *state;

  IGN(user_data);
  slot = (WG_DOUBLE_BASS_MANAGER **)csound->QueryGlobalVariable(
      csound, WG_DOUBLE_BASS_MANAGER_NAME);
  if (slot == NULL || *slot == NULL) {
    return OK;
  }
  manager = *slot;
  state = manager->first;
  while (state != NULL) {
    uint32_t string_index;
    WG_DOUBLE_BASS_STATE *next = state->next;
    if (state->state_lock != NULL) {
      csound->DestroyMutex(state->state_lock);
    }
    if (state->resonance_lock != NULL) {
      csound->DestroyMutex(state->resonance_lock);
    }
    for (string_index = 0U; string_index < WG_DOUBLE_BASS_STRINGS;
         string_index++) {
      if (state->strings[string_index].lock != NULL) {
        csound->DestroyMutex(state->strings[string_index].lock);
      }
    }
    if (state->rail_memory != NULL) {
      csound->Free(csound, state->rail_memory);
    }
    if (state->bridge_memory != NULL) {
      csound->Free(csound, state->bridge_memory);
    }
    csound->Free(csound, state);
    state = next;
  }
  if (manager->lock != NULL) {
    csound->DestroyMutex(manager->lock);
  }
  csound->Free(csound, manager);
  *slot = NULL;
  csound->DestroyGlobalVariable(csound, WG_DOUBLE_BASS_MANAGER_NAME);
  return OK;
}
#endif

static WG_DOUBLE_BASS_MANAGER *wg_double_bass_get_manager(CSOUND *csound)
{
  /* Csound serializes opcode init, including this one-time setup. */
  WG_DOUBLE_BASS_MANAGER **slot =
      (WG_DOUBLE_BASS_MANAGER **)csound->QueryGlobalVariable(
          csound, WG_DOUBLE_BASS_MANAGER_NAME);
  const WG_DOUBLE_BASS_MODEL *model = wg_double_bass_fixed_model();
  WG_DOUBLE_BASS_MANAGER *manager;
  uint32_t friction_index;

  if (slot != NULL && *slot != NULL) {
    return *slot;
  }
  if (!wg_double_bass_model_is_valid(model)) {
    return NULL;
  }
  if (slot == NULL &&
      csound->CreateGlobalVariable(csound, WG_DOUBLE_BASS_MANAGER_NAME,
                                   sizeof(WG_DOUBLE_BASS_MANAGER *)) != OK) {
    return NULL;
  }
  slot = (WG_DOUBLE_BASS_MANAGER **)csound->QueryGlobalVariable(
      csound, WG_DOUBLE_BASS_MANAGER_NAME);
  if (slot == NULL) {
    return NULL;
  }
  manager = (WG_DOUBLE_BASS_MANAGER *)csound->Calloc(
      csound, sizeof(WG_DOUBLE_BASS_MANAGER));
  if (manager == NULL) {
    csound->DestroyGlobalVariable(csound, WG_DOUBLE_BASS_MANAGER_NAME);
    return NULL;
  }
  {
    double previous_mu = HUGE_VAL;
    for (friction_index = 0U;
         friction_index < WG_DOUBLE_BASS_FRICTION_LUT_SIZE; friction_index++) {
      const double speed = WG_DOUBLE_BASS_FRICTION_MAX_SPEED *
          (double)friction_index /
          (double)(WG_DOUBLE_BASS_FRICTION_LUT_SIZE - 1U);
      const double mu = wg_double_bass_model_friction_mu(model, speed);

      if (!isfinite(mu) || mu <= 0.0 || mu > previous_mu) {
        csound->Free(csound, manager);
        csound->DestroyGlobalVariable(csound, WG_DOUBLE_BASS_MANAGER_NAME);
        return NULL;
      }
      manager->friction_lut[friction_index] = mu;
      previous_mu = mu;
    }
  }
  manager->next_handle = 1;
  manager->lock = csound->Create_Mutex(0);
  if (WG_DOUBLE_BASS_REQUIRE_MUTEXES && manager->lock == NULL) {
    csound->Free(csound, manager);
    csound->DestroyGlobalVariable(csound, WG_DOUBLE_BASS_MANAGER_NAME);
    return NULL;
  }
  *slot = manager;
#if defined(__wasi__)
  /* Csound owns the named global and tracked allocations in WASI builds. */
#else
  if (csound->RegisterResetCallback(csound, manager,
                                    wg_double_bass_manager_reset) != OK) {
    wg_double_bass_manager_reset(csound, NULL);
    return NULL;
  }
#endif
  return manager;
}

static WG_DOUBLE_BASS_STATE *wg_double_bass_find_locked(
    WG_DOUBLE_BASS_MANAGER *manager, int32_t handle)
{
  WG_DOUBLE_BASS_STATE *state = manager->first;
  while (state != NULL && state->handle != handle) {
    state = state->next;
  }
  return state;
}

static WG_DOUBLE_BASS_STATE *wg_double_bass_get_by_handle(
    CSOUND *csound, int32_t handle)
{
  WG_DOUBLE_BASS_MANAGER *manager = wg_double_bass_get_manager(csound);
  WG_DOUBLE_BASS_STATE *state;

  if (manager == NULL) {
    return NULL;
  }
  csound->LockMutex(manager->lock);
  state = wg_double_bass_find_locked(manager, handle);
  csound->UnlockMutex(manager->lock);
  return state;
}

static WG_DOUBLE_BASS_STATE *wg_double_bass_allocate_state(
    CSOUND *csound, WG_DOUBLE_BASS_MANAGER *manager, int32_t handle,
    uint32_t ksmps, double reference_pitch_hz)
{
  const WG_DOUBLE_BASS_MODEL *model = wg_double_bass_fixed_model();
  WG_DOUBLE_BASS_STATE *state;
  const double sample_rate = (double)csound->GetEngineSr(csound);
  const double minimum_open_frequency = wg_double_bass_scaled_pitch(
      wg_double_bass_a440_open_frequencies[0], reference_pitch_hz);
  const double normal_max_frequency = wg_double_bass_scaled_pitch(
      WG_DOUBLE_BASS_A440_NORMAL_MAX_FREQUENCY, reference_pitch_hz);
  const double sounded_max_frequency = fmin(
      (double)WG_DOUBLE_BASS_HARMONIC_MAX_ORDER * normal_max_frequency,
      WG_DOUBLE_BASS_MAX_FREQUENCY_RATIO * sample_rate);
  double required_size;
  size_t total_values;
  size_t bridge_values;
  size_t allocation_bytes;
  uint32_t string_index;
  int32_t locks_ok;

  if (manager == NULL) {
    return NULL;
  }
  state = (WG_DOUBLE_BASS_STATE *)csound->Calloc(
      csound, sizeof(WG_DOUBLE_BASS_STATE));
  if (state == NULL) {
    return NULL;
  }
  required_size = ceil(
      sample_rate / minimum_open_frequency) +
      (double)WG_DOUBLE_BASS_RAIL_MARGIN;
  if (!isfinite(sample_rate) || sample_rate <= 0.0 ||
      !isfinite(minimum_open_frequency) || minimum_open_frequency <= 0.0 ||
      !isfinite(normal_max_frequency) ||
      normal_max_frequency < minimum_open_frequency ||
      !isfinite(sounded_max_frequency) ||
      sounded_max_frequency < minimum_open_frequency ||
      !isfinite(required_size) || required_size < 8.0 ||
      required_size > (double)UINT32_MAX) {
    csound->Free(csound, state);
    return NULL;
  }
  state->rail_size = (uint32_t)required_size;
  if (!wg_double_bass_checked_size_multiply(
          (size_t)state->rail_size,
          (size_t)(5U * WG_DOUBLE_BASS_STRINGS), &total_values) ||
      !wg_double_bass_checked_size_multiply(
          total_values, sizeof(double), &allocation_bytes)) {
    csound->Free(csound, state);
    return NULL;
  }
  state->rail_memory = (double *)csound->Calloc(
      csound, allocation_bytes);
  if (state->rail_memory == NULL) {
    csound->Free(csound, state);
    return NULL;
  }
  if (!wg_double_bass_checked_size_multiply(
          (size_t)ksmps,
          (size_t)(5U * WG_DOUBLE_BASS_STRINGS + 2U), &bridge_values) ||
      !wg_double_bass_checked_size_multiply(
          bridge_values, sizeof(double), &allocation_bytes)) {
    csound->Free(csound, state->rail_memory);
    csound->Free(csound, state);
    return NULL;
  }
  state->bridge_memory = (double *)csound->Calloc(
      csound, allocation_bytes);
  if (state->bridge_memory == NULL) {
    csound->Free(csound, state->rail_memory);
    csound->Free(csound, state);
    return NULL;
  }
  state->body_input = state->bridge_memory +
      (size_t)ksmps * 2U * WG_DOUBLE_BASS_STRINGS;
  state->sympathetic_control[0] = state->body_input +
      (size_t)ksmps * WG_DOUBLE_BASS_STRINGS;
  state->sympathetic_control[1] = state->sympathetic_control[0] + ksmps;
  state->handle = handle;
  state->model = model;
  state->ksmps = ksmps;
  state->sample_rate = sample_rate;
  state->reference_pitch_hz = reference_pitch_hz;
  state->normal_max_frequency = normal_max_frequency;
  state->sounded_max_frequency = sounded_max_frequency;
  state->body = 0.72;
  /* No renderer block has supplied this control yet. */
  state->sympathetic = 0.0;
  state->mute = 0.0;
  state->body_gain_smoothing = wg_double_bass_smoothing_coefficient(
      model->body.gain_smoothing_seconds, sample_rate);
  wg_double_bass_initialize_body(state);
  wg_double_bass_prepare_body(state);
  state->body_wet_level = state->body_wet_level_target;
  for (string_index = 0U; string_index < WG_DOUBLE_BASS_BODY_MODES;
       string_index++) {
    state->body_modes[string_index].gain =
        state->body_modes[string_index].target_gain;
  }
  state->state_lock = csound->Create_Mutex(0);
  state->resonance_lock = csound->Create_Mutex(0);
  for (string_index = 0U; string_index < WG_DOUBLE_BASS_STRINGS;
       string_index++) {
    WG_DOUBLE_BASS_STRING_STATE *string = &state->strings[string_index];
    const WG_DOUBLE_BASS_STRING_MODEL *string_model =
        &model->strings[string_index];
    double *memory = state->rail_memory +
        (size_t)string_index * 5U * state->rail_size;
    string->model = model;
    string->string_index = string_index;
    string->toward_bridge = memory;
    string->toward_nut = memory + state->rail_size;
    string->second_toward_bridge = memory + 2U * state->rail_size;
    string->second_toward_nut = memory + 3U * state->rail_size;
    string->harmonic_history = memory + 4U * state->rail_size;
    string->bridge_send[0] = state->bridge_memory +
        (size_t)string_index * ksmps;
    string->bridge_send[1] = state->bridge_memory +
        ((size_t)WG_DOUBLE_BASS_STRINGS + string_index) * ksmps;
    string->strange_coupling_send[0] = state->sympathetic_control[1] +
        ksmps + (size_t)string_index * ksmps;
    string->strange_coupling_send[1] =
        state->sympathetic_control[1] + ksmps +
        ((size_t)WG_DOUBLE_BASS_STRINGS + string_index) * ksmps;
    string->friction_lut = manager->friction_lut;
    string->rail_size = state->rail_size;
    string->position = 0.12;
    string->force = 0.45;
    string->speed = 0.55;
    string->bow_position_target = 0.12;
    string->bow_position = 0.12;
    string->open_frequency = wg_double_bass_scaled_pitch(
        wg_double_bass_a440_open_frequencies[string_index], reference_pitch_hz);
    string->delay_velocity_smoothing = wg_double_bass_smoothing_coefficient(
        WG_DOUBLE_BASS_DELAY_ACCEL_SECONDS, sample_rate);
    string->string_loss_smoothing = wg_double_bass_smoothing_coefficient(
        0.004, sample_rate);
    string->release_loop_gain = 1.0;
    string->release_loop_gain_target = 1.0;
    string->release_gain_smoothing = wg_double_bass_smoothing_coefficient(
        WG_DOUBLE_BASS_RELEASE_ENGAGE_SECONDS, sample_rate);
    string->release_model_speed = string->speed;
    string->bridge_dynamic_pole = exp(
        -WG_DOUBLE_BASS_TWO_PI * WG_DOUBLE_BASS_BRIDGE_DYNAMIC_CUTOFF_HZ /
        sample_rate);
    string->finger_position_smoothing = wg_double_bass_smoothing_coefficient(
        0.003, sample_rate);
    string->finger_pressure_attack_smoothing =
        wg_double_bass_smoothing_coefficient(0.001, sample_rate);
    string->finger_pressure_release_smoothing =
        wg_double_bass_smoothing_coefficient(0.0025, sample_rate);
    string->harmonic_touch_attack_smoothing =
        wg_double_bass_smoothing_coefficient(
            model->harmonics.attack_seconds, sample_rate);
    string->harmonic_touch_release_smoothing =
        wg_double_bass_smoothing_coefficient(
            model->harmonics.release_seconds, sample_rate);
    string->harmonic_order_mix_step = 1.0 / fmax(
        1.0, model->harmonics.order_transition_seconds * sample_rate);
    string->harmonic_valid = 1;
    string->finger_noise_decay = exp(
        -1.0 / fmax(1.0, 0.0015 * sample_rate));
    string->finger_rng = 0x9e3779b9U ^
        (uint32_t)handle * 0x85ebca6bU ^
        (string_index + 1U) * 0xc2b2ae35U;
    if (string->finger_rng == 0U) {
      string->finger_rng = 0x6d2b79f5U;
    }
    string->exciter_rng = 0x243f6a88U ^
        (uint32_t)handle * 0x9e3779b9U ^
        (string_index + 1U) * 0x85ebca6bU;
    if (string->exciter_rng == 0U) {
      string->exciter_rng = 0xa341316cU;
    }
    string->exciter_position = string->position;
    string->exciter_contact_attack_smoothing =
        wg_double_bass_smoothing_coefficient(
            model->exciters.contact_attack_seconds, sample_rate);
    string->exciter_contact_release_smoothing =
        wg_double_bass_smoothing_coefficient(
            model->exciters.contact_release_seconds, sample_rate);
    string->exciter_speed_smoothing =
        wg_double_bass_smoothing_coefficient(
            model->exciters.speed_smooth_seconds, sample_rate);
    string->exciter_armed = 1;
    string->characteristic_impedance =
        string_model->characteristic_impedance;
    string->bow_speed_smoothing = wg_double_bass_smoothing_coefficient(
        0.002, sample_rate);
    string->bow_contact_attack_smoothing =
        wg_double_bass_smoothing_coefficient(0.0015, sample_rate);
    string->bow_contact_release_smoothing =
        wg_double_bass_smoothing_coefficient(0.006, sample_rate);
    string->bow_position_smoothing = wg_double_bass_smoothing_coefficient(
        0.005, sample_rate);
    string->bow_scratch_rise = wg_double_bass_smoothing_coefficient(
        0.002, sample_rate);
    string->bow_scratch_fall = wg_double_bass_smoothing_coefficient(
        0.030, sample_rate);
    string->contact_thermal_attack = wg_double_bass_smoothing_coefficient(
        model->bow.contact.thermal_attack_seconds, sample_rate);
    string->contact_thermal_release = wg_double_bass_smoothing_coefficient(
        model->bow.contact.thermal_release_seconds, sample_rate);
    string->contact_memory_release = wg_double_bass_smoothing_coefficient(
        model->bow.contact.memory_release_seconds, sample_rate);
    string->mechanical_decay = exp(
        -1.0 / fmax(1.0, 0.0045 * sample_rate));
    string->board_active_decay =
        1.0 - 1.0 / fmax(1.0, 0.080 * sample_rate);
    string->board_inactive_decay =
        1.0 - 1.0 / fmax(1.0, 0.006 * sample_rate);
#if defined(HLOLLI_WG_DOUBLE_BASS_TEST_API)
    string->diagnostic_finger_gain = 1.0;
    string->diagnostic_board_gain = 1.0;
    string->diagnostic_rosin_gain = 1.0;
    string->diagnostic_mechanical_gain = 1.0;
    string->diagnostic_nut_cutoff_scale = 1.0;
    string->diagnostic_bridge_cutoff_scale = 1.0;
#endif
    string->gesture_transition_step = 1.0 / fmax(
        1.0, model->gestures.transition_seconds * sample_rate);
    string->gesture_bow_change_step = 1.0 / fmax(
        1.0, model->gestures.bow_change_seconds * sample_rate);
    wg_double_bass_reset_gesture_live(string);
    wg_double_bass_reset_bow_mechanics(string);
    wg_double_bass_reset_strange_live(string);
    string->bow_noise_rng = 0xb7e15162U ^
        (uint32_t)handle * 0x9e3779b9U ^
        (string_index + 1U) * 0x85ebca6bU;
    if (string->bow_noise_rng == 0U) {
      string->bow_noise_rng = 0x8aed2a6bU;
    }
    string->strange_rng = 0x3c6ef372U ^
        (uint32_t)handle * 0x27d4eb2dU ^
        (string_index + 1U) * 0x165667b1U;
    if (string->strange_rng == 0U) {
      string->strange_rng = 0x1b873593U;
    }
    string->trigger_armed = 1;
    state->strings[string_index].lock = csound->Create_Mutex(0);
  }
  for (string_index = 0U; string_index < WG_DOUBLE_BASS_STRINGS;
       string_index++) {
    uint32_t source_index;

    for (source_index = 0U; source_index < WG_DOUBLE_BASS_STRINGS;
         source_index++) {
      state->bridge_coupling_gain[string_index][source_index] =
          string_index == source_index ? 0.0 :
          model->coupling.bridge_coefficients
              [string_index][source_index] /
              (2.0 * sqrt(
                  state->strings[string_index].characteristic_impedance *
                  state->strings[source_index].characteristic_impedance));
    }
  }
  locks_ok = state->state_lock != NULL && state->resonance_lock != NULL;
  for (string_index = 0U; string_index < WG_DOUBLE_BASS_STRINGS;
       string_index++) {
    if (state->strings[string_index].lock == NULL) {
      locks_ok = 0;
    }
  }
  if (WG_DOUBLE_BASS_REQUIRE_MUTEXES && !locks_ok) {
    if (state->state_lock != NULL) {
      csound->DestroyMutex(state->state_lock);
    }
    if (state->resonance_lock != NULL) {
      csound->DestroyMutex(state->resonance_lock);
    }
    for (string_index = 0U; string_index < WG_DOUBLE_BASS_STRINGS;
         string_index++) {
      if (state->strings[string_index].lock != NULL) {
        csound->DestroyMutex(state->strings[string_index].lock);
      }
    }
    csound->Free(csound, state->rail_memory);
    csound->Free(csound, state->bridge_memory);
    csound->Free(csound, state);
    return NULL;
  }
  return state;
}

static int32_t wg_double_bass_create(
    CSOUND *csound, OPDS *h, MYFLT *out_handle, double reference_pitch_hz)
{
  WG_DOUBLE_BASS_MANAGER *manager = wg_double_bass_get_manager(csound);
  WG_DOUBLE_BASS_STATE *state;
  const uint32_t engine_ksmps = wg_double_bass_engine_ksmps(csound);
  int32_t handle;

  if (UNLIKELY(manager == NULL)) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_create: cannot allocate manager\n");
  }
  if (UNLIKELY(!isfinite(reference_pitch_hz) ||
               reference_pitch_hz < WG_DOUBLE_BASS_MIN_REFERENCE_PITCH_HZ ||
               reference_pitch_hz > WG_DOUBLE_BASS_MAX_REFERENCE_PITCH_HZ)) {
    return csound->InitError(
        csound,
        "hlolli_wg_double_bass_create: reference pitch must be from 380 to 480 Hz\n");
  }
  if (UNLIKELY(engine_ksmps == 0U)) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_create: invalid engine ksmps\n");
  }
  if (UNLIKELY(h->insdshead->ksmps != engine_ksmps)) {
    return csound->InitError(
        csound,
        "hlolli_wg_double_bass_create: handles require engine ksmps %u\n",
        engine_ksmps);
  }
  csound->LockMutex(manager->lock);
  handle = manager->next_handle;
  if (handle <= 0 || handle > WG_DOUBLE_BASS_MAX_HANDLE) {
    csound->UnlockMutex(manager->lock);
    return csound->InitError(
        csound, "hlolli_wg_double_bass_create: no handles remain\n");
  }
  state = wg_double_bass_allocate_state(
      csound, manager, handle, engine_ksmps, reference_pitch_hz);
  if (state == NULL) {
    csound->UnlockMutex(manager->lock);
    return csound->InitError(
        csound, "hlolli_wg_double_bass_create: cannot allocate double-bass state\n");
  }
  state->next = manager->first;
  manager->first = state;
  manager->next_handle++;
  csound->UnlockMutex(manager->lock);
  *out_handle = (MYFLT)handle;
  return OK;
}

static int32_t wg_double_bass_create_init(
    CSOUND *csound, WG_DOUBLE_BASS_CREATE *p)
{
  return wg_double_bass_create(
      csound, &p->h, p->handle, WG_DOUBLE_BASS_DEFAULT_REFERENCE_PITCH_HZ);
}

static int32_t wg_double_bass_create_tuned_init(
    CSOUND *csound, WG_DOUBLE_BASS_CREATE_TUNED *p)
{
  const double reference_pitch_hz = p->reference_pitch_hz != NULL
      ? (double)*p->reference_pitch_hz : WG_DOUBLE_BASS_NAN;

  return wg_double_bass_create(
      csound, &p->h, p->handle, reference_pitch_hz);
}

static uint32_t wg_double_bass_preferred_string(
    const WG_DOUBLE_BASS_STATE *state, double frequency)
{
  uint32_t string_index;

  for (string_index = WG_DOUBLE_BASS_STRINGS; string_index > 1U;
       string_index--) {
    if (wg_double_bass_string_can_play(state, string_index - 1U, frequency)) {
      return string_index - 1U;
    }
  }
  return 0U;
}

static WG_DOUBLE_BASS_VOICE *wg_double_bass_voice_from_opds(OPDS *owner)
{
  return (WG_DOUBLE_BASS_VOICE *)owner;
}

static uint32_t wg_double_bass_voice_articulation(const WG_DOUBLE_BASS_VOICE *voice)
{
  const double articulation = wg_double_bass_input(voice->karticulation, 0.0);

  return (uint32_t)floor(wg_double_bass_clamp(
      articulation, 0.0, 15.0) + 0.5);
}

static double wg_double_bass_voice_selection_frequency(
    const WG_DOUBLE_BASS_STATE *state, const WG_DOUBLE_BASS_VOICE *voice,
    double sounding_frequency)
{
  const double harmonic_request = voice->kharmonic != NULL
      ? (double)*voice->kharmonic : 0.0;
  double fundamental = sounding_frequency;

  return wg_double_bass_pitch_request_is_valid(
      state, sounding_frequency, harmonic_request, NULL, &fundamental)
      ? fundamental : sounding_frequency;
}

static int32_t wg_double_bass_voice_holds_continuous(
    const WG_DOUBLE_BASS_VOICE *voice)
{
  const uint32_t articulation = wg_double_bass_voice_articulation(voice);

  return wg_double_bass_input(voice->ktrigger, 0.0) > 1.0e-7 &&
      (articulation == WG_DOUBLE_BASS_ARTICULATION_ARCO ||
       articulation == WG_DOUBLE_BASS_ARTICULATION_TREMOLO ||
       articulation == WG_DOUBLE_BASS_ARTICULATION_TRATTO);
}

static int32_t wg_double_bass_last_controller_forms_continuous(
    const WG_DOUBLE_BASS_STRING_STATE *string,
    const WG_DOUBLE_BASS_VOICE *next)
{
  const uint32_t next_articulation = wg_double_bass_voice_articulation(next);
  return string->last_controller_valid && next->span.valid &&
      string->last_controller_end_sample == next->span.start_sample &&
      string->last_controller_continuous &&
      string->last_controller_articulation == next_articulation &&
      wg_double_bass_voice_holds_continuous(next);
}

static int32_t wg_double_bass_voices_form_continuous(
    const WG_DOUBLE_BASS_VOICE *current,
    const WG_DOUBLE_BASS_VOICE *next)
{
  const uint32_t current_articulation =
      wg_double_bass_voice_articulation(current);
  const uint32_t next_articulation = wg_double_bass_voice_articulation(next);
  return current->span.valid && next->span.valid &&
      current->span.end_sample == next->span.start_sample &&
      current_articulation == next_articulation &&
      wg_double_bass_voice_holds_continuous(current) &&
      wg_double_bass_voice_holds_continuous(next);
}

static int32_t wg_double_bass_predecessor_forms_continuous(
    const WG_DOUBLE_BASS_VOICE *voice)
{
  const uint32_t articulation = wg_double_bass_voice_articulation(voice);

  return voice->predecessor_valid && voice->span.valid &&
      voice->predecessor_end_sample == voice->span.start_sample &&
      voice->predecessor_continuous &&
      voice->predecessor_articulation == articulation &&
      wg_double_bass_voice_holds_continuous(voice);
}

static uint32_t wg_double_bass_nonfinite_fallback_string(
    CSOUND *csound, WG_DOUBLE_BASS_VOICE *voice)
{
  WG_DOUBLE_BASS_STATE *state = voice->double_bass;
  uint32_t string_index;

  if (voice->owns_string && voice->string_index < WG_DOUBLE_BASS_STRINGS) {
    return voice->string_index;
  }
  if (state == NULL || !voice->automatic_string) {
    return 0U;
  }
  csound->LockMutex(state->state_lock);
  for (string_index = 0U; string_index < WG_DOUBLE_BASS_STRINGS;
       string_index++) {
    WG_DOUBLE_BASS_STRING_STATE *string = &state->strings[string_index];
    WG_DOUBLE_BASS_VOICE *tail =
        wg_double_bass_voice_from_opds(string->controller_tail);

    if (tail != NULL && tail->automatic_string &&
        wg_double_bass_voices_form_continuous(tail, voice)) {
      csound->UnlockMutex(state->state_lock);
      return string_index;
    }
  }
  for (string_index = 0U; string_index < WG_DOUBLE_BASS_STRINGS;
       string_index++) {
    WG_DOUBLE_BASS_STRING_STATE *string = &state->strings[string_index];

    if (string->last_controller_automatic &&
        wg_double_bass_last_controller_forms_continuous(string, voice)) {
      csound->UnlockMutex(state->state_lock);
      return string_index;
    }
  }
  csound->UnlockMutex(state->state_lock);
  return 0U;
}

static double wg_double_bass_voice_sounding_frequency(
    CSOUND *csound, WG_DOUBLE_BASS_VOICE *voice, double harmonic_request)
{
  const double requested = voice->kfrequency != NULL
      ? (double)*voice->kfrequency : WG_DOUBLE_BASS_NAN;
  double sounding_frequency;
  uint32_t string_index;
  uint32_t order;

  if (isfinite(requested)) {
    sounding_frequency = requested;
  } else {
    string_index = wg_double_bass_nonfinite_fallback_string(csound, voice);
    order = wg_double_bass_harmonic_order(harmonic_request);
    sounding_frequency =
        voice->double_bass->strings[string_index].open_frequency *
        (order != 0U ? (double)order : 1.0);
  }
  return wg_double_bass_canonical_sounding_frequency(
      voice->double_bass, sounding_frequency, harmonic_request);
}

static int32_t wg_double_bass_choose_auto_string(
    CSOUND *csound, WG_DOUBLE_BASS_STATE *state, WG_DOUBLE_BASS_VOICE *voice,
    double frequency, uint32_t *chosen)
{
  const uint32_t preferred = wg_double_bass_preferred_string(state, frequency);
  double best_jump = HUGE_VAL;
  uint32_t best = WG_DOUBLE_BASS_STRINGS;
  uint32_t string_index;

  if (!wg_double_bass_string_can_play(state, 0U, frequency)) {
    return -1;
  }
  for (string_index = 0U; string_index < WG_DOUBLE_BASS_STRINGS;
       string_index++) {
    WG_DOUBLE_BASS_STRING_STATE *string = &state->strings[string_index];
    WG_DOUBLE_BASS_VOICE *tail;
    double tail_harmonic_request;
    double tail_frequency;
    double jump;

    tail = wg_double_bass_voice_from_opds(string->controller_tail);
    if (!wg_double_bass_string_can_play(state, string_index, frequency) ||
        string->owner_serial == 0U || tail == NULL ||
        !wg_double_bass_instances_can_queue(
            csound, string->controller_tail, &tail->span,
            &voice->h, &voice->span)) {
      continue;
    }
    if (!tail->automatic_string ||
        !wg_double_bass_voices_form_continuous(tail, voice)) {
      continue;
    }
    tail_harmonic_request = tail->kharmonic != NULL
        ? (double)*tail->kharmonic : 0.0;
    tail_frequency = wg_double_bass_voice_sounding_frequency(
        csound, tail, tail_harmonic_request);
    tail_frequency = wg_double_bass_voice_selection_frequency(
        state, tail, tail_frequency);
    jump = fabs(log(frequency / tail_frequency));
    if (jump < best_jump - 1.0e-15 ||
        (fabs(jump - best_jump) <= 1.0e-15 &&
         abs((int32_t)string_index - (int32_t)preferred) <
             abs((int32_t)best - (int32_t)preferred))) {
      best = string_index;
      best_jump = jump;
    }
  }
  for (string_index = 0U; string_index < WG_DOUBLE_BASS_STRINGS;
       string_index++) {
    WG_DOUBLE_BASS_STRING_STATE *string = &state->strings[string_index];
    double jump;

    if (!wg_double_bass_string_can_play(state, string_index, frequency) ||
        string->owner_serial != 0U ||
        !string->last_controller_automatic ||
        !wg_double_bass_last_controller_forms_continuous(string, voice)) {
      continue;
    }
    jump = fabs(log(frequency / string->last_controller_frequency));
    if (jump < best_jump - 1.0e-15 ||
        (fabs(jump - best_jump) <= 1.0e-15 &&
         abs((int32_t)string_index - (int32_t)preferred) <
             abs((int32_t)best - (int32_t)preferred))) {
      best = string_index;
      best_jump = jump;
    }
  }
  if (best < WG_DOUBLE_BASS_STRINGS) {
    *chosen = best;
    return 1;
  }
  for (string_index = preferred + 1U; string_index > 0U; string_index--) {
    const uint32_t candidate = string_index - 1U;
    WG_DOUBLE_BASS_STRING_STATE *string = &state->strings[candidate];
    WG_DOUBLE_BASS_VOICE *tail =
        wg_double_bass_voice_from_opds(string->controller_tail);

    if (!wg_double_bass_string_can_play(state, candidate, frequency)) {
      continue;
    }
    if (string->owner_serial == 0U ||
        (tail != NULL && wg_double_bass_instances_can_queue(
            csound, &tail->h, &tail->span, &voice->h, &voice->span))) {
      *chosen = candidate;
      return 1;
    }
  }
  return 0;
}

static void wg_double_bass_promote_controller(
    WG_DOUBLE_BASS_STRING_STATE *string, WG_DOUBLE_BASS_VOICE *current)
{
  OPDS *next = current->handoff_next;

  if (next != NULL) {
    WG_DOUBLE_BASS_VOICE *next_voice = wg_double_bass_voice_from_opds(next);
    next_voice->predecessor_end_sample = current->span.end_sample;
    next_voice->predecessor_articulation =
        wg_double_bass_voice_articulation(current);
    next_voice->predecessor_continuous =
        wg_double_bass_voice_holds_continuous(current);
    next_voice->predecessor_valid = current->span.valid;
  }
  current->handoff_next = NULL;
  current->handed_off = 1;
  string->controller_owner = next;
  if (next != NULL) {
    string->owner_serial = wg_double_bass_voice_from_opds(next)->voice_serial;
  } else {
    string->controller_tail = NULL;
    string->owner_serial = 0U;
  }
}

static void wg_double_bass_unlink_controller(
    WG_DOUBLE_BASS_STRING_STATE *string, WG_DOUBLE_BASS_VOICE *voice)
{
  WG_DOUBLE_BASS_VOICE *previous =
      wg_double_bass_voice_from_opds(string->controller_owner);

  while (previous != NULL && previous->handoff_next != &voice->h) {
    previous = wg_double_bass_voice_from_opds(previous->handoff_next);
  }
  if (previous == NULL) {
    return;
  }
  previous->handoff_next = voice->handoff_next;
  if (string->controller_tail == &voice->h) {
    string->controller_tail = &previous->h;
  }
  voice->handoff_next = NULL;
}

static int32_t wg_double_bass_release_voice(
    CSOUND *csound, WG_DOUBLE_BASS_VOICE *p)
{
  if (p->double_bass != NULL && p->owns_string) {
    WG_DOUBLE_BASS_STRING_STATE *string;
    csound->LockMutex(p->double_bass->state_lock);
    string = &p->double_bass->strings[p->string_index];
    csound->LockMutex(string->lock);
    if (string->controller_owner == &p->h &&
        string->owner_serial == p->voice_serial) {
      if (p->handoff_next == NULL && p->h.insdshead != NULL) {
        string->last_controller_frequency =
            string->harmonic_fundamental_frequency > 0.0
                ? string->harmonic_fundamental_frequency
                : string->frequency;
        string->last_controller_end_sample = p->span.end_sample;
        string->last_controller_automatic = p->automatic_string;
        string->last_controller_articulation =
            wg_double_bass_voice_articulation(p);
        string->last_controller_continuous =
            wg_double_bass_voice_holds_continuous(p);
        string->last_controller_valid =
            p->span.valid &&
            string->last_controller_frequency > 0.0;
      }
      wg_double_bass_promote_controller(string, p);
      if (string->controller_owner == NULL) {
        /* Keep the edge latch and rail state for an exact later handoff. */
        string->trigger = 0.0;
      }
    } else if (!p->handed_off) {
      wg_double_bass_unlink_controller(string, p);
    }
    csound->UnlockMutex(string->lock);
    csound->UnlockMutex(p->double_bass->state_lock);
  }
  p->double_bass = NULL;
  p->voice_serial = 0U;
  p->handoff_next = NULL;
  p->automatic_string = 0;
  p->predecessor_valid = 0;
  p->continuity_checked = 0;
  p->handed_off = 0;
  p->owns_string = 0;
  return OK;
}

static int32_t wg_double_bass_voice_init(
    CSOUND *csound, WG_DOUBLE_BASS_VOICE *p)
{
  const int32_t handle = wg_double_bass_read_handle(p->idouble_bass);
  const double requested_string = p->istring != NULL
      ? (double)*p->istring : WG_DOUBLE_BASS_NAN;
  uint32_t string_index;
  WG_DOUBLE_BASS_STRING_STATE *string;
  WG_DOUBLE_BASS_VOICE *tail;

  if (p->double_bass != NULL && p->owns_string) {
    wg_double_bass_release_voice(csound, p);
  }
  p->double_bass = NULL;
  p->voice_serial = 0U;
  p->string_index = 0U;
  p->handoff_next = NULL;
  memset(&p->span, 0, sizeof(p->span));
  p->predecessor_end_sample = 0U;
  p->predecessor_articulation = WG_DOUBLE_BASS_ARTICULATION_ARCO;
  p->automatic_string = 0;
  p->predecessor_continuous = 0;
  p->predecessor_valid = 0;
  p->continuity_checked = 0;
  p->handed_off = 0;
  p->owns_string = 0;
  if (UNLIKELY(handle < 1)) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass: handle must be a positive integer\n");
  }
  if (UNLIKELY(!isfinite(requested_string) || requested_string < 0.0 ||
               requested_string > 4.0 ||
               floor(requested_string) != requested_string)) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass: string must be 0, 1, 2, 3, or 4\n");
  }
  p->double_bass = wg_double_bass_get_by_handle(csound, handle);
  if (UNLIKELY(p->double_bass == NULL)) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass: unknown double-bass handle %d\n", handle);
  }
  if (UNLIKELY(p->double_bass->ksmps != p->h.insdshead->ksmps)) {
    p->double_bass = NULL;
    return csound->InitError(
        csound, "hlolli_wg_double_bass: handle %d requires engine ksmps %u\n",
        handle, wg_double_bass_engine_ksmps(csound));
  }
  wg_double_bass_capture_instance_span(csound, &p->h, &p->span);

  p->automatic_string = requested_string == 0.0;
  if (p->automatic_string) {
    return OK;
  }
  string_index = (uint32_t)requested_string - 1U;
  csound->LockMutex(p->double_bass->state_lock);
  string = &p->double_bass->strings[string_index];
  tail = wg_double_bass_voice_from_opds(string->controller_tail);
  if (string->owner_serial != 0U &&
      (tail == NULL || !wg_double_bass_instances_can_queue(
           csound, &tail->h, &tail->span, &p->h, &p->span))) {
    csound->UnlockMutex(p->double_bass->state_lock);
    p->double_bass = NULL;
    return csound->InitError(
        csound, "hlolli_wg_double_bass: string %u already has a controller\n",
        string_index + 1U);
  }
  if (p->double_bass->next_voice_serial == UINT64_MAX) {
    csound->UnlockMutex(p->double_bass->state_lock);
    p->double_bass = NULL;
    return csound->InitError(
        csound, "hlolli_wg_double_bass: no voice identifiers remain\n");
  }
  p->double_bass->next_voice_serial++;
  p->voice_serial = p->double_bass->next_voice_serial;
  p->string_index = string_index;
  csound->LockMutex(string->lock);
  if (string->controller_owner == NULL) {
    string->controller_owner = &p->h;
    string->controller_tail = &p->h;
    string->owner_serial = p->voice_serial;
  } else {
    tail->handoff_next = &p->h;
    string->controller_tail = &p->h;
  }
  csound->UnlockMutex(string->lock);
  p->owns_string = 1;
  csound->UnlockMutex(p->double_bass->state_lock);
  return OK;
}

static int32_t wg_double_bass_acquire_automatic_voice(
    CSOUND *csound, WG_DOUBLE_BASS_VOICE *p,
    double sounding_frequency, double selection_frequency)
{
  WG_DOUBLE_BASS_STRING_STATE *string;
  WG_DOUBLE_BASS_VOICE *tail;
  uint32_t string_index = 0U;
  int32_t selected;

  csound->LockMutex(p->double_bass->state_lock);
  selected = wg_double_bass_choose_auto_string(
      csound, p->double_bass, p, selection_frequency, &string_index);
  if (selected < 0) {
    csound->UnlockMutex(p->double_bass->state_lock);
    return csound->PerfError(
        csound, &p->h,
        "hlolli_wg_double_bass: automatic fundamental is outside the playable range\n");
  }
  if (selected == 0) {
    csound->UnlockMutex(p->double_bass->state_lock);
    return csound->PerfError(
        csound, &p->h, "hlolli_wg_double_bass: all four strings are busy\n");
  }
  if (p->double_bass->next_voice_serial == UINT64_MAX) {
    csound->UnlockMutex(p->double_bass->state_lock);
    return csound->PerfError(
        csound, &p->h, "hlolli_wg_double_bass: no voice identifiers remain\n");
  }
  p->double_bass->next_voice_serial++;
  p->voice_serial = p->double_bass->next_voice_serial;
  p->string_index = string_index;
  string = &p->double_bass->strings[string_index];
  csound->LockMutex(string->lock);
  tail = wg_double_bass_voice_from_opds(string->controller_tail);
  if (string->controller_owner == NULL) {
    string->controller_owner = &p->h;
    string->controller_tail = &p->h;
    string->owner_serial = p->voice_serial;
  } else {
    tail->handoff_next = &p->h;
    string->controller_tail = &p->h;
    if (string->controller_owner == &tail->h && tail->span.valid &&
        p->span.valid &&
        tail->span.end_sample <= p->span.start_sample) {
      wg_double_bass_promote_controller(string, tail);
    }
  }
  if (!(string->frequency > 0.0)) {
    string->frequency = sounding_frequency;
  }
  csound->UnlockMutex(string->lock);
  p->owns_string = 1;
  csound->UnlockMutex(p->double_bass->state_lock);
  return OK;
}

static int32_t wg_double_bass_voice_perf(
    CSOUND *csound, WG_DOUBLE_BASS_VOICE *p)
{
  WG_DOUBLE_BASS_STRING_STATE *string;
  const uint64_t epoch = csound->GetEngineKcounter(csound);
  const uint32_t ksmps = p->h.insdshead->ksmps;
  const uint32_t offset = p->h.insdshead->ksmps_offset;
  const uint32_t limit = ksmps - p->h.insdshead->ksmps_no_end;
  const uint32_t active_samples = limit > offset ? limit - offset : 0U;
  const int64_t current_sample = csound->GetCurrentTimeSamples(csound);
  const uint64_t block_start = current_sample > 0 ?
      (uint64_t)current_sample : 0U;
  const uint64_t start_sample = wg_double_bass_add_samples(block_start, offset);
  const uint64_t end_sample = wg_double_bass_add_samples(block_start, limit);
  double pan;
  double left_gain;
  double right_gain;
  double sounding_frequency;
  double fundamental_frequency;
  double harmonic_request;
  int32_t result;
  int32_t trigger_positive;
  uint32_t sample;

  memset(p->out_left, 0, ksmps * sizeof(MYFLT));
  memset(p->out_right, 0, ksmps * sizeof(MYFLT));
  if (UNLIKELY(p->double_bass == NULL)) {
    return csound->PerfError(
        csound, &p->h, "hlolli_wg_double_bass: voice has no string state\n");
  }
  harmonic_request = p->kharmonic != NULL
      ? (double)*p->kharmonic : 0.0;
  sounding_frequency = wg_double_bass_voice_sounding_frequency(
      csound, p, harmonic_request);
  if (UNLIKELY(wg_double_bass_sounded_harmonic_exceeds_dsp_range(
          p->double_bass, sounding_frequency, harmonic_request))) {
    return csound->PerfError(
        csound, &p->h,
        "hlolli_wg_double_bass: sounded harmonic is outside the DSP range "
        "(sounded %.17g, harmonic %.17g)\n",
        sounding_frequency, harmonic_request);
  }
  if (UNLIKELY(!wg_double_bass_pitch_request_is_valid(
          p->double_bass, sounding_frequency, harmonic_request,
          NULL, &fundamental_frequency))) {
    return csound->PerfError(
        csound, &p->h,
        "hlolli_wg_double_bass: fundamental is outside the playable range "
        "(sounded %.17g, harmonic %.17g)\n",
        sounding_frequency, harmonic_request);
  }
  if (UNLIKELY(p->owns_string && !wg_double_bass_string_can_play(
          p->double_bass, p->string_index, fundamental_frequency))) {
    return csound->PerfError(
        csound, &p->h,
        "hlolli_wg_double_bass: string %u fundamental is outside the playable range "
        "(sounded %.17g, harmonic %.17g)\n",
        p->string_index + 1U, sounding_frequency, harmonic_request);
  }
  if (!p->owns_string) {
    result = wg_double_bass_acquire_automatic_voice(
        csound, p, sounding_frequency, fundamental_frequency);
    if (UNLIKELY(result != OK)) {
      return result;
    }
  }
  pan = -0.45 + 0.30 * (double)p->string_index;
  left_gain = sqrt(0.5 * (1.0 - pan));
  right_gain = sqrt(0.5 * (1.0 + pan));
  csound->LockMutex(p->double_bass->state_lock);
  string = &p->double_bass->strings[p->string_index];
  csound->LockMutex(string->lock);
  if (p->handed_off) {
    csound->UnlockMutex(string->lock);
    csound->UnlockMutex(p->double_bass->state_lock);
    return OK;
  }
  if (string->controller_owner == &p->h &&
      p->handoff_next != NULL &&
      p->handoff_next->insdshead->ksmps_offset == 0U) {
    wg_double_bass_promote_controller(string, p);
    csound->UnlockMutex(string->lock);
    csound->UnlockMutex(p->double_bass->state_lock);
    return OK;
  }
  if (string->controller_owner != &p->h && offset == 0U &&
      string->controller_owner != NULL &&
      wg_double_bass_voice_from_opds(
          string->controller_owner)->handoff_next == &p->h) {
    wg_double_bass_promote_controller(
        string, wg_double_bass_voice_from_opds(string->controller_owner));
  }
  if (string->controller_owner != &p->h ||
      string->owner_serial != p->voice_serial) {
    const double start = (double)p->h.insdshead->p2.value;
    const double owner_start = string->controller_owner != NULL ?
        (double)string->controller_owner->insdshead->p2.value : -1.0;
    csound->UnlockMutex(string->lock);
    csound->UnlockMutex(p->double_bass->state_lock);
    return csound->PerfError(
        csound, &p->h,
        "hlolli_wg_double_bass: string %u handoff ran out of order "
        "(start %.9g, owner %.9g, offset %u)\n",
        p->string_index + 1U, start, owner_start, offset);
  }
  if (!p->continuity_checked) {
    if ((p->predecessor_valid &&
         !wg_double_bass_predecessor_forms_continuous(p)) ||
        (!p->predecessor_valid && string->last_controller_valid &&
         !wg_double_bass_last_controller_forms_continuous(string, p))) {
      wg_double_bass_release_bow(string);
    }
    string->last_controller_valid = 0;
    p->continuity_checked = 1;
  }
  string->passive_open_active = 0;
  string->passive_open_level = 0.0;
  if (!string->update_epoch_valid || string->update_epoch != epoch) {
    string->update_epoch = epoch;
    string->updated_until = offset;
    string->update_epoch_valid = 1;
  }
  if (string->updated_until > offset) {
    csound->UnlockMutex(string->lock);
    csound->UnlockMutex(p->double_bass->state_lock);
    return csound->PerfError(
        csound, &p->h,
        "hlolli_wg_double_bass: string %u controller spans overlap\n",
        p->string_index + 1U);
  }
  wg_double_bass_prepare_bridge_send(string, epoch, ksmps);
  wg_double_bass_advance_string_gap(string, start_sample,
                               p->double_bass->sample_rate);
  string->trigger = wg_double_bass_clamp(
      wg_double_bass_input(p->ktrigger, 0.0), 0.0, 1.25);
  string->frequency = sounding_frequency;
  string->force = wg_double_bass_clamp(
      wg_double_bass_input(p->kforce, 0.45), 0.0, 1.0);
  string->speed = wg_double_bass_clamp(
      wg_double_bass_input(p->kspeed, 0.55), -1.0, 1.0);
  string->position = wg_double_bass_clamp(
      wg_double_bass_input(p->kposition, 0.12), 0.01, 0.49);
  string->vibrato_depth_cents = wg_double_bass_clamp(
      wg_double_bass_input(p->kvibrato_depth, 18.0), 0.0, 200.0);
  string->vibrato_rate_hz = wg_double_bass_clamp(
      wg_double_bass_input(p->kvibrato_rate, 5.2), 0.0, 20.0);
  string->articulation = (uint32_t)floor(wg_double_bass_clamp(
      wg_double_bass_input(p->karticulation, 0.0), 0.0, 15.0) + 0.5);
  {
    wg_double_bass_prepare_harmonic(
        p->double_bass, string, harmonic_request, string->frequency);
  }
  string->strange = wg_double_bass_clamp(
      wg_double_bass_input(p->kstrange, 0.0), -1.0, 1.0);
  trigger_positive = string->trigger > 1.0e-7;
  if (string->release_gate_positive && !trigger_positive) {
    wg_double_bass_begin_release(string);
  } else if (!string->release_gate_positive && trigger_positive) {
    wg_double_bass_begin_attack(string);
  }
  wg_double_bass_prepare_finger(
      string, string->harmonic_fundamental_frequency);
  wg_double_bass_prepare_waveguide(
      string, string->harmonic_fundamental_frequency,
      p->double_bass->sample_rate);
  wg_double_bass_prepare_gesture(string, p->double_bass->sample_rate);
  wg_double_bass_prepare_exciter(string, p->double_bass->sample_rate);
  for (sample = offset; sample < limit; sample++) {
    const double bridge_coupling = wg_double_bass_bridge_coupling_tick(
        p->double_bass, p->string_index, epoch, sample);
    const double wave = wg_double_bass_waveguide_step(
        string, 1, p->double_bass->sample_rate, bridge_coupling);
    /* The bridge tap is force divided by 2 Z0 at this lossy boundary. */
    const double bridge_force =
        2.0 * string->characteristic_impedance * wave;
    wg_double_bass_write_bridge_send(
        string, epoch, sample, bridge_force);
    p->out_left[sample] =
        (MYFLT)(WG_DOUBLE_BASS_DRY_GAIN * left_gain * wave);
    p->out_right[sample] =
        (MYFLT)(WG_DOUBLE_BASS_DRY_GAIN * right_gain * wave);
  }
  string->activity = fmax(string->activity, string->trigger);
  wg_double_bass_advance_phase(&string->phase, active_samples,
                          string->frequency, p->double_bass->sample_rate);
  string->last_update_sample = end_sample;
  string->last_update_valid = 1;
  string->updated_until = limit;
  if (p->handoff_next != NULL &&
      limit <= p->handoff_next->insdshead->ksmps_offset) {
    if (limit < p->handoff_next->insdshead->ksmps_offset) {
      wg_double_bass_release_bow(string);
    }
    wg_double_bass_promote_controller(string, p);
  }
  csound->UnlockMutex(string->lock);
  csound->UnlockMutex(p->double_bass->state_lock);
  return OK;
}

static WG_DOUBLE_BASS_RESONANCE *wg_double_bass_renderer_from_opds(OPDS *owner)
{
  return (WG_DOUBLE_BASS_RESONANCE *)owner;
}

static void wg_double_bass_promote_renderer(
    WG_DOUBLE_BASS_STATE *state, WG_DOUBLE_BASS_RESONANCE *current)
{
  OPDS *next = current->handoff_next;

  current->handoff_next = NULL;
  current->handed_off = 1;
  state->renderer_owner = next;
  if (next == NULL) {
    state->renderer_tail = NULL;
  }
}

static void wg_double_bass_unlink_renderer(
    WG_DOUBLE_BASS_STATE *state, WG_DOUBLE_BASS_RESONANCE *renderer)
{
  WG_DOUBLE_BASS_RESONANCE *previous =
      wg_double_bass_renderer_from_opds(state->renderer_owner);

  while (previous != NULL && previous->handoff_next != &renderer->h) {
    previous = wg_double_bass_renderer_from_opds(previous->handoff_next);
  }
  if (previous == NULL) {
    return;
  }
  previous->handoff_next = renderer->handoff_next;
  if (state->renderer_tail == &renderer->h) {
    state->renderer_tail = &previous->h;
  }
  renderer->handoff_next = NULL;
}

static int32_t wg_double_bass_release_renderer(
    CSOUND *csound, WG_DOUBLE_BASS_RESONANCE *p)
{
  if (p->double_bass != NULL && p->owns_renderer) {
    csound->LockMutex(p->double_bass->resonance_lock);
    if (p->double_bass->renderer_owner == &p->h) {
      wg_double_bass_promote_renderer(p->double_bass, p);
    } else if (!p->handed_off) {
      wg_double_bass_unlink_renderer(p->double_bass, p);
    }
    csound->UnlockMutex(p->double_bass->resonance_lock);
  }
  p->double_bass = NULL;
  memset(&p->span, 0, sizeof(p->span));
  p->handoff_next = NULL;
  p->handed_off = 0;
  p->owns_renderer = 0;
  return OK;
}

static int32_t wg_double_bass_resonance_init(
    CSOUND *csound, WG_DOUBLE_BASS_RESONANCE *p)
{
  const int32_t handle = wg_double_bass_read_handle(p->idouble_bass);

  if (p->double_bass != NULL && p->owns_renderer) {
    wg_double_bass_release_renderer(csound, p);
  }
  p->double_bass = NULL;
  p->handoff_next = NULL;
  p->handed_off = 0;
  p->owns_renderer = 0;
  if (UNLIKELY(handle < 1)) {
    return csound->InitError(
        csound,
        "hlolli_wg_double_bass_resonance: handle must be a positive integer\n");
  }
  p->double_bass = wg_double_bass_get_by_handle(csound, handle);
  if (UNLIKELY(p->double_bass == NULL)) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_resonance: unknown double-bass handle %d\n",
        handle);
  }
  if (UNLIKELY(p->double_bass->ksmps != p->h.insdshead->ksmps)) {
    p->double_bass = NULL;
    return csound->InitError(
        csound,
        "hlolli_wg_double_bass_resonance: handle %d requires engine ksmps %u\n",
        handle, wg_double_bass_engine_ksmps(csound));
  }
  wg_double_bass_capture_instance_span(csound, &p->h, &p->span);
  csound->LockMutex(p->double_bass->resonance_lock);
  if (p->double_bass->renderer_owner != NULL) {
    WG_DOUBLE_BASS_RESONANCE *tail =
        wg_double_bass_renderer_from_opds(p->double_bass->renderer_tail);
    if (tail == NULL || !wg_double_bass_instances_can_queue(
            csound, &tail->h, &tail->span, &p->h, &p->span)) {
      csound->UnlockMutex(p->double_bass->resonance_lock);
      p->double_bass = NULL;
      return csound->InitError(
          csound,
          "hlolli_wg_double_bass_resonance: double_bass %d already has an output opcode\n",
          handle);
    }
  }
  if (p->double_bass->renderer_owner == NULL) {
    p->double_bass->renderer_owner = &p->h;
    p->double_bass->renderer_tail = &p->h;
  } else {
    wg_double_bass_renderer_from_opds(
        p->double_bass->renderer_tail)->handoff_next = &p->h;
    p->double_bass->renderer_tail = &p->h;
  }
  p->owns_renderer = 1;
  csound->UnlockMutex(p->double_bass->resonance_lock);
  return OK;
}

static void wg_double_bass_advance_unowned_strings(
    CSOUND *csound, WG_DOUBLE_BASS_STATE *state, uint64_t epoch,
    uint64_t block_start, uint32_t offset, uint32_t limit)
{
  const double sympathetic = wg_double_bass_sympathetic_control_max(
      state, epoch, offset, limit);
  uint32_t string_index;

  csound->LockMutex(state->state_lock);
  for (string_index = 0U; string_index < WG_DOUBLE_BASS_STRINGS;
       string_index++) {
    WG_DOUBLE_BASS_STRING_STATE *string = &state->strings[string_index];
    const double decay_seconds = wg_double_bass_bound_string_model(string)
        ->loss_time_constant_seconds;
    const double level_decay = exp(-1.0 / fmax(
        1.0, decay_seconds * state->sample_rate));
    uint32_t start = offset;
    uint32_t sample;

    csound->LockMutex(string->lock);
    if (!wg_double_bass_sympathetic_state_is_finite(string)) {
      wg_double_bass_recover_sympathetic_state(string);
    }
    if (string->controller_owner != NULL) {
      csound->UnlockMutex(string->lock);
      continue;
    }
    if (sympathetic > 1.0e-12) {
      if (!string->passive_open_active) {
        wg_double_bass_release_bow(string);
      }
      string->passive_open_active = 1;
    }
    if (!string->passive_open_active) {
      csound->UnlockMutex(string->lock);
      continue;
    }
    if (string->update_epoch_valid && string->update_epoch == epoch) {
      start = string->updated_until > start ? string->updated_until : start;
    } else {
      string->update_epoch = epoch;
      string->updated_until = offset;
      string->update_epoch_valid = 1;
    }
    if (start >= limit) {
      csound->UnlockMutex(string->lock);
      continue;
    }

    wg_double_bass_advance_string_gap(
        string, wg_double_bass_add_samples(block_start, start),
        state->sample_rate);
    string->trigger = 0.0;
    /* Losing the controller removes bow contact, not the left-hand stop.
       Retuning a live tail to open would transpose its stored vibration and
       synthesize a finger release that the score never requested. Only an
       untouched string needs open geometry; later geometry is handle state. */
    if (!string->harmonic_initialized) {
      string->frequency = string->open_frequency;
      wg_double_bass_prepare_harmonic(
          state, string, 0.0, string->open_frequency);
      wg_double_bass_prepare_finger(string, string->open_frequency);
    }
    string->vibrato_depth_cents = 0.0;
    string->vibrato_rate_hz = 0.0;
    string->strange = 0.0;
    wg_double_bass_prepare_waveguide(
        string, string->harmonic_fundamental_frequency, state->sample_rate);
    string->release_gate_positive = 0;
    wg_double_bass_prepare_bridge_send(string, epoch, state->ksmps);
    for (sample = start; sample < limit; sample++) {
      const double bridge_coupling = wg_double_bass_bridge_coupling_tick(
          state, string_index, epoch, sample);
      const double wave = wg_double_bass_waveguide_step(
          string, 0, state->sample_rate, bridge_coupling);
      const double bridge_force =
          2.0 * string->characteristic_impedance * wave;

      wg_double_bass_write_bridge_send(
          string, epoch, sample, bridge_force);
      string->passive_open_level = fmax(
          fabs(wave), string->passive_open_level * level_decay);
      string->passive_open_energy = fmin(
          1.0e12, string->passive_open_energy + wave * wave);
      string->passive_open_peak = fmax(
          string->passive_open_peak, fabs(wave));
      string->passive_open_samples = wg_double_bass_add_samples(
          string->passive_open_samples, 1U);
    }
    string->activity *= wg_double_bass_decay(
        (uint64_t)(limit - start), state->sample_rate,
        decay_seconds);
    wg_double_bass_advance_phase(
        &string->phase, (uint64_t)(limit - start),
        string->frequency, state->sample_rate);
    string->last_update_sample = wg_double_bass_add_samples(
        block_start, limit);
    string->last_update_valid = 1;
    string->updated_until = limit;
    if (sympathetic <= 1.0e-12 &&
        string->passive_open_level <= 1.0e-9) {
      string->passive_open_level = 0.0;
      string->passive_open_active = 0;
    }
    csound->UnlockMutex(string->lock);
  }
  csound->UnlockMutex(state->state_lock);
}

static double wg_double_bass_collect_bridge_input(
    CSOUND *csound, WG_DOUBLE_BASS_STATE *state, uint64_t epoch,
    uint32_t offset, uint32_t limit)
{
  const uint64_t source_epoch = epoch > 0U ? epoch - 1U : 0U;
  const int32_t source_valid = epoch > 0U;
  double activity = 0.0;
  uint32_t string_index;

  csound->LockMutex(state->state_lock);
  for (string_index = 0U; string_index < WG_DOUBLE_BASS_STRINGS;
       string_index++) {
    WG_DOUBLE_BASS_STRING_STATE *string = &state->strings[string_index];
    double *input = state->body_input +
        (size_t)string_index * state->ksmps;
    const uint32_t bank = (uint32_t)(source_epoch & 1U);

    memset(input + offset, 0,
           (size_t)(limit - offset) * sizeof(double));
    csound->LockMutex(string->lock);
    activity = fmax(activity, string->trigger);
    if (source_valid && string->bridge_send_valid[bank] &&
        string->bridge_send_epoch[bank] == source_epoch) {
      memcpy(input + offset, string->bridge_send[bank] + offset,
             (size_t)(limit - offset) * sizeof(double));
    }
    csound->UnlockMutex(string->lock);
  }
  csound->UnlockMutex(state->state_lock);
  return activity;
}

static int32_t wg_double_bass_resonance_perf(
    CSOUND *csound, WG_DOUBLE_BASS_RESONANCE *p)
{
  const uint64_t epoch = csound->GetEngineKcounter(csound);
  const uint32_t ksmps = p->h.insdshead->ksmps;
  const uint32_t offset = p->h.insdshead->ksmps_offset;
  const uint32_t limit = ksmps - p->h.insdshead->ksmps_no_end;
  const int64_t current_sample = csound->GetCurrentTimeSamples(csound);
  const uint64_t block_start = current_sample > 0 ?
      (uint64_t)current_sample : 0U;
  const uint64_t start_sample = wg_double_bass_add_samples(block_start, offset);
  const uint64_t end_sample = wg_double_bass_add_samples(block_start, limit);
  double activity;
  uint32_t sample;

  memset(p->out_left, 0, ksmps * sizeof(MYFLT));
  memset(p->out_right, 0, ksmps * sizeof(MYFLT));
  if (UNLIKELY(p->double_bass == NULL || !p->owns_renderer)) {
    return csound->PerfError(
        csound, &p->h,
        "hlolli_wg_double_bass_resonance: renderer has no double-bass state\n");
  }
  csound->LockMutex(p->double_bass->resonance_lock);
  if (p->handed_off) {
    csound->UnlockMutex(p->double_bass->resonance_lock);
    return OK;
  }
  if (p->double_bass->renderer_owner == &p->h &&
      p->handoff_next != NULL &&
      p->handoff_next->insdshead->ksmps_offset == 0U) {
    wg_double_bass_promote_renderer(p->double_bass, p);
    csound->UnlockMutex(p->double_bass->resonance_lock);
    return OK;
  }
  if (p->double_bass->renderer_owner != &p->h && offset == 0U &&
      p->double_bass->renderer_owner != NULL &&
      wg_double_bass_renderer_from_opds(
          p->double_bass->renderer_owner)->handoff_next == &p->h) {
    wg_double_bass_promote_renderer(
        p->double_bass,
        wg_double_bass_renderer_from_opds(p->double_bass->renderer_owner));
  }
  if (p->double_bass->renderer_owner != &p->h) {
    const double start = (double)p->h.insdshead->p2.value;
    const double owner_start = p->double_bass->renderer_owner != NULL ?
        (double)p->double_bass->renderer_owner->insdshead->p2.value : -1.0;
    csound->UnlockMutex(p->double_bass->resonance_lock);
    return csound->PerfError(
        csound, &p->h,
        "hlolli_wg_double_bass_resonance: double_bass %d output handoff ran out of "
        "order (start %.9g, owner %.9g, offset %u)\n",
        p->double_bass->handle, start, owner_start, offset);
  }
  if (!p->double_bass->render_epoch_valid ||
      p->double_bass->render_epoch != epoch) {
    p->double_bass->render_epoch = epoch;
    p->double_bass->rendered_until = offset;
    p->double_bass->render_epoch_valid = 1;
  }
  if (p->double_bass->rendered_until > offset) {
    csound->UnlockMutex(p->double_bass->resonance_lock);
    return csound->PerfError(
        csound, &p->h,
        "hlolli_wg_double_bass_resonance: double_bass %d output spans overlap\n",
        p->double_bass->handle);
  }
  wg_double_bass_advance_renderer_gap(p->double_bass, start_sample);
  p->double_bass->body = wg_double_bass_clamp(
      wg_double_bass_input(p->kbody, 0.72), 0.0, 1.0);
  p->double_bass->sympathetic = wg_double_bass_clamp(
      wg_double_bass_input(p->ksympathetic, 0.55), 0.0, 1.0);
  p->double_bass->mute = wg_double_bass_clamp(
      wg_double_bass_input(p->kmute, 0.0), 0.0, 1.0);
  wg_double_bass_prepare_sympathetic_control(p->double_bass, epoch);
  for (sample = offset; sample < limit; sample++) {
    p->double_bass->sympathetic_control[(uint32_t)(epoch & 1U)][sample] =
        p->double_bass->sympathetic;
  }
  wg_double_bass_advance_unowned_strings(
      csound, p->double_bass, epoch, block_start, offset, limit);
  wg_double_bass_prepare_body(p->double_bass);
  activity = wg_double_bass_collect_bridge_input(
      csound, p->double_bass, epoch, offset, limit);
  p->double_bass->resonance_activity = fmax(
      p->double_bass->resonance_activity, activity);
  for (sample = offset; sample < limit; sample++) {
    double input[WG_DOUBLE_BASS_STRINGS];
    double left;
    double right;
    uint32_t string_index;

    for (string_index = 0U; string_index < WG_DOUBLE_BASS_STRINGS;
         string_index++) {
      input[string_index] = p->double_bass->body_input[
          (size_t)string_index * ksmps + sample];
    }
    wg_double_bass_body_tick(p->double_bass, input, &left, &right, 1);
    p->out_left[sample] = (MYFLT)left;
    p->out_right[sample] = (MYFLT)right;
  }
  p->double_bass->last_render_sample = end_sample;
  p->double_bass->last_render_valid = 1;
  p->double_bass->rendered_until = limit;
  if (p->handoff_next != NULL &&
      limit <= p->handoff_next->insdshead->ksmps_offset) {
    wg_double_bass_promote_renderer(p->double_bass, p);
  }
  csound->UnlockMutex(p->double_bass->resonance_lock);
  return OK;
}

#if defined(HLOLLI_WG_DOUBLE_BASS_TEST_API)
static int32_t wg_double_bass_test_diagnostic_gains_init(
    CSOUND *csound, WG_DOUBLE_BASS_TEST_DIAGNOSTIC_GAINS *p)
{
  const int32_t handle = wg_double_bass_read_handle(p->idouble_bass);
  const double requested_string = p->istring != NULL
      ? (double)*p->istring : WG_DOUBLE_BASS_NAN;
  const double finger = p->ifinger != NULL
      ? (double)*p->ifinger : WG_DOUBLE_BASS_NAN;
  const double board = p->iboard != NULL
      ? (double)*p->iboard : WG_DOUBLE_BASS_NAN;
  const double rosin = p->irosin != NULL
      ? (double)*p->irosin : WG_DOUBLE_BASS_NAN;
  const double mechanical = p->imechanical != NULL
      ? (double)*p->imechanical : WG_DOUBLE_BASS_NAN;
  const double nut_cutoff_scale = p->inut_cutoff_scale != NULL
      ? (double)*p->inut_cutoff_scale : WG_DOUBLE_BASS_NAN;
  const double bridge_cutoff_scale = p->ibridge_cutoff_scale != NULL
      ? (double)*p->ibridge_cutoff_scale : WG_DOUBLE_BASS_NAN;
  WG_DOUBLE_BASS_STATE *state;
  WG_DOUBLE_BASS_STRING_STATE *string;

  *p->result = FL(0.0);
  if (handle < 1 || !isfinite(requested_string) ||
      requested_string < 1.0 || requested_string > 4.0 ||
      floor(requested_string) != requested_string ||
      !isfinite(finger) || finger < 0.0 || finger > 1.0 ||
      !isfinite(board) || board < 0.0 || board > 1.0 ||
      !isfinite(rosin) || rosin < 0.0 || rosin > 1.0 ||
      !isfinite(mechanical) || mechanical < 0.0 || mechanical > 1.0 ||
      !isfinite(nut_cutoff_scale) || nut_cutoff_scale < 0.125 ||
      nut_cutoff_scale > 4.0 ||
      !isfinite(bridge_cutoff_scale) || bridge_cutoff_scale < 0.125 ||
      bridge_cutoff_scale > 4.0) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_test_diagnostic_gains: invalid input\n");
  }
  state = wg_double_bass_get_by_handle(csound, handle);
  if (state == NULL) {
    return csound->InitError(
        csound,
        "hlolli_wg_double_bass_test_diagnostic_gains: unknown double-bass handle %d\n",
        handle);
  }
  csound->LockMutex(state->state_lock);
  string = &state->strings[(uint32_t)requested_string - 1U];
  csound->LockMutex(string->lock);
  string->diagnostic_finger_gain = finger;
  string->diagnostic_board_gain = board;
  string->diagnostic_rosin_gain = rosin;
  string->diagnostic_mechanical_gain = mechanical;
  string->diagnostic_nut_cutoff_scale = nut_cutoff_scale;
  string->diagnostic_bridge_cutoff_scale = bridge_cutoff_scale;
  csound->UnlockMutex(string->lock);
  csound->UnlockMutex(state->state_lock);
  *p->result = FL(1.0);
  return OK;
}

static int32_t wg_double_bass_test_string_init(
    CSOUND *csound, WG_DOUBLE_BASS_TEST_STRING *p)
{
  const int32_t handle = wg_double_bass_read_handle(p->idouble_bass);
  const double requested_string = p->istring != NULL ?
      (double)*p->istring : WG_DOUBLE_BASS_NAN;
  WG_DOUBLE_BASS_STATE *state;
  WG_DOUBLE_BASS_STRING_STATE *string;

  if (handle < 1 || !isfinite(requested_string) || requested_string < 1.0 ||
      requested_string > 4.0 || floor(requested_string) != requested_string) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_test_string: invalid handle or string\n");
  }
  state = wg_double_bass_get_by_handle(csound, handle);
  if (state == NULL) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_test_string: unknown double-bass handle %d\n",
        handle);
  }
  csound->LockMutex(state->state_lock);
  string = &state->strings[(uint32_t)requested_string - 1U];
  csound->LockMutex(string->lock);
  *p->phase = (MYFLT)string->phase;
  *p->vibrato_phase = (MYFLT)string->vibrato_phase;
  *p->frequency = (MYFLT)string->frequency;
  *p->trigger = (MYFLT)string->trigger;
  *p->force = (MYFLT)string->force;
  *p->speed = (MYFLT)string->speed;
  *p->activity = (MYFLT)string->activity;
  *p->last_update_sample = (MYFLT)string->last_update_sample;
  *p->gap_samples = (MYFLT)string->gap_samples;
  *p->owner = string->controller_owner != NULL ? FL(1.0) : FL(0.0);
  *p->successor = string->controller_owner != string->controller_tail ?
      FL(1.0) : FL(0.0);
  csound->UnlockMutex(string->lock);
  csound->UnlockMutex(state->state_lock);
  return OK;
}

static int32_t wg_double_bass_test_finger_init(
    CSOUND *csound, WG_DOUBLE_BASS_TEST_FINGER *p)
{
  const int32_t handle = wg_double_bass_read_handle(p->idouble_bass);
  const double requested_string = p->istring != NULL ?
      (double)*p->istring : WG_DOUBLE_BASS_NAN;
  WG_DOUBLE_BASS_STATE *state;
  WG_DOUBLE_BASS_STRING_STATE *string;
  int32_t finite;

  if (handle < 1 || !isfinite(requested_string) || requested_string < 1.0 ||
      requested_string > 4.0 || floor(requested_string) != requested_string) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_test_finger: invalid handle or string\n");
  }
  state = wg_double_bass_get_by_handle(csound, handle);
  if (state == NULL) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_test_finger: unknown double-bass handle %d\n",
        handle);
  }
  csound->LockMutex(state->state_lock);
  string = &state->strings[(uint32_t)requested_string - 1U];
  csound->LockMutex(string->lock);
  finite = isfinite(string->open_frequency) &&
      isfinite(string->finger_target_position) &&
      isfinite(string->finger_position) &&
      isfinite(string->finger_pressure_target) &&
      isfinite(string->finger_pressure) &&
      isfinite(string->finger_velocity) &&
      isfinite(string->delay_velocity) &&
      isfinite(string->effective_frequency) &&
      isfinite(string->finger_last_noise) &&
      isfinite(string->finger_noise_energy) &&
      isfinite(string->finger_noise_peak) &&
      string->open_frequency > 0.0 &&
      string->finger_target_position >= 0.0 &&
      string->finger_target_position <= 1.0 &&
      string->finger_position >= 0.0 && string->finger_position <= 1.0 &&
      string->finger_pressure_target >= 0.0 &&
      string->finger_pressure_target <= 1.0 &&
      string->finger_pressure >= 0.0 && string->finger_pressure <= 1.0 &&
      fabs(string->delay_velocity) <= WG_DOUBLE_BASS_DELAY_SLEW + 1.0e-12 &&
      fabs(string->finger_last_noise) <=
          WG_DOUBLE_BASS_FINGER_NOISE_LIMIT + 1.0e-12 &&
      string->finger_noise_energy >= 0.0 &&
      string->finger_noise_peak >= 0.0 &&
      string->finger_noise_peak <= WG_DOUBLE_BASS_FINGER_NOISE_LIMIT + 1.0e-12 &&
      string->finger_motion_kind <= WG_DOUBLE_BASS_FINGER_SHIFTING;
  *p->open_frequency = (MYFLT)string->open_frequency;
  *p->target_position = (MYFLT)string->finger_target_position;
  *p->position = (MYFLT)string->finger_position;
  *p->target_pressure = (MYFLT)string->finger_pressure_target;
  *p->pressure = (MYFLT)string->finger_pressure;
  *p->velocity = (MYFLT)string->finger_velocity;
  *p->delay_velocity = (MYFLT)string->delay_velocity;
  *p->effective_frequency = (MYFLT)string->effective_frequency;
  *p->last_noise = (MYFLT)string->finger_last_noise;
  *p->noise_energy = (MYFLT)string->finger_noise_energy;
  *p->noise_peak = (MYFLT)string->finger_noise_peak;
  *p->noise_state = (MYFLT)string->finger_rng;
  *p->arrivals = (MYFLT)string->finger_arrivals;
  *p->releases = (MYFLT)string->finger_releases;
  *p->shifts = (MYFLT)string->finger_shifts;
  *p->movement_samples = (MYFLT)string->finger_movement_samples;
  *p->noise_samples = (MYFLT)string->finger_noise_samples;
  *p->finite = finite ? FL(1.0) : FL(0.0);
  csound->UnlockMutex(string->lock);
  csound->UnlockMutex(state->state_lock);
  return OK;
}

static int32_t wg_double_bass_test_harmonic_init(
    CSOUND *csound, WG_DOUBLE_BASS_TEST_HARMONIC *p)
{
  const int32_t handle = wg_double_bass_read_handle(p->idouble_bass);
  const double requested_string = p->istring != NULL
      ? (double)*p->istring : WG_DOUBLE_BASS_NAN;
  WG_DOUBLE_BASS_STATE *state;
  WG_DOUBLE_BASS_STRING_STATE *string;
  int32_t finite = 1;
  uint32_t index;

  if (handle < 1 || !isfinite(requested_string) || requested_string < 1.0 ||
      requested_string > 4.0 || floor(requested_string) != requested_string) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_test_harmonic: invalid handle or string\n");
  }
  state = wg_double_bass_get_by_handle(csound, handle);
  if (state == NULL) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_test_harmonic: unknown double-bass handle %d\n",
        handle);
  }
  csound->LockMutex(state->state_lock);
  string = &state->strings[(uint32_t)requested_string - 1U];
  csound->LockMutex(string->lock);
  if (string->harmonic_history == NULL || string->rail_size < 8U ||
      string->harmonic_write_index >= string->rail_size ||
      string->harmonic > WG_DOUBLE_BASS_HARMONIC_MAX_ORDER ||
      string->harmonic_filter_order > WG_DOUBLE_BASS_HARMONIC_MAX_ORDER ||
      string->harmonic_pending_order > WG_DOUBLE_BASS_HARMONIC_MAX_ORDER ||
      (string->harmonic_valid != 0 && string->harmonic_valid != 1) ||
      (string->harmonic_natural != 0 && string->harmonic_natural != 1) ||
      !isfinite(string->harmonic_request) ||
      !isfinite(string->sounding_frequency) ||
      !isfinite(string->harmonic_fundamental_frequency) ||
      !isfinite(string->harmonic_stop_position) ||
      !isfinite(string->harmonic_touch_position) ||
      !isfinite(string->harmonic_touch_target) ||
      !isfinite(string->harmonic_touch_pressure) ||
      !isfinite(string->harmonic_order_mix) ||
      !isfinite(string->harmonic_order_mix_step) ||
      !isfinite(string->harmonic_touch_period) ||
      !isfinite(string->harmonic_projection_input) ||
      !isfinite(string->harmonic_projection_output) ||
      string->harmonic_stop_position < 0.0 ||
      string->harmonic_stop_position > 1.0 ||
      string->harmonic_touch_position < 0.0 ||
      string->harmonic_touch_position > 1.0 ||
      string->harmonic_touch_target < 0.0 ||
      string->harmonic_touch_target > 1.0 ||
      string->harmonic_touch_pressure < 0.0 ||
      string->harmonic_touch_pressure > 1.0 ||
      string->harmonic_order_mix < 0.0 ||
      string->harmonic_order_mix > 1.0 ||
      string->harmonic_order_mix_step <= 0.0 ||
      string->harmonic_order_mix_step > 1.0 ||
      string->harmonic_touch_period < 0.0 ||
      string->harmonic_touch_period > (double)string->rail_size - 2.0 ||
      fabs(string->harmonic_projection_input) > WG_DOUBLE_BASS_WAVE_LIMIT ||
      fabs(string->harmonic_projection_output) > WG_DOUBLE_BASS_WAVE_LIMIT) {
    finite = 0;
  }
  if (string->harmonic_history != NULL) {
    for (index = 0U; index < string->rail_size; index++) {
      if (!isfinite(string->harmonic_history[index]) ||
          fabs(string->harmonic_history[index]) > WG_DOUBLE_BASS_WAVE_LIMIT) {
        finite = 0;
        break;
      }
    }
  }
  *p->requested_order = (MYFLT)string->harmonic_request;
  *p->active_order = (MYFLT)string->harmonic;
  *p->filter_order = (MYFLT)string->harmonic_filter_order;
  *p->valid = string->harmonic_valid ? FL(1.0) : FL(0.0);
  *p->natural = string->harmonic_natural ? FL(1.0) : FL(0.0);
  *p->sounding_frequency = (MYFLT)string->sounding_frequency;
  *p->fundamental_frequency =
      (MYFLT)string->harmonic_fundamental_frequency;
  *p->stop_position = (MYFLT)string->harmonic_stop_position;
  *p->touch_position = (MYFLT)string->harmonic_touch_position;
  *p->touch_target = (MYFLT)string->harmonic_touch_target;
  *p->touch_pressure = (MYFLT)string->harmonic_touch_pressure;
  *p->touch_period = (MYFLT)string->harmonic_touch_period;
  *p->projection_input = (MYFLT)string->harmonic_projection_input;
  *p->projection_output = (MYFLT)string->harmonic_projection_output;
  *p->transitions = (MYFLT)string->harmonic_transitions;
  *p->rejections = (MYFLT)string->harmonic_rejections;
  *p->touch_samples = (MYFLT)string->harmonic_touch_samples;
  *p->history_index = (MYFLT)string->harmonic_write_index;
  *p->finite = finite ? FL(1.0) : FL(0.0);
  csound->UnlockMutex(string->lock);
  csound->UnlockMutex(state->state_lock);
  return OK;
}

static int32_t wg_double_bass_test_exciter_init(
    CSOUND *csound, WG_DOUBLE_BASS_TEST_EXCITER *p)
{
  const int32_t handle = wg_double_bass_read_handle(p->idouble_bass);
  const double requested_string = p->istring != NULL ?
      (double)*p->istring : WG_DOUBLE_BASS_NAN;
  WG_DOUBLE_BASS_STATE *state;
  WG_DOUBLE_BASS_STRING_STATE *string;
  int32_t active;
  int32_t pending_impact;
  int32_t finite;

  if (handle < 1 || !isfinite(requested_string) || requested_string < 1.0 ||
      requested_string > 4.0 || floor(requested_string) != requested_string) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_test_exciter: invalid handle or string\n");
  }
  state = wg_double_bass_get_by_handle(csound, handle);
  if (state == NULL) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_test_exciter: unknown double-bass handle %d\n",
        handle);
  }
  csound->LockMutex(state->state_lock);
  string = &state->strings[(uint32_t)requested_string - 1U];
  csound->LockMutex(string->lock);
  active = string->exciter_age < string->exciter_total_samples ||
      (string->articulation == WG_DOUBLE_BASS_ARTICULATION_TRATTO &&
       string->exciter_contact > 1.0e-7);
  pending_impact =
      string->exciter_mode == WG_DOUBLE_BASS_ARTICULATION_BARTOK &&
      string->exciter_impact_amplitude > 0.0 &&
      string->exciter_age < string->exciter_impact_delay;
  finite = wg_double_bass_exciter_live_is_finite(string) &&
      isfinite(string->exciter_energy) &&
      isfinite(string->exciter_peak) &&
      (string->exciter_mode == 0U ||
       (string->exciter_mode >= WG_DOUBLE_BASS_ARTICULATION_PIZZICATO &&
        string->exciter_mode <= WG_DOUBLE_BASS_ARTICULATION_TRATTO)) &&
      string->exciter_age <= string->exciter_total_samples &&
      string->exciter_position >= 0.01 && string->exciter_position <= 0.49 &&
      string->exciter_contact_target >= 0.0 &&
      string->exciter_contact_target <= 1.0 &&
      string->exciter_contact >= 0.0 && string->exciter_contact <= 1.0 &&
      fabs(string->exciter_speed_target) <= 0.55 + 1.0e-12 &&
      fabs(string->exciter_speed) <= 0.55 + 1.0e-12 &&
      fabs(string->exciter_last_output) <=
          WG_DOUBLE_BASS_EXCITER_LIMIT + 1.0e-12 &&
      string->exciter_energy >= 0.0 && string->exciter_peak >= 0.0 &&
      string->exciter_peak <= WG_DOUBLE_BASS_EXCITER_LIMIT + 1.0e-12 &&
      string->exciter_rng != 0U &&
      string->collision_compression >= 0.0 &&
      string->collision_force >= 0.0;
  *p->articulation = (MYFLT)string->articulation;
  *p->mode = (MYFLT)string->exciter_mode;
  *p->armed = string->exciter_armed ? FL(1.0) : FL(0.0);
  *p->active = active ? FL(1.0) : FL(0.0);
  *p->age = (MYFLT)string->exciter_age;
  *p->total_samples = (MYFLT)string->exciter_total_samples;
  *p->pulse_samples = (MYFLT)string->exciter_pulse_samples;
  *p->notch_delay = (MYFLT)string->exciter_notch_delay;
  *p->position = (MYFLT)string->exciter_position;
  *p->contact_target = (MYFLT)string->exciter_contact_target;
  *p->contact = (MYFLT)string->exciter_contact;
  *p->speed_target = (MYFLT)string->exciter_speed_target;
  *p->speed = (MYFLT)string->exciter_speed;
  *p->last_output = (MYFLT)string->exciter_last_output;
  *p->energy = (MYFLT)string->exciter_energy;
  *p->peak = (MYFLT)string->exciter_peak;
  *p->rng = (MYFLT)string->exciter_rng;
  *p->pending_impact = pending_impact ? FL(1.0) : FL(0.0);
  *p->normal_pizzicato_attacks =
      (MYFLT)string->normal_pizzicato_attacks;
  *p->left_hand_pizzicato_attacks =
      (MYFLT)string->left_hand_pizzicato_attacks;
  *p->bartok_pizzicato_attacks =
      (MYFLT)string->bartok_pizzicato_attacks;
  *p->fingerboard_impacts = (MYFLT)string->fingerboard_impacts;
  *p->wood_strikes = (MYFLT)string->wood_strikes;
  *p->wood_tratto_attacks = (MYFLT)string->wood_tratto_attacks;
  *p->wood_tratto_samples = (MYFLT)string->wood_tratto_samples;
  *p->collision_samples = (MYFLT)string->collision_samples;
  *p->collision_compression = (MYFLT)string->collision_compression;
  *p->collision_force = (MYFLT)string->collision_force;
  *p->exciter_samples = (MYFLT)string->exciter_samples;
  *p->recoveries = (MYFLT)string->exciter_recoveries;
  *p->finite = finite ? FL(1.0) : FL(0.0);
  csound->UnlockMutex(string->lock);
  csound->UnlockMutex(state->state_lock);
  return OK;
}

static int32_t wg_double_bass_test_exciter_fault_init(
    CSOUND *csound, WG_DOUBLE_BASS_TEST_EXCITER_FAULT *p)
{
  const int32_t handle = wg_double_bass_read_handle(p->idouble_bass);
  const double requested_string = p->istring != NULL ?
      (double)*p->istring : WG_DOUBLE_BASS_NAN;
  const double requested_mode = p->imode != NULL ?
      (double)*p->imode : WG_DOUBLE_BASS_NAN;
  WG_DOUBLE_BASS_STATE *state;
  WG_DOUBLE_BASS_STRING_STATE *string;

  *p->result = FL(0.0);
  if (handle < 1 || !isfinite(requested_string) || requested_string < 1.0 ||
      requested_string > 4.0 || floor(requested_string) != requested_string ||
      !isfinite(requested_mode) || requested_mode < 1.0 ||
      requested_mode > 3.0 || floor(requested_mode) != requested_mode) {
    return csound->InitError(
        csound,
        "hlolli_wg_double_bass_test_exciter_fault: invalid handle, string, or mode\n");
  }
  state = wg_double_bass_get_by_handle(csound, handle);
  if (state == NULL) {
    return csound->InitError(
        csound,
        "hlolli_wg_double_bass_test_exciter_fault: unknown double-bass handle %d\n",
        handle);
  }
  csound->LockMutex(state->state_lock);
  string = &state->strings[(uint32_t)requested_string - 1U];
  csound->LockMutex(string->lock);
  if (requested_mode == 1.0) {
    string->exciter_amplitude = WG_DOUBLE_BASS_NAN;
  } else if (requested_mode == 2.0) {
    string->collision_compression = WG_DOUBLE_BASS_NAN;
  } else {
    string->exciter_contact = WG_DOUBLE_BASS_NAN;
  }
  *p->result = FL(1.0);
  csound->UnlockMutex(string->lock);
  csound->UnlockMutex(state->state_lock);
  return OK;
}

static int32_t wg_double_bass_test_waveguide_init(
    CSOUND *csound, WG_DOUBLE_BASS_TEST_WAVEGUIDE *p)
{
  const int32_t handle = wg_double_bass_read_handle(p->idouble_bass);
  const double requested_string = p->istring != NULL ?
      (double)*p->istring : WG_DOUBLE_BASS_NAN;
  WG_DOUBLE_BASS_STATE *state;
  WG_DOUBLE_BASS_STRING_STATE *string;
  double nut_energy = 0.0;
  double bridge_energy = 0.0;
  double nut_signature = 0.0;
  double bridge_signature = 0.0;
  double loop_phase = 0.0;
  int32_t finite = 1;
  uint32_t offset;

  if (handle < 1 || !isfinite(requested_string) || requested_string < 1.0 ||
      requested_string > 4.0 || floor(requested_string) != requested_string) {
    return csound->InitError(
        csound,
        "hlolli_wg_double_bass_test_waveguide: invalid handle or string\n");
  }
  state = wg_double_bass_get_by_handle(csound, handle);
  if (state == NULL) {
    return csound->InitError(
        csound,
        "hlolli_wg_double_bass_test_waveguide: unknown double-bass handle %d\n",
        handle);
  }
  csound->LockMutex(state->state_lock);
  string = &state->strings[(uint32_t)requested_string - 1U];
  csound->LockMutex(string->lock);
  if (string->rail_size == 0U || string->toward_nut == NULL ||
      string->toward_bridge == NULL ||
      string->second_toward_nut == NULL ||
      string->second_toward_bridge == NULL) {
    finite = 0;
  } else {
    for (offset = 0U; offset < string->rail_size; offset++) {
      const uint32_t nut_index = (uint32_t)(
          ((uint64_t)string->nut_write_index + offset) % string->rail_size);
      const uint32_t bridge_index = (uint32_t)(
          ((uint64_t)string->bridge_write_index + offset) %
          string->rail_size);
      const double weight = (double)(offset + 1U) /
          (double)string->rail_size;
      const double nut = string->toward_nut[nut_index];
      const double bridge = string->toward_bridge[bridge_index];
      const double second_nut = string->second_toward_nut[nut_index];
      const double second_bridge =
          string->second_toward_bridge[bridge_index];
      if (!isfinite(nut) || !isfinite(bridge) ||
          !isfinite(second_nut) || !isfinite(second_bridge)) {
        finite = 0;
      }
      if (fabs(nut) > WG_DOUBLE_BASS_WAVE_LIMIT ||
          fabs(bridge) > WG_DOUBLE_BASS_WAVE_LIMIT ||
          fabs(second_nut) > WG_DOUBLE_BASS_WAVE_LIMIT ||
          fabs(second_bridge) > WG_DOUBLE_BASS_WAVE_LIMIT) {
        finite = 0;
      }
      nut_energy += nut * nut;
      bridge_energy += bridge * bridge;
      nut_signature += weight * nut;
      bridge_signature += weight * bridge;
    }
    nut_energy /= (double)string->rail_size;
    bridge_energy /= (double)string->rail_size;
    nut_signature /= (double)string->rail_size;
    bridge_signature /= (double)string->rail_size;
  }
  if (string->target_frequency > 0.0) {
    loop_phase = wg_double_bass_loop_phase_samples(
        string, string->target_delay, string->delay_position,
        string->delay_tuning_frequency,
        state->sample_rate);
  }
  if (!isfinite(string->delay) || !isfinite(string->target_delay) ||
      !isfinite(string->target_frequency) ||
      !isfinite(string->delay_tuning_frequency) || !isfinite(loop_phase) ||
      !isfinite(nut_energy) || !isfinite(bridge_energy) ||
      !isfinite(nut_signature) || !isfinite(bridge_signature) ||
      !isfinite(string->nut_pole) || !isfinite(string->nut_gain) ||
      !isfinite(string->nut_state) || !isfinite(string->bridge_pole) ||
      !isfinite(string->bridge_gain) || !isfinite(string->bridge_state) ||
      !isfinite(string->second_delay) ||
      !isfinite(string->second_bridge_delay) ||
      !isfinite(string->second_nut_delay) ||
      !isfinite(string->second_nut_pole) ||
      !isfinite(string->second_nut_gain) ||
      !isfinite(string->second_nut_gain_target) ||
      !isfinite(string->second_nut_state) ||
      !isfinite(string->second_bridge_pole) ||
      !isfinite(string->second_bridge_gain) ||
      !isfinite(string->second_bridge_gain_target) ||
      !isfinite(string->second_bridge_state) ||
      !isfinite(string->release_loop_gain) ||
      !isfinite(string->release_loop_gain_target) ||
      !isfinite(string->release_gain_smoothing) ||
      !isfinite(string->release_model_speed) ||
      !isfinite(string->bridge_dynamic_pole) ||
      !isfinite(string->bridge_dynamic_input) ||
      !isfinite(string->bridge_dynamic_output) ||
      !isfinite(string->last_bridge_output) ||
      !isfinite(string->bridge_delay) || !isfinite(string->nut_delay) ||
      !isfinite(string->target_bridge_delay) ||
      !isfinite(string->target_nut_delay) ||
      string->bridge_write_index >= string->rail_size ||
      string->nut_write_index >= string->rail_size ||
      (string->delay_initialized &&
       (string->delay < 2.0 ||
        string->delay > (double)string->rail_size - 2.0 ||
        string->target_delay < 2.0 ||
        string->target_delay > (double)string->rail_size - 2.0 ||
        string->bridge_delay < WG_DOUBLE_BASS_MIN_BRANCH_DELAY ||
        string->nut_delay < WG_DOUBLE_BASS_MIN_BRANCH_DELAY ||
        string->second_delay < 2.0 ||
        string->second_delay > (double)string->rail_size - 2.0 ||
        string->second_bridge_delay < WG_DOUBLE_BASS_MIN_BRANCH_DELAY ||
        string->second_nut_delay < WG_DOUBLE_BASS_MIN_BRANCH_DELAY ||
        string->bridge_delay > (double)string->rail_size - 2.0 ||
        string->nut_delay > (double)string->rail_size - 2.0 ||
        string->second_bridge_delay > (double)string->rail_size - 2.0 ||
        string->second_nut_delay > (double)string->rail_size - 2.0)) ||
      (!string->delay_initialized &&
       (string->delay != 0.0 || string->target_delay != 0.0 ||
        string->target_frequency != 0.0)) ||
      string->nut_pole < 0.0 || string->nut_pole >= 1.0 ||
      string->bridge_pole < 0.0 || string->bridge_pole >= 1.0 ||
      string->second_nut_pole < 0.0 || string->second_nut_pole >= 1.0 ||
      string->second_bridge_pole < 0.0 ||
      string->second_bridge_pole >= 1.0 ||
      string->bridge_dynamic_pole < 0.0 ||
      string->bridge_dynamic_pole >= 1.0 ||
      string->nut_gain < 0.0 || string->nut_gain >= 1.0 ||
      string->bridge_gain < 0.0 || string->bridge_gain >= 1.0 ||
      string->second_nut_gain < 0.0 ||
      string->second_nut_gain >= 1.0 ||
      string->second_bridge_gain < 0.0 ||
      string->second_bridge_gain >= 1.0) {
    finite = 0;
  }
  if (string->release_loop_gain < 0.0 ||
      string->release_loop_gain > 1.0 ||
      string->release_loop_gain_target < 0.0 ||
      string->release_loop_gain_target > 1.0 ||
      string->release_gain_smoothing <= 0.0 ||
      string->release_gain_smoothing > 1.0 ||
      string->release_model_speed < -1.0 ||
      string->release_model_speed > 1.0 ||
      string->release_model_articulation >
          WG_DOUBLE_BASS_ARTICULATION_MAX ||
      (string->release_gate_positive != 0 &&
       string->release_gate_positive != 1)) {
    finite = 0;
  }
  *p->capacity = (MYFLT)string->rail_size;
  *p->read_delay = (MYFLT)string->delay;
  *p->target_delay = (MYFLT)string->target_delay;
  *p->target_frequency = (MYFLT)string->target_frequency;
  *p->loop_phase_samples = (MYFLT)loop_phase;
  *p->toward_nut_energy = (MYFLT)nut_energy;
  *p->toward_bridge_energy = (MYFLT)bridge_energy;
  *p->toward_nut_signature = (MYFLT)nut_signature;
  *p->toward_bridge_signature = (MYFLT)bridge_signature;
  *p->nut_pole = (MYFLT)string->nut_pole;
  *p->nut_gain = (MYFLT)string->nut_gain;
  *p->nut_state = (MYFLT)string->nut_state;
  *p->bridge_pole = (MYFLT)string->bridge_pole;
  *p->bridge_gain = (MYFLT)string->bridge_gain;
  *p->bridge_state = (MYFLT)string->bridge_state;
  *p->write_index = (MYFLT)string->bridge_write_index;
  *p->wave_samples = (MYFLT)string->wave_samples;
  *p->excitation_count = (MYFLT)string->excitation_count;
  *p->clear_count = (MYFLT)string->wave_clears;
  *p->reset_count = (MYFLT)string->wave_resets;
  *p->clip_count = (MYFLT)string->wave_clips;
  *p->bridge_dynamic_input = (MYFLT)string->bridge_dynamic_input;
  *p->bridge_dynamic_output = (MYFLT)string->bridge_dynamic_output;
  *p->last_bridge_output = (MYFLT)string->last_bridge_output;
  *p->finite = finite ? FL(1.0) : FL(0.0);
  csound->UnlockMutex(string->lock);
  csound->UnlockMutex(state->state_lock);
  return OK;
}

static int32_t wg_double_bass_test_bow_init(
    CSOUND *csound, WG_DOUBLE_BASS_TEST_BOW *p)
{
  const int32_t handle = wg_double_bass_read_handle(p->idouble_bass);
  const double requested_string = p->istring != NULL ?
      (double)*p->istring : WG_DOUBLE_BASS_NAN;
  WG_DOUBLE_BASS_STATE *state;
  WG_DOUBLE_BASS_STRING_STATE *string;
  double effective_position;
  int32_t finite = 1;

  if (handle < 1 || !isfinite(requested_string) || requested_string < 1.0 ||
      requested_string > 4.0 || floor(requested_string) != requested_string) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_test_bow: invalid handle or string\n");
  }
  state = wg_double_bass_get_by_handle(csound, handle);
  if (state == NULL) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_test_bow: unknown double-bass handle %d\n",
        handle);
  }
  csound->LockMutex(state->state_lock);
  string = &state->strings[(uint32_t)requested_string - 1U];
  csound->LockMutex(string->lock);
  effective_position = string->bridge_delay /
      fmax(2.0, string->bridge_delay + string->nut_delay);
  if (string->bow_state > WG_DOUBLE_BASS_BOW_SCRATCH ||
      !isfinite(string->bow_requested_force) ||
      !isfinite(string->bow_effective_force) ||
      !isfinite(string->bow_speed) ||
      !isfinite(string->hair_effective_speed) ||
      !isfinite(string->bow_contact) ||
      !isfinite(string->bow_position_target) ||
      !isfinite(effective_position) ||
      !isfinite(string->bridge_delay) || !isfinite(string->nut_delay) ||
      !isfinite(string->characteristic_impedance) ||
      !isfinite(string->bow_free_velocity) ||
      !isfinite(string->bow_string_velocity) ||
      !isfinite(string->bow_relative_velocity) ||
      !isfinite(string->bow_friction_force) ||
      !isfinite(string->bow_junction_increment) ||
      !isfinite(string->bow_min_force) ||
      !isfinite(string->bow_max_force) ||
      !isfinite(string->bow_solver_residual) ||
      !isfinite(string->bow_solver_bracket_width) ||
      !isfinite(string->bow_scratch_score) ||
      !isfinite(string->bow_max_abs_relative) ||
      string->bow_requested_force < 0.0 ||
      string->bow_effective_force < 0.0 ||
      string->bow_min_force < 0.0 ||
      string->bow_max_force < string->bow_min_force ||
      string->bow_effective_force > string->bow_requested_force + 1.0e-12 ||
      string->bow_contact < 0.0 || string->bow_contact > 1.0 ||
      string->bow_position_target < 0.01 ||
      string->bow_position_target > 0.49 ||
      effective_position < 0.0 || effective_position > 0.5 ||
      string->bridge_delay < WG_DOUBLE_BASS_MIN_BRANCH_DELAY ||
      string->nut_delay < WG_DOUBLE_BASS_MIN_BRANCH_DELAY ||
      !(string->characteristic_impedance > 0.0) ||
      string->bow_root_count > 4U ||
      string->bow_solver_iterations > WG_DOUBLE_BASS_BOW_SOLVE_STEPS ||
      string->bow_solver_iterations_max > WG_DOUBLE_BASS_BOW_SOLVE_STEPS ||
      string->bow_solver_residual < 0.0 ||
      string->bow_solver_bracket_width < 0.0 ||
      string->bow_scratch_score < 0.0 ||
      string->bow_scratch_score > 1.0 + 1.0e-12 ||
      fabs(string->bow_string_velocity -
           (string->bow_free_velocity + string->bow_junction_increment)) >
          1.0e-9 * (1.0 + fabs(string->bow_string_velocity)) ||
      fabs(string->bow_relative_velocity -
           (string->hair_effective_speed -
            string->bow_string_velocity)) >
          1.0e-9 * (1.0 + fabs(string->bow_relative_velocity)) ||
      fabs(string->bow_junction_increment) >
          WG_DOUBLE_BASS_FRICTION_MAX_SPEED + 1.0e-9) {
    finite = 0;
  }
  *p->state = (MYFLT)string->bow_state;
  *p->requested_force = (MYFLT)string->bow_requested_force;
  *p->effective_force = (MYFLT)string->bow_effective_force;
  *p->bow_speed = (MYFLT)string->hair_effective_speed;
  *p->contact = (MYFLT)string->bow_contact;
  *p->target_position = (MYFLT)string->bow_position_target;
  *p->effective_position = (MYFLT)effective_position;
  *p->bridge_delay = (MYFLT)string->bridge_delay;
  *p->nut_delay = (MYFLT)string->nut_delay;
  *p->impedance = (MYFLT)string->characteristic_impedance;
  *p->free_velocity = (MYFLT)string->bow_free_velocity;
  *p->string_velocity = (MYFLT)string->bow_string_velocity;
  *p->relative_velocity = (MYFLT)string->bow_relative_velocity;
  *p->friction_force = (MYFLT)string->bow_friction_force;
  *p->junction_increment = (MYFLT)string->bow_junction_increment;
  *p->min_force = (MYFLT)string->bow_min_force;
  *p->max_force = (MYFLT)string->bow_max_force;
  *p->residual = (MYFLT)string->bow_solver_residual;
  *p->bracket_width = (MYFLT)string->bow_solver_bracket_width;
  *p->root_count = (MYFLT)string->bow_root_count;
  *p->iterations_last = (MYFLT)string->bow_solver_iterations;
  *p->iterations_max = (MYFLT)string->bow_solver_iterations_max;
  *p->solver_calls = (MYFLT)string->bow_solver_calls;
  *p->solver_failures = (MYFLT)string->bow_solver_failures;
  *p->solver_fallbacks = (MYFLT)string->bow_solver_fallbacks;
  *p->stick_samples = (MYFLT)string->bow_stick_samples;
  *p->slip_samples = (MYFLT)string->bow_slip_samples;
  *p->scratch_samples = (MYFLT)string->bow_scratch_samples;
  *p->no_motion_samples = (MYFLT)string->bow_no_motion_samples;
  *p->state_transitions = (MYFLT)string->bow_state_transitions;
  *p->direction_changes = (MYFLT)string->bow_direction_changes;
  *p->attacks = (MYFLT)string->excitation_count;
  *p->recoveries = (MYFLT)string->bow_recoveries;
  *p->scratch_score = (MYFLT)string->bow_scratch_score;
  *p->max_abs_relative = (MYFLT)string->bow_max_abs_relative;
  *p->finite = finite ? FL(1.0) : FL(0.0);
  csound->UnlockMutex(string->lock);
  csound->UnlockMutex(state->state_lock);
  return OK;
}

static int32_t wg_double_bass_test_bow_fault_init(
    CSOUND *csound, WG_DOUBLE_BASS_TEST_BOW_FAULT *p)
{
  const int32_t handle = wg_double_bass_read_handle(p->idouble_bass);
  const double requested_string = p->istring != NULL ?
      (double)*p->istring : WG_DOUBLE_BASS_NAN;
  const double requested_mode = p->imode != NULL ?
      (double)*p->imode : WG_DOUBLE_BASS_NAN;
  WG_DOUBLE_BASS_STATE *state;
  WG_DOUBLE_BASS_STRING_STATE *string;

  *p->result = FL(0.0);
  if (handle < 1 || !isfinite(requested_string) || requested_string < 1.0 ||
      requested_string > 4.0 || floor(requested_string) != requested_string ||
      !isfinite(requested_mode) || requested_mode < 1.0 ||
      requested_mode > 3.0 || floor(requested_mode) != requested_mode) {
    return csound->InitError(
        csound,
        "hlolli_wg_double_bass_test_bow_fault: invalid handle, string, or mode\n");
  }
  state = wg_double_bass_get_by_handle(csound, handle);
  if (state == NULL) {
    return csound->InitError(
        csound,
        "hlolli_wg_double_bass_test_bow_fault: unknown double-bass handle %d\n",
        handle);
  }
  csound->LockMutex(state->state_lock);
  string = &state->strings[(uint32_t)requested_string - 1U];
  csound->LockMutex(string->lock);
  if (requested_mode == 1.0) {
    string->bow_speed = WG_DOUBLE_BASS_NAN;
  } else if (requested_mode == 2.0) {
    string->bow_scratch_score = WG_DOUBLE_BASS_NAN;
  } else {
    string->bow_previous_slip_speed = WG_DOUBLE_BASS_NAN;
  }
  *p->result = FL(1.0);
  csound->UnlockMutex(string->lock);
  csound->UnlockMutex(state->state_lock);
  return OK;
}

static int32_t wg_double_bass_test_gesture_init(
    CSOUND *csound, WG_DOUBLE_BASS_TEST_GESTURE *p)
{
  const int32_t handle = wg_double_bass_read_handle(p->idouble_bass);
  const double requested_string = p->istring != NULL ?
      (double)*p->istring : WG_DOUBLE_BASS_NAN;
  WG_DOUBLE_BASS_STATE *state;
  WG_DOUBLE_BASS_STRING_STATE *string;
  int32_t finite;

  if (handle < 1 || !isfinite(requested_string) || requested_string < 1.0 ||
      requested_string > 4.0 || floor(requested_string) != requested_string) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_test_gesture: invalid handle or string\n");
  }
  state = wg_double_bass_get_by_handle(csound, handle);
  if (state == NULL) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_test_gesture: unknown double-bass handle %d\n",
        handle);
  }
  csound->LockMutex(state->state_lock);
  string = &state->strings[(uint32_t)requested_string - 1U];
  csound->LockMutex(string->lock);
  finite = wg_double_bass_gesture_live_is_finite(string) &&
      (string->gesture_active == 0 || string->gesture_active == 1) &&
      (string->gesture_bow_change_active == 0 ||
       string->gesture_bow_change_active == 1);
  *p->articulation = (MYFLT)string->gesture_articulation;
  *p->active = string->gesture_active ? FL(1.0) : FL(0.0);
  *p->phase = (MYFLT)string->gesture_phase;
  *p->force_target = (MYFLT)string->gesture_force_target;
  *p->speed_target = (MYFLT)string->gesture_speed_target;
  *p->force_output = (MYFLT)string->gesture_force_output;
  *p->speed_output = (MYFLT)string->gesture_speed_output;
  *p->contact_output = (MYFLT)string->gesture_contact_output;
  *p->onset_samples = (MYFLT)string->gesture_onset_samples;
  *p->stroke_samples = (MYFLT)string->gesture_stroke_samples;
  *p->strokes = (MYFLT)string->gesture_strokes;
  *p->bow_changes = (MYFLT)string->bow_direction_changes;
  *p->preset_changes = (MYFLT)string->gesture_preset_changes;
  *p->direct_overrides = (MYFLT)string->gesture_direct_overrides;
  *p->recoveries = (MYFLT)string->gesture_recoveries;
  *p->release_articulation =
      (MYFLT)string->release_model_articulation;
  *p->release_gain_target = (MYFLT)string->release_loop_gain_target;
  *p->finite = finite ? FL(1.0) : FL(0.0);
  csound->UnlockMutex(string->lock);
  csound->UnlockMutex(state->state_lock);
  return OK;
}

static int32_t wg_double_bass_test_gesture_fault_init(
    CSOUND *csound, WG_DOUBLE_BASS_TEST_GESTURE_FAULT *p)
{
  const int32_t handle = wg_double_bass_read_handle(p->idouble_bass);
  const double requested_string = p->istring != NULL ?
      (double)*p->istring : WG_DOUBLE_BASS_NAN;
  const double requested_mode = p->imode != NULL ?
      (double)*p->imode : WG_DOUBLE_BASS_NAN;
  WG_DOUBLE_BASS_STATE *state;
  WG_DOUBLE_BASS_STRING_STATE *string;

  *p->result = FL(0.0);
  if (handle < 1 || !isfinite(requested_string) || requested_string < 1.0 ||
      requested_string > 4.0 || floor(requested_string) != requested_string ||
      !isfinite(requested_mode) || requested_mode < 1.0 ||
      requested_mode > 3.0 || floor(requested_mode) != requested_mode) {
    return csound->InitError(
        csound,
        "hlolli_wg_double_bass_test_gesture_fault: invalid handle, string, or mode\n");
  }
  state = wg_double_bass_get_by_handle(csound, handle);
  if (state == NULL) {
    return csound->InitError(
        csound,
        "hlolli_wg_double_bass_test_gesture_fault: unknown double-bass handle %d\n",
        handle);
  }
  csound->LockMutex(state->state_lock);
  string = &state->strings[(uint32_t)requested_string - 1U];
  csound->LockMutex(string->lock);
  if ((uint32_t)requested_mode == 1U) {
    string->gesture_phase = WG_DOUBLE_BASS_NAN;
  } else if ((uint32_t)requested_mode == 2U) {
    string->gesture_transition_mix = WG_DOUBLE_BASS_NAN;
  } else {
    string->gesture_force_output = WG_DOUBLE_BASS_NAN;
  }
  csound->UnlockMutex(string->lock);
  csound->UnlockMutex(state->state_lock);
  *p->result = FL(1.0);
  return OK;
}

static int32_t wg_double_bass_test_physics_init(
    CSOUND *csound, WG_DOUBLE_BASS_TEST_PHYSICS *p)
{
  const int32_t handle = wg_double_bass_read_handle(p->idouble_bass);
  const double requested_string = p->istring != NULL
      ? (double)*p->istring : WG_DOUBLE_BASS_NAN;
  WG_DOUBLE_BASS_STATE *state;
  WG_DOUBLE_BASS_STRING_STATE *string;
  int32_t finite;

  if (handle < 1 || !isfinite(requested_string) ||
      requested_string < 1.0 || requested_string > 4.0 ||
      floor(requested_string) != requested_string) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_test_physics: invalid handle or string\n");
  }
  state = wg_double_bass_get_by_handle(csound, handle);
  if (state == NULL) {
    return csound->InitError(
        csound,
        "hlolli_wg_double_bass_test_physics: unknown double-bass handle %d\n",
        handle);
  }
  csound->LockMutex(state->state_lock);
  string = &state->strings[(uint32_t)requested_string - 1U];
  csound->LockMutex(string->lock);
  finite = wg_double_bass_bow_mechanics_is_finite(string);
  *p->contact_memory = (MYFLT)string->contact_memory;
  *p->contact_memory_velocity =
      (MYFLT)string->contact_memory_velocity;
  *p->contact_adhesion = (MYFLT)string->contact_adhesion;
  *p->contact_steady_displacement =
      (MYFLT)string->contact_steady_displacement;
  *p->contact_force = (MYFLT)string->contact_force;
  *p->contact_iterations = (MYFLT)string->contact_solver_iterations;
  *p->contact_failures = (MYFLT)string->contact_solver_failures;
  *p->temperature = (MYFLT)string->contact_temperature;
  *p->thermal_scale = (MYFLT)string->contact_thermal_scale;
  *p->thermal_work = (MYFLT)string->contact_thermal_work;
  *p->hair_displacement = (MYFLT)string->hair_displacement;
  *p->hair_velocity = (MYFLT)string->hair_velocity;
  *p->hair_effective_speed = (MYFLT)string->hair_effective_speed;
  *p->board_displacement = (MYFLT)string->board_displacement;
  *p->board_compression = (MYFLT)string->board_compression;
  *p->board_force = (MYFLT)string->board_force;
  *p->board_energy = (MYFLT)string->board_energy;
  *p->board_impacts = (MYFLT)string->board_impacts;
  *p->noise_last = (MYFLT)string->bow_noise_last;
  *p->noise_energy = (MYFLT)string->bow_noise_energy;
  *p->noise_peak = (MYFLT)string->bow_noise_peak;
  *p->noise_samples = (MYFLT)string->bow_noise_samples;
  *p->mechanical_events = (MYFLT)string->mechanical_events;
  *p->coupling_input = (MYFLT)string->coupling_input;
  *p->coupling_energy = (MYFLT)string->coupling_energy;
  *p->coupling_peak = (MYFLT)string->coupling_peak;
  *p->coupling_samples = (MYFLT)string->coupling_samples;
  *p->coupling_source_mask = (MYFLT)string->coupling_source_mask;
  *p->recoveries = (MYFLT)string->bow_mechanics_recoveries;
  *p->second_polarization = (MYFLT)
      wg_double_bass_bound_string_model(string)->second_polarization.mix;
  *p->finite = finite ? FL(1.0) : FL(0.0);
  csound->UnlockMutex(string->lock);
  csound->UnlockMutex(state->state_lock);
  return OK;
}

static int32_t wg_double_bass_test_physics_fault_init(
    CSOUND *csound, WG_DOUBLE_BASS_TEST_PHYSICS_FAULT *p)
{
  const int32_t handle = wg_double_bass_read_handle(p->idouble_bass);
  const double requested_string = p->istring != NULL
      ? (double)*p->istring : WG_DOUBLE_BASS_NAN;
  const double requested_mode = p->imode != NULL
      ? (double)*p->imode : WG_DOUBLE_BASS_NAN;
  WG_DOUBLE_BASS_STATE *state;
  WG_DOUBLE_BASS_STRING_STATE *string;

  *p->result = FL(0.0);
  if (handle < 1 || !isfinite(requested_string) ||
      requested_string < 1.0 || requested_string > 4.0 ||
      floor(requested_string) != requested_string ||
      !isfinite(requested_mode) || requested_mode < 1.0 ||
      requested_mode > 6.0 || floor(requested_mode) != requested_mode) {
    return csound->InitError(
        csound,
        "hlolli_wg_double_bass_test_physics_fault: invalid input\n");
  }
  state = wg_double_bass_get_by_handle(csound, handle);
  if (state == NULL) {
    return csound->InitError(
        csound,
        "hlolli_wg_double_bass_test_physics_fault: unknown double-bass handle %d\n",
        handle);
  }
  csound->LockMutex(state->state_lock);
  string = &state->strings[(uint32_t)requested_string - 1U];
  csound->LockMutex(string->lock);
  switch ((uint32_t)requested_mode) {
    case 1U:
      string->contact_memory = WG_DOUBLE_BASS_NAN;
      break;
    case 2U:
      string->contact_temperature = WG_DOUBLE_BASS_NAN;
      break;
    case 3U:
      string->hair_displacement = WG_DOUBLE_BASS_NAN;
      break;
    case 4U:
      string->board_displacement = WG_DOUBLE_BASS_NAN;
      break;
    case 5U:
      string->bow_noise_highpass = WG_DOUBLE_BASS_NAN;
      break;
    default:
      string->coupling_energy = WG_DOUBLE_BASS_NAN;
      break;
  }
  *p->result = FL(1.0);
  csound->UnlockMutex(string->lock);
  csound->UnlockMutex(state->state_lock);
  return OK;
}

static int32_t wg_double_bass_test_strange_init(
    CSOUND *csound, WG_DOUBLE_BASS_TEST_STRANGE *p)
{
  const int32_t handle = wg_double_bass_read_handle(p->idouble_bass);
  const double requested_string = p->istring != NULL
      ? (double)*p->istring : WG_DOUBLE_BASS_NAN;
  const uint64_t epoch = csound->GetEngineKcounter(csound);
  uint32_t sample = p->h.insdshead->ksmps_offset;
  WG_DOUBLE_BASS_STATE *state;
  WG_DOUBLE_BASS_STRING_STATE *string;

  if (handle < 1 || !isfinite(requested_string) ||
      requested_string < 1.0 || requested_string > 4.0 ||
      floor(requested_string) != requested_string) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_test_strange: invalid handle or string\n");
  }
  state = wg_double_bass_get_by_handle(csound, handle);
  if (state == NULL) {
    return csound->InitError(
        csound,
        "hlolli_wg_double_bass_test_strange: unknown double-bass handle %d\n",
        handle);
  }
  if (sample >= state->ksmps) {
    sample = 0U;
  }
  csound->LockMutex(state->state_lock);
  string = &state->strings[(uint32_t)requested_string - 1U];
  csound->LockMutex(string->lock);
  *p->strange = (MYFLT)string->strange;
  *p->sympathetic = (MYFLT)state->sympathetic;
  *p->sympathetic_applied = (MYFLT)wg_double_bass_sympathetic_control_at(
      state, epoch, sample);
  *p->air_state = (MYFLT)string->strange_air_state;
  *p->dispersion_input = (MYFLT)string->strange_dispersion_input;
  *p->dispersion_output = (MYFLT)string->strange_dispersion_output;
  *p->subharmonic_phase = (MYFLT)string->strange_subharmonic_phase;
  *p->subharmonic_envelope =
      (MYFLT)string->strange_subharmonic_envelope;
  *p->squeal_state = (MYFLT)string->strange_squeal_state;
  *p->squeal_frequency = (MYFLT)wg_double_bass_strange_squeal_frequency(
      string->effective_frequency, state->sample_rate);
  *p->coupling_mix = (MYFLT)string->strange_coupling_mix;
  *p->last_output = (MYFLT)string->strange_last_output;
  *p->energy = (MYFLT)string->strange_energy;
  *p->peak = (MYFLT)string->strange_peak;
  *p->samples = (MYFLT)string->strange_samples;
  *p->recoveries = (MYFLT)string->strange_recoveries;
  *p->passive_active = string->passive_open_active ? FL(1.0) : FL(0.0);
  *p->passive_level = (MYFLT)string->passive_open_level;
  *p->passive_energy = (MYFLT)string->passive_open_energy;
  *p->passive_peak = (MYFLT)string->passive_open_peak;
  *p->passive_samples = (MYFLT)string->passive_open_samples;
  *p->finite = wg_double_bass_sympathetic_state_is_finite(string)
      ? FL(1.0) : FL(0.0);
  csound->UnlockMutex(string->lock);
  csound->UnlockMutex(state->state_lock);
  return OK;
}

static int32_t wg_double_bass_test_strange_fault_init(
    CSOUND *csound, WG_DOUBLE_BASS_TEST_STRANGE_FAULT *p)
{
  const int32_t handle = wg_double_bass_read_handle(p->idouble_bass);
  const double requested_string = p->istring != NULL
      ? (double)*p->istring : WG_DOUBLE_BASS_NAN;
  const double requested_mode = p->imode != NULL
      ? (double)*p->imode : WG_DOUBLE_BASS_NAN;
  WG_DOUBLE_BASS_STATE *state;
  WG_DOUBLE_BASS_STRING_STATE *string;

  *p->result = FL(0.0);
  if (handle < 1 || !isfinite(requested_string) ||
      requested_string < 1.0 || requested_string > 4.0 ||
      floor(requested_string) != requested_string ||
      !isfinite(requested_mode) || requested_mode < 1.0 ||
      requested_mode > 7.0 || floor(requested_mode) != requested_mode) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_test_strange_fault: invalid input\n");
  }
  state = wg_double_bass_get_by_handle(csound, handle);
  if (state == NULL) {
    return csound->InitError(
        csound,
        "hlolli_wg_double_bass_test_strange_fault: unknown double-bass handle %d\n",
        handle);
  }
  csound->LockMutex(state->state_lock);
  string = &state->strings[(uint32_t)requested_string - 1U];
  csound->LockMutex(string->lock);
  switch ((uint32_t)requested_mode) {
    case 1U:
      string->strange_air_state = WG_DOUBLE_BASS_NAN;
      break;
    case 2U:
      string->strange_dispersion_output = WG_DOUBLE_BASS_NAN;
      break;
    case 3U:
      string->strange_subharmonic_envelope = WG_DOUBLE_BASS_NAN;
      break;
    case 4U:
      string->strange_squeal_state = WG_DOUBLE_BASS_NAN;
      break;
    case 5U:
      string->strange_coupling_mix = WG_DOUBLE_BASS_NAN;
      break;
    case 6U:
      string->strange_energy = WG_DOUBLE_BASS_NAN;
      break;
    default:
      string->passive_open_level = WG_DOUBLE_BASS_NAN;
      break;
  }
  *p->result = FL(1.0);
  csound->UnlockMutex(string->lock);
  csound->UnlockMutex(state->state_lock);
  return OK;
}

static int32_t wg_double_bass_test_string_impulse_init(
    CSOUND *csound, WG_DOUBLE_BASS_TEST_STRING_IMPULSE *p)
{
  const int32_t handle = wg_double_bass_read_handle(p->idouble_bass);
  const double requested_string = p->istring != NULL ?
      (double)*p->istring : WG_DOUBLE_BASS_NAN;
  const double frequency = p->ifrequency != NULL ?
      (double)*p->ifrequency : WG_DOUBLE_BASS_NAN;
  const double amplitude = p->iamplitude != NULL ?
      (double)*p->iamplitude : WG_DOUBLE_BASS_NAN;
  WG_DOUBLE_BASS_STATE *state;
  WG_DOUBLE_BASS_STRING_STATE *string;
  uint32_t delay_whole;
  uint32_t read_index;
  uint32_t sample;

  *p->result = FL(0.0);
  if (handle < 1 || !isfinite(requested_string) || requested_string < 1.0 ||
      requested_string > 4.0 || floor(requested_string) != requested_string ||
      !isfinite(frequency) ||
      !isfinite(amplitude) || fabs(amplitude) > 1.0) {
    return csound->InitError(
        csound,
        "hlolli_wg_double_bass_test_string_impulse: invalid test input\n");
  }
  state = wg_double_bass_get_by_handle(csound, handle);
  if (state == NULL) {
    return csound->InitError(
        csound,
        "hlolli_wg_double_bass_test_string_impulse: unknown double-bass handle %d\n",
        handle);
  }
  if (!wg_double_bass_string_can_play(
          state, (uint32_t)requested_string - 1U, frequency)) {
    return csound->InitError(
        csound,
        "hlolli_wg_double_bass_test_string_impulse: fundamental is outside the playable range\n");
  }
  csound->LockMutex(state->state_lock);
  string = &state->strings[(uint32_t)requested_string - 1U];
  csound->LockMutex(string->lock);
  string->frequency = frequency;
  wg_double_bass_prepare_harmonic(
      state, string, 0.0, string->frequency);
  wg_double_bass_prepare_waveguide(string, string->frequency, state->sample_rate);
  delay_whole = (uint32_t)floor(string->nut_delay);
  read_index = (uint32_t)(
      ((uint64_t)string->nut_write_index + string->rail_size -
       delay_whole) % string->rail_size);
  static const double shape[4] = {0.5, 1.0, -1.0, -0.5};

  for (sample = 0U; sample < 4U; sample++) {
    const uint32_t index = (read_index + sample) % string->rail_size;
    string->toward_nut[index] = wg_double_bass_clamp(
        string->toward_nut[index] + amplitude * shape[sample],
        -WG_DOUBLE_BASS_WAVE_LIMIT, WG_DOUBLE_BASS_WAVE_LIMIT);
  }
  string->excitation_count = wg_double_bass_add_samples(
      string->excitation_count, 1U);
  *p->result = FL(1.0);
  csound->UnlockMutex(string->lock);
  csound->UnlockMutex(state->state_lock);
  return OK;
}

static int32_t wg_double_bass_test_renderer_init(
    CSOUND *csound, WG_DOUBLE_BASS_TEST_RENDERER *p)
{
  const int32_t handle = wg_double_bass_read_handle(p->idouble_bass);
  WG_DOUBLE_BASS_STATE *state;

  if (handle < 1) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_test_renderer: invalid handle\n");
  }
  state = wg_double_bass_get_by_handle(csound, handle);
  if (state == NULL) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_test_renderer: unknown double-bass handle %d\n",
        handle);
  }
  csound->LockMutex(state->resonance_lock);
  *p->body = (MYFLT)state->body;
  *p->sympathetic = (MYFLT)state->sympathetic;
  *p->mute = (MYFLT)state->mute;
  *p->activity = (MYFLT)state->resonance_activity;
  *p->last_update_sample = (MYFLT)state->last_render_sample;
  *p->gap_samples = (MYFLT)state->renderer_gap_samples;
  *p->owner = state->renderer_owner != NULL ? FL(1.0) : FL(0.0);
  *p->successor = state->renderer_owner != state->renderer_tail ?
      FL(1.0) : FL(0.0);
  csound->UnlockMutex(state->resonance_lock);
  return OK;
}

static int32_t wg_double_bass_test_body_init(
    CSOUND *csound, WG_DOUBLE_BASS_TEST_BODY *p)
{
  const int32_t handle = wg_double_bass_read_handle(p->idouble_bass);
  WG_DOUBLE_BASS_STATE *state;
  double modal_energy = 0.0;
  double signature = 0.0;
  int32_t finite;
  uint32_t mode_index;
  uint32_t string_index;

  if (handle < 1) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_test_body: invalid handle\n");
  }
  state = wg_double_bass_get_by_handle(csound, handle);
  if (state == NULL) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_test_body: unknown double-bass handle %d\n",
        handle);
  }
  csound->LockMutex(state->resonance_lock);
  csound->LockMutex(state->state_lock);
  finite = wg_double_bass_body_state_is_finite(state) &&
      isfinite(state->body_drive_energy) &&
      isfinite(state->body_output_energy_left) &&
      isfinite(state->body_output_energy_right) &&
      isfinite(state->body_peak_left) &&
      isfinite(state->body_peak_right);
  for (mode_index = 0U; mode_index < WG_DOUBLE_BASS_BODY_MODES;
       mode_index++) {
    const WG_DOUBLE_BASS_BODY_MODE_STATE *mode =
        &state->body_modes[mode_index];
    const double weight = (double)mode_index + 1.0;
    modal_energy += mode->real * mode->real +
        mode->imaginary * mode->imaginary;
    signature += weight * mode->real +
        (weight + 0.5) * mode->imaginary;
  }
  *p->body = (MYFLT)state->body;
  *p->sympathetic = (MYFLT)state->sympathetic;
  *p->mute = (MYFLT)state->mute;
  *p->mode_count = (MYFLT)WG_DOUBLE_BASS_BODY_MODES;
  *p->last_drive = (MYFLT)state->body_last_drive;
  *p->drive_energy = (MYFLT)state->body_drive_energy;
  *p->modal_energy = (MYFLT)modal_energy;
  *p->state_signature = (MYFLT)signature;
  *p->last_left = (MYFLT)state->body_last_left;
  *p->last_right = (MYFLT)state->body_last_right;
  *p->output_energy_left = (MYFLT)state->body_output_energy_left;
  *p->output_energy_right = (MYFLT)state->body_output_energy_right;
  *p->peak_left = (MYFLT)state->body_peak_left;
  *p->peak_right = (MYFLT)state->body_peak_right;
  *p->processed_samples = (MYFLT)state->body_samples;
  *p->fast_forward_samples = (MYFLT)state->body_fast_forward_samples;
  *p->reset_count = (MYFLT)state->body_resets;
  *p->clip_count = (MYFLT)state->body_clips;
  for (string_index = 0U; string_index < WG_DOUBLE_BASS_STRINGS;
       string_index++) {
    WG_DOUBLE_BASS_STRING_STATE *string = &state->strings[string_index];
    MYFLT *send_samples;
    MYFLT *send_energy;

    csound->LockMutex(string->lock);
    finite = finite && isfinite(string->bridge_send_energy) &&
        isfinite(string->bridge_send_peak);
    switch (string_index) {
      case 0U:
        send_samples = p->send_samples_1;
        send_energy = p->send_energy_1;
        break;
      case 1U:
        send_samples = p->send_samples_2;
        send_energy = p->send_energy_2;
        break;
      case 2U:
        send_samples = p->send_samples_3;
        send_energy = p->send_energy_3;
        break;
      default:
        send_samples = p->send_samples_4;
        send_energy = p->send_energy_4;
        break;
    }
    *send_samples = (MYFLT)string->bridge_send_samples;
    *send_energy = (MYFLT)string->bridge_send_energy;
    csound->UnlockMutex(string->lock);
  }
  *p->finite = finite ? FL(1.0) : FL(0.0);
  csound->UnlockMutex(state->state_lock);
  csound->UnlockMutex(state->resonance_lock);
  return OK;
}

static int32_t wg_double_bass_test_body_mode_init(
    CSOUND *csound, WG_DOUBLE_BASS_TEST_BODY_MODE *p)
{
  const int32_t handle = wg_double_bass_read_handle(p->idouble_bass);
  const double requested_mode = p->imode != NULL ?
      (double)*p->imode : WG_DOUBLE_BASS_NAN;
  WG_DOUBLE_BASS_STATE *state;
  WG_DOUBLE_BASS_BODY_MODE_STATE *mode;
  const WG_DOUBLE_BASS_BODY_MODE_SPEC *spec;
  uint32_t mode_index;

  if (handle < 1 || !isfinite(requested_mode) || requested_mode < 1.0 ||
      requested_mode > (double)WG_DOUBLE_BASS_BODY_MODES ||
      floor(requested_mode) != requested_mode) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_test_body_mode: invalid input\n");
  }
  state = wg_double_bass_get_by_handle(csound, handle);
  if (state == NULL) {
    return csound->InitError(
        csound,
        "hlolli_wg_double_bass_test_body_mode: unknown double-bass handle %d\n",
        handle);
  }
  mode_index = (uint32_t)requested_mode - 1U;
  spec = &state->model->body.modes[mode_index];
  csound->LockMutex(state->resonance_lock);
  mode = &state->body_modes[mode_index];
  *p->frequency = (MYFLT)spec->frequency;
  *p->base_bandwidth = (MYFLT)spec->bandwidth;
  *p->effective_bandwidth = (MYFLT)mode->effective_bandwidth;
  *p->radius = (MYFLT)mode->radius;
  *p->gain = (MYFLT)mode->gain;
  *p->pan = (MYFLT)spec->pan;
  *p->real = (MYFLT)mode->real;
  *p->imaginary = (MYFLT)mode->imaginary;
  *p->finite = isfinite(mode->real) && isfinite(mode->imaginary) &&
          isfinite(mode->radius) && isfinite(mode->gain) &&
          mode->radius >= 0.0 && mode->radius < 1.0
      ? FL(1.0) : FL(0.0);
  csound->UnlockMutex(state->resonance_lock);
  return OK;
}

static int32_t wg_double_bass_test_body_fault_init(
    CSOUND *csound, WG_DOUBLE_BASS_TEST_BODY_FAULT *p)
{
  const int32_t handle = wg_double_bass_read_handle(p->idouble_bass);
  const double requested_mode = p->imode != NULL ?
      (double)*p->imode : WG_DOUBLE_BASS_NAN;
  WG_DOUBLE_BASS_STATE *state;

  *p->result = FL(0.0);
  if (handle < 1 || !isfinite(requested_mode) || requested_mode < 1.0 ||
      requested_mode > (double)WG_DOUBLE_BASS_BODY_MODES ||
      floor(requested_mode) != requested_mode) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_test_body_fault: invalid input\n");
  }
  state = wg_double_bass_get_by_handle(csound, handle);
  if (state == NULL) {
    return csound->InitError(
        csound,
        "hlolli_wg_double_bass_test_body_fault: unknown double-bass handle %d\n",
        handle);
  }
  csound->LockMutex(state->resonance_lock);
  state->body_modes[(uint32_t)requested_mode - 1U].real = WG_DOUBLE_BASS_NAN;
  csound->UnlockMutex(state->resonance_lock);
  *p->result = FL(1.0);
  return OK;
}

static int32_t wg_double_bass_test_bridge_impulse_init(
    CSOUND *csound, WG_DOUBLE_BASS_TEST_BRIDGE_IMPULSE *p)
{
  const int32_t handle = wg_double_bass_read_handle(p->idouble_bass);
  const double requested_string = p->istring != NULL ?
      (double)*p->istring : WG_DOUBLE_BASS_NAN;
  const double amplitude = p->iamplitude != NULL ?
      (double)*p->iamplitude : WG_DOUBLE_BASS_NAN;
  const uint64_t epoch = csound->GetEngineKcounter(csound);
  WG_DOUBLE_BASS_STATE *state;
  WG_DOUBLE_BASS_STRING_STATE *string;
  uint32_t sample;

  *p->result = FL(0.0);
  if (handle < 1 || !isfinite(requested_string) ||
      requested_string < 1.0 || requested_string > 4.0 ||
      floor(requested_string) != requested_string ||
      !isfinite(amplitude) || fabs(amplitude) > WG_DOUBLE_BASS_BODY_DRIVE_LIMIT) {
    return csound->InitError(
        csound, "hlolli_wg_double_bass_test_bridge_impulse: invalid input\n");
  }
  state = wg_double_bass_get_by_handle(csound, handle);
  if (state == NULL) {
    return csound->InitError(
        csound,
        "hlolli_wg_double_bass_test_bridge_impulse: unknown double-bass handle %d\n",
        handle);
  }
  sample = p->h.insdshead->ksmps_offset;
  if (sample >= state->ksmps) {
    sample = 0U;
  }
  csound->LockMutex(state->state_lock);
  string = &state->strings[(uint32_t)requested_string - 1U];
  csound->LockMutex(string->lock);
  wg_double_bass_prepare_bridge_send(string, epoch, state->ksmps);
  wg_double_bass_write_bridge_send(string, epoch, sample, amplitude);
  csound->UnlockMutex(string->lock);
  csound->UnlockMutex(state->state_lock);
  *p->result = FL(1.0);
  return OK;
}
#endif

static OENTRY localops[] = {
    {"hlolli_wg_double_bass_create", sizeof(WG_DOUBLE_BASS_CREATE), 0,
     "i", "", (SUBR)wg_double_bass_create_init, NULL, NULL, NULL, 0},
    {"hlolli_wg_double_bass_create", sizeof(WG_DOUBLE_BASS_CREATE_TUNED), 0,
     "i", "i", (SUBR)wg_double_bass_create_tuned_init, NULL, NULL, NULL, 0},
    {"hlolli_wg_double_bass", sizeof(WG_DOUBLE_BASS_VOICE), 0,
     "aa", "kkkkkkkkkkii", (SUBR)wg_double_bass_voice_init,
     (SUBR)wg_double_bass_voice_perf, (SUBR)wg_double_bass_release_voice, NULL, 0},
    {"hlolli_wg_double_bass_resonance", sizeof(WG_DOUBLE_BASS_RESONANCE), _CW,
     "aa", "ikkk", (SUBR)wg_double_bass_resonance_init,
     (SUBR)wg_double_bass_resonance_perf,
     (SUBR)wg_double_bass_release_renderer, NULL, 0},
#if defined(HLOLLI_WG_DOUBLE_BASS_TEST_API)
    {"hlolli_wg_double_bass_test_diagnostic_gains",
     sizeof(WG_DOUBLE_BASS_TEST_DIAGNOSTIC_GAINS), 0,
     "i", "iiiiiiii", (SUBR)wg_double_bass_test_diagnostic_gains_init,
     NULL, NULL, NULL, 0},
    {"hlolli_wg_double_bass_test_string", sizeof(WG_DOUBLE_BASS_TEST_STRING), 0,
     "iiiiiiiiiii", "ii", (SUBR)wg_double_bass_test_string_init,
     NULL, NULL, NULL, 0},
    {"hlolli_wg_double_bass_test_finger", sizeof(WG_DOUBLE_BASS_TEST_FINGER), 0,
     "iiiiiiiiiiii" "iiiiii", "ii", (SUBR)wg_double_bass_test_finger_init,
     NULL, NULL, NULL, 0},
    {"hlolli_wg_double_bass_test_harmonic", sizeof(WG_DOUBLE_BASS_TEST_HARMONIC), 0,
     "iiiiiiiiiiii" "iiiiiii", "ii", (SUBR)wg_double_bass_test_harmonic_init,
     NULL, NULL, NULL, 0},
    {"hlolli_wg_double_bass_test_exciter", sizeof(WG_DOUBLE_BASS_TEST_EXCITER), 0,
     "iiiiiiiiiiii" "iiiiiiiiiiii" "iiiiiii", "ii",
     (SUBR)wg_double_bass_test_exciter_init, NULL, NULL, NULL, 0},
    {"hlolli_wg_double_bass_test_exciter_fault",
     sizeof(WG_DOUBLE_BASS_TEST_EXCITER_FAULT), 0,
     "i", "iii", (SUBR)wg_double_bass_test_exciter_fault_init,
     NULL, NULL, NULL, 0},
    {"hlolli_wg_double_bass_test_waveguide",
     sizeof(WG_DOUBLE_BASS_TEST_WAVEGUIDE), 0,
     "iiiiiiiiiiii" "iiiiiiiiiiii" "i", "ii",
     (SUBR)wg_double_bass_test_waveguide_init, NULL, NULL, NULL, 0},
    {"hlolli_wg_double_bass_test_bow", sizeof(WG_DOUBLE_BASS_TEST_BOW), 0,
     "iiiiiiiiiiii" "iiiiiiiiiiii" "iiiiiiiiiiii", "ii",
     (SUBR)wg_double_bass_test_bow_init, NULL, NULL, NULL, 0},
    {"hlolli_wg_double_bass_test_bow_fault",
     sizeof(WG_DOUBLE_BASS_TEST_BOW_FAULT), 0,
     "i", "iii", (SUBR)wg_double_bass_test_bow_fault_init,
     NULL, NULL, NULL, 0},
    {"hlolli_wg_double_bass_test_gesture", sizeof(WG_DOUBLE_BASS_TEST_GESTURE), 0,
     "iiiiiiiiiiii" "iiiiii", "ii", (SUBR)wg_double_bass_test_gesture_init,
     NULL, NULL, NULL, 0},
    {"hlolli_wg_double_bass_test_gesture_fault",
     sizeof(WG_DOUBLE_BASS_TEST_GESTURE_FAULT), 0,
     "i", "iii", (SUBR)wg_double_bass_test_gesture_fault_init,
     NULL, NULL, NULL, 0},
    {"hlolli_wg_double_bass_test_physics", sizeof(WG_DOUBLE_BASS_TEST_PHYSICS), 0,
     "iiiiiiiiiiii" "iiiiiiiiiiii" "iiiiiii", "ii",
     (SUBR)wg_double_bass_test_physics_init, NULL, NULL, NULL, 0},
    {"hlolli_wg_double_bass_test_physics_fault",
     sizeof(WG_DOUBLE_BASS_TEST_PHYSICS_FAULT), 0,
     "i", "iii", (SUBR)wg_double_bass_test_physics_fault_init,
     NULL, NULL, NULL, 0},
    {"hlolli_wg_double_bass_test_strange", sizeof(WG_DOUBLE_BASS_TEST_STRANGE), 0,
     "iiiiiiiiiiii" "iiiiiiiiii", "ii",
     (SUBR)wg_double_bass_test_strange_init, NULL, NULL, NULL, 0},
    {"hlolli_wg_double_bass_test_strange_fault",
     sizeof(WG_DOUBLE_BASS_TEST_STRANGE_FAULT), 0,
     "i", "iii", (SUBR)wg_double_bass_test_strange_fault_init,
     NULL, NULL, NULL, 0},
    {"hlolli_wg_double_bass_test_string_impulse",
     sizeof(WG_DOUBLE_BASS_TEST_STRING_IMPULSE), 0,
     "i", "iiii", (SUBR)wg_double_bass_test_string_impulse_init,
     NULL, NULL, NULL, 0},
    {"hlolli_wg_double_bass_test_renderer", sizeof(WG_DOUBLE_BASS_TEST_RENDERER), 0,
     "iiiiiiii", "i", (SUBR)wg_double_bass_test_renderer_init,
     NULL, NULL, NULL, 0},
    {"hlolli_wg_double_bass_test_body", sizeof(WG_DOUBLE_BASS_TEST_BODY), 0,
     "iiiiiiiiiiii" "iiiiiiiiiiii" "iii", "i",
     (SUBR)wg_double_bass_test_body_init, NULL, NULL, NULL, 0},
    {"hlolli_wg_double_bass_test_body_mode",
     sizeof(WG_DOUBLE_BASS_TEST_BODY_MODE), 0,
     "iiiiiiiii", "ii", (SUBR)wg_double_bass_test_body_mode_init,
     NULL, NULL, NULL, 0},
    {"hlolli_wg_double_bass_test_body_fault",
     sizeof(WG_DOUBLE_BASS_TEST_BODY_FAULT), 0,
     "i", "ii", (SUBR)wg_double_bass_test_body_fault_init,
     NULL, NULL, NULL, 0},
    {"hlolli_wg_double_bass_test_bridge_impulse",
     sizeof(WG_DOUBLE_BASS_TEST_BRIDGE_IMPULSE), 0,
     "i", "iii", (SUBR)wg_double_bass_test_bridge_impulse_init,
     NULL, NULL, NULL, 0},
#endif
};

LINKAGE
