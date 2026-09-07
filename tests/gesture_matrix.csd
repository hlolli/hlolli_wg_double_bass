<CsoundSynthesizer>
<CsOptions>
-d -m128
</CsOptions>
<CsInstruments>

#ifndef TEST_CASE
#define TEST_CASE #1#
#endif
#ifndef TEST_STRING
#define TEST_STRING #1#
#endif
#ifndef TEST_FREQUENCY
#define TEST_FREQUENCY #41.20344461410875#
#endif
#ifndef TEST_FREQUENCY2
#define TEST_FREQUENCY2 #61.80516692116312#
#endif
#ifndef TEST_ARTICULATION
#define TEST_ARTICULATION #0#
#endif
#ifndef TEST_HARMONIC
#define TEST_HARMONIC #0#
#endif
#ifndef TEST_FORCE
#define TEST_FORCE #0.50#
#endif
#ifndef TEST_SPEED
#define TEST_SPEED #0.45#
#endif
#ifndef TEST_POSITION
#define TEST_POSITION #0.12#
#endif

sr = 48000
ksmps = 32
nchnls = 2
0dbfs = 1

giBass hlolli_wg_double_bass_create

instr Voice
  kTime timeinsts
  kGate = kTime < 0.28 ? 1 : 0
  aLeft, aRight hlolli_wg_double_bass \
      kGate, $TEST_FREQUENCY, $TEST_FORCE, $TEST_SPEED, $TEST_POSITION, \
      0, 0, $TEST_ARTICULATION, $TEST_HARMONIC, 0, \
      $TEST_STRING, giBass
  outs 0.04 * aLeft, 0.04 * aRight
endin

instr Slur
  kTime timeinsts
  kGate = kTime < 0.45 ? 1 : 0
  kFrequency linseg $TEST_FREQUENCY, 0.24, $TEST_FREQUENCY2, 0.36, $TEST_FREQUENCY2
  aLeft, aRight hlolli_wg_double_bass \
      kGate, kFrequency, 0.50, 0.45, 0.12, \
      0, 0, 0, 0, 0, $TEST_STRING, giBass
  outs 0.04 * aLeft, 0.04 * aRight
endin

instr PresetSequence
  kTime timeinsts
  kArticulation = kTime < 0.10 ? 0 : \
      (kTime < 0.20 ? 1 : \
      (kTime < 0.30 ? 2 : \
      (kTime < 0.40 ? 3 : 4)))
  aLeft, aRight hlolli_wg_double_bass \
      1, $TEST_FREQUENCY, 0.50, 0.45, 0.12, \
      0, 0, kArticulation, 0, 0, $TEST_STRING, giBass
  outs 0.04 * aLeft, 0.04 * aRight
endin

instr ControlSequence
  kTime timeinsts
  if kTime < 0.16 then
    kForce = 0.25
    kSpeed = 0.25
    kPosition = 0.08
  elseif kTime < 0.32 then
    kForce = 0.75
    kSpeed = 0.25
    kPosition = 0.08
  else
    kForce = 0.75
    kSpeed = -0.65
    kPosition = 0.20
  endif
  aLeft, aRight hlolli_wg_double_bass \
      1, $TEST_FREQUENCY, kForce, kSpeed, kPosition, \
      0, 0, 1, 0, 0, $TEST_STRING, giBass
  outs 0.04 * aLeft, 0.04 * aRight
endin

instr Renderer
  aLeft, aRight hlolli_wg_double_bass_resonance giBass, 0.55, 0.6, 0
  outs 0.25 * aLeft, 0.25 * aRight
endin

