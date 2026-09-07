<CsoundSynthesizer>
<CsOptions>
-d -m128
</CsOptions>
<CsInstruments>

#ifndef TEST_SR
#define TEST_SR #48000#
#endif
#ifndef TEST_KSMPS
#define TEST_KSMPS #32#
#endif
#ifndef TEST_SECONDS
#define TEST_SECONDS #0.75#
#endif
#ifndef TEST_MODE
#define TEST_MODE #1#
#endif
#ifndef TEST_ENABLE_1
#define TEST_ENABLE_1 #1#
#endif
#ifndef TEST_ENABLE_2
#define TEST_ENABLE_2 #1#
#endif
#ifndef TEST_ENABLE_3
#define TEST_ENABLE_3 #1#
#endif
#ifndef TEST_ENABLE_4
#define TEST_ENABLE_4 #1#
#endif

sr = $TEST_SR
ksmps = $TEST_KSMPS
nchnls = 2
0dbfs = 1

; Release-build runtime matrix. TEST_MODE=1 drives deliberately out-of-range
; controls through the public clamps; TEST_MODE=2 holds an ordinary arco note.
giBass hlolli_wg_double_bass_create

opcode StressVoice, aa, ii
  iString, iFrequency xin
  kTime timeinsts
  if $TEST_MODE == 1 then
    kPhase phasor 3.1 + 0.17 * iString, 0.13 * iString
    kSlow phasor 0.43 + 0.03 * iString, 0.07 * iString
    kShape = 1 - abs(2 * kPhase - 1)
    kFrequency = iFrequency * (1 + 1.2 * (1 - abs(2 * kSlow - 1)))
    kTrigger = frac(kTime * (9.0 + 0.3 * iString)) < 0.76 ? 1 : 0
    kForce = -0.35 + 1.70 * kShape
    kSpeed = -1.40 + 2.80 * kPhase
    kPosition = -0.10 + 0.70 * kShape
    kArticulation = int(frac(kTime * 23) * 9)
    kStrange = -1.30 + 2.60 * kSlow
  else
    kTrigger = kTime < 0.8 * $TEST_SECONDS ? 1 : 0
    kFrequency = iFrequency
    kForce = 0.48
    kSpeed = -0.34
    kPosition = 0.12
    kArticulation = 0
    kStrange = 0
  endif
  aLeft, aRight hlolli_wg_double_bass \
      kTrigger, kFrequency, kForce, kSpeed, kPosition, \
      18, 9, kArticulation, 0, kStrange, iString, giBass
  xout 0.018 * aLeft, 0.018 * aRight
endop

instr Voice1
  aLeft, aRight StressVoice 1, 41.20344461410875
  outs aLeft, aRight
endin

instr Voice2
  aLeft, aRight StressVoice 2, 55
  outs aLeft, aRight
endin

instr Voice3
  aLeft, aRight StressVoice 3, 73.41619197935188
  outs aLeft, aRight
endin

instr Voice4
  aLeft, aRight StressVoice 4, 97.99885899543733
  outs aLeft, aRight
endin

instr Renderer
  kBody = $TEST_MODE == 1 ? 1.25 : 0.72
  kSympathetic = $TEST_MODE == 1 ? 1.40 : 0.55
  kMute = $TEST_MODE == 1 ? -0.20 : 0
  aLeft, aRight hlolli_wg_double_bass_resonance \
      giBass, kBody, kSympathetic, kMute
  outs 0.12 * aLeft, 0.12 * aRight
endin