instr Probe
  iG01, iG02, iG03, iG04, iG05, iG06, iG07, iG08, \
      iG09, iG10, iG11, iG12, iG13, iG14, iG15, iG16, \
      iG17, iG18 hlolli_wg_double_bass_test_gesture giBass, p5
  printf_i "WG_MATRIX_GESTURE %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n", \
      1, p4, p5, iG01, iG02, iG03, iG04, iG05, iG06, iG07, \
      iG08, iG09, iG10, iG11, iG12, iG13, iG14, iG15, iG16, iG17, iG18

  iE01, iE02, iE03, iE04, iE05, iE06, iE07, iE08, \
      iE09, iE10, iE11, iE12, iE13, iE14, iE15, iE16, \
      iE17, iE18, iE19, iE20, iE21, iE22, iE23, iE24, \
      iE25, iE26, iE27, iE28, iE29, iE30, iE31 \
      hlolli_wg_double_bass_test_exciter giBass, p5
  printf_i "WG_MATRIX_EXCITER %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n", \
      1, p4, p5, iE01, iE02, iE04, iE09, iE15, iE16, iE19, iE20, \
      iE21, iE22, iE23, iE24, iE25, iE26, iE29, iE30, iE31

  iB01, iB02, iB03, iB04, iB05, iB06, iB07, iB08, \
      iB09, iB10, iB11, iB12, iB13, iB14, iB15, iB16, \
      iB17, iB18, iB19, iB20, iB21, iB22, iB23, iB24, \
      iB25, iB26, iB27, iB28, iB29, iB30, iB31, iB32, \
      iB33, iB34, iB35, iB36 \
      hlolli_wg_double_bass_test_bow giBass, p5
  printf_i "WG_MATRIX_BOW %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n", \
      1, p4, p5, iB24, iB25, iB32, iB33, iB36, iB05, iB06, iB07

  iW01, iW02, iW03, iW04, iW05, iW06, iW07, iW08, \
      iW09, iW10, iW11, iW12, iW13, iW14, iW15, iW16, \
      iW17, iW18, iW19, iW20, iW21, iW22, iW23, iW24, iW25 \
      hlolli_wg_double_bass_test_waveguide giBass, p5
  printf_i "WG_MATRIX_WAVE %.17g %.17g %.17g %.17g %.17g\n", \
      1, p4, p5, iW18, iW21, iW25

  iH01, iH02, iH03, iH04, iH05, iH06, iH07, iH08, \
      iH09, iH10, iH11, iH12, iH13, iH14, iH15, iH16, \
      iH17, iH18, iH19 hlolli_wg_double_bass_test_harmonic giBass, p5
  printf_i "WG_MATRIX_HARMONIC %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n", \
      1, p4, p5, iH01, iH02, iH15, iH16, iH19

  iF01, iF02, iF03, iF04, iF05, iF06, iF07, iF08, iF09, \
      iF10, iF11, iF12, iF13, iF14, iF15, iF16, iF17, iF18 \
      hlolli_wg_double_bass_test_finger giBass, p5
  printf_i "WG_MATRIX_FINGER %.17g %.17g %.17g %.17g %.17g\n", \
      1, p4, p5, iF08, iF15, iF18
endin

instr Schedule
  if $TEST_CASE == 1 then
    event_i "i", "Voice", 0, 0.55
    event_i "i", "Renderer", 0, 0.55
    event_i "i", "Probe", 0.18, 0.001, 1, $TEST_STRING
    event_i "i", "Probe", 0.44, 0.001, 2, $TEST_STRING
  elseif $TEST_CASE == 2 then
    event_i "i", "Slur", 0, 0.60
    event_i "i", "Renderer", 0, 0.60
    event_i "i", "Probe", 0.08, 0.001, 1, $TEST_STRING
    event_i "i", "Probe", 0.36, 0.001, 2, $TEST_STRING
    event_i "i", "Probe", 0.52, 0.001, 3, $TEST_STRING
  elseif $TEST_CASE == 3 then
    event_i "i", "PresetSequence", 0, 0.55
    event_i "i", "Renderer", 0, 0.55
    event_i "i", "Probe", 0.08, 0.001, 1, $TEST_STRING
    event_i "i", "Probe", 0.18, 0.001, 2, $TEST_STRING
    event_i "i", "Probe", 0.28, 0.001, 3, $TEST_STRING
    event_i "i", "Probe", 0.38, 0.001, 4, $TEST_STRING
    event_i "i", "Probe", 0.48, 0.001, 5, $TEST_STRING
  elseif $TEST_CASE == 4 then
    event_i "i", "ControlSequence", 0, 0.50
    event_i "i", "Renderer", 0, 0.50
    event_i "i", "Probe", 0.12, 0.001, 1, $TEST_STRING
    event_i "i", "Probe", 0.25, 0.001, 2, $TEST_STRING
    event_i "i", "Probe", 0.42, 0.001, 3, $TEST_STRING
  endif
endin

</CsInstruments>
<CsScore>
i "Schedule" 0 0.001
e
</CsScore>
</CsoundSynthesizer>