instr Probe
  iW01, iW02, iW03, iW04, iW05, iW06, iW07, iW08, \
      iW09, iW10, iW11, iW12, iW13, iW14, iW15, iW16, \
      iW17, iW18, iW19, iW20, iW21, iW22, iW23, iW24, iW25 \
      hlolli_wg_double_bass_test_waveguide giBass, p4

  iB01, iB02, iB03, iB04, iB05, iB06, iB07, iB08, \
      iB09, iB10, iB11, iB12, iB13, iB14, iB15, iB16, \
      iB17, iB18, iB19, iB20, iB21, iB22, iB23, iB24, \
      iB25, iB26, iB27, iB28, iB29, iB30, iB31, iB32, \
      iB33, iB34, iB35, iB36 hlolli_wg_double_bass_test_bow giBass, p4

  iG01, iG02, iG03, iG04, iG05, iG06, iG07, iG08, \
      iG09, iG10, iG11, iG12, iG13, iG14, iG15, iG16, \
      iG17, iG18 hlolli_wg_double_bass_test_gesture giBass, p4

  iE01, iE02, iE03, iE04, iE05, iE06, iE07, iE08, \
      iE09, iE10, iE11, iE12, iE13, iE14, iE15, iE16, \
      iE17, iE18, iE19, iE20, iE21, iE22, iE23, iE24, \
      iE25, iE26, iE27, iE28, iE29, iE30, iE31 \
      hlolli_wg_double_bass_test_exciter giBass, p4

  iP01, iP02, iP03, iP04, iP05, iP06, iP07, iP08, \
      iP09, iP10, iP11, iP12, iP13, iP14, iP15, iP16, \
      iP17, iP18, iP19, iP20, iP21, iP22, iP23, iP24, \
      iP25, iP26, iP27, iP28, iP29, iP30, iP31 \
      hlolli_wg_double_bass_test_physics giBass, p4

  iR01, iR02, iR03, iR04, iR05, iR06, iR07, iR08, \
      iR09, iR10, iR11, iR12, iR13, iR14, iR15, iR16, \
      iR17, iR18, iR19, iR20, iR21, iR22 \
      hlolli_wg_double_bass_test_strange giBass, p4

  printf_i "WG_RELEASE_STRING %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n", \
      1, p4, p5, iW17, iW18, iW21, iW25, iB24, iB25, iB33, iB36, \
      iG15, iG18, iE30, iE31, iP07, iP29, iP31, iR16, iR22
endin

instr BodyProbe
  iY01, iY02, iY03, iY04, iY05, iY06, iY07, iY08, \
      iY09, iY10, iY11, iY12, iY13, iY14, iY15, iY16, \
      iY17, iY18, iY19, iY20, iY21, iY22, iY23, iY24, \
      iY25, iY26, iY27 hlolli_wg_double_bass_test_body giBass
  printf_i "WG_RELEASE_BODY %.17g %.17g %.17g %.17g\n", \
      1, iY23, iY25, iY26, iY27
endin

instr Schedule
  if $TEST_ENABLE_1 == 1 then
    event_i "i", "Voice1", 0, $TEST_SECONDS
  endif
  if $TEST_ENABLE_2 == 1 then
    event_i "i", "Voice2", 0, $TEST_SECONDS
  endif
  if $TEST_ENABLE_3 == 1 then
    event_i "i", "Voice3", 0, $TEST_SECONDS
  endif
  if $TEST_ENABLE_4 == 1 then
    event_i "i", "Voice4", 0, $TEST_SECONDS
  endif
  event_i "i", "Renderer", 0, $TEST_SECONDS
  event_i "i", "Probe", $TEST_SECONDS - 0.02, 0.001, 1, $TEST_ENABLE_1
  event_i "i", "Probe", $TEST_SECONDS - 0.02, 0.001, 2, $TEST_ENABLE_2
  event_i "i", "Probe", $TEST_SECONDS - 0.02, 0.001, 3, $TEST_ENABLE_3
  event_i "i", "Probe", $TEST_SECONDS - 0.02, 0.001, 4, $TEST_ENABLE_4
  event_i "i", "BodyProbe", $TEST_SECONDS - 0.02, 0.001
endin

</CsInstruments>
<CsScore>
i "Schedule" 0 0.001
e
</CsScore>
</CsoundSynthesizer>
