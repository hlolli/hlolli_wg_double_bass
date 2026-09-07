<CsoundSynthesizer>
<CsOptions>
-d -m128
</CsOptions>
<CsInstruments>

#ifndef TEST_KIND
#define TEST_KIND #1#
#endif
#ifndef TEST_MODE
#define TEST_MODE #1#
#endif

sr = 48000
ksmps = 32
nchnls = 2
0dbfs = 1

; The fixed data and sound remain violin-derived. Fault opcodes belong to the
; private test API; the public signal must return to finite bounded output.
giBass hlolli_wg_double_bass_create

instr 1
  iArticulation = 1
  if $TEST_KIND == 1 then
    if $TEST_MODE == 1 then
      iArticulation = 5
    elseif $TEST_MODE == 2 then
      iArticulation = 7
    else
      iArticulation = 8
    endif
  endif
  kPhase phasor 1.1
  kShape = 1 - abs(2 * kPhase - 1)
  kFrequency = 330 * (0.99 + 0.02 * kShape)
  kForce = 0.34 + 0.18 * kShape
  kSpeed = (0.38 + 0.08 * kShape) * (kPhase < 0.5 ? 1 : -1)
  aLeft, aRight hlolli_wg_double_bass \
      1, kFrequency, kForce, kSpeed, 0.12, \
      8, 5.1, iArticulation, 0, 0.4, 1, giBass
  outs 0.3 * aLeft, 0.3 * aRight
endin

instr 2
  aLeft, aRight hlolli_wg_double_bass_resonance \
      giBass, 0.7, 0.4, 0.05
  outs 0.4 * aLeft, 0.4 * aRight
endin

instr 3
  iFaultString = 1
  if $TEST_KIND == 5 && $TEST_MODE == 7 then
    iFaultString = 2
  endif
  if $TEST_KIND == 1 then
    iResult hlolli_wg_double_bass_test_exciter_fault \
        giBass, iFaultString, $TEST_MODE
  elseif $TEST_KIND == 2 then
    iResult hlolli_wg_double_bass_test_bow_fault \
        giBass, iFaultString, $TEST_MODE
  elseif $TEST_KIND == 3 then
    iResult hlolli_wg_double_bass_test_gesture_fault \
        giBass, iFaultString, $TEST_MODE
  elseif $TEST_KIND == 4 then
    iResult hlolli_wg_double_bass_test_physics_fault \
        giBass, iFaultString, $TEST_MODE
  elseif $TEST_KIND == 5 then
    iResult hlolli_wg_double_bass_test_strange_fault \
        giBass, iFaultString, $TEST_MODE
  else
    iResult hlolli_wg_double_bass_test_body_fault giBass, $TEST_MODE
  endif
  printf_i "WG_FAULT %d %d %.17g\n", \
      1, $TEST_KIND, $TEST_MODE, iResult
endin

instr 4
  iProbeString = 1
  if $TEST_KIND == 5 && $TEST_MODE == 7 then
    iProbeString = 2
  endif
  if $TEST_KIND == 1 then
    iE01, iE02, iE03, iE04, iE05, iE06, iE07, iE08, \
        iE09, iE10, iE11, iE12, iE13, iE14, iE15, iE16, \
        iE17, iE18, iE19, iE20, iE21, iE22, iE23, iE24, \
        iE25, iE26, iE27, iE28, iE29, iRecoveries, iFinite \
        hlolli_wg_double_bass_test_exciter giBass, 1
  elseif $TEST_KIND == 2 then
    iB01, iB02, iB03, iB04, iB05, iB06, iB07, iB08, \
        iB09, iB10, iB11, iB12, iB13, iB14, iB15, iB16, \
        iB17, iB18, iB19, iB20, iB21, iB22, iB23, iB24, \
        iB25, iB26, iB27, iB28, iB29, iB30, iB31, iB32, \
        iRecoveries, iB34, iB35, iFinite \
        hlolli_wg_double_bass_test_bow giBass, 1
  elseif $TEST_KIND == 3 then
    iG01, iG02, iG03, iG04, iG05, iG06, iG07, iG08, \
        iG09, iG10, iG11, iG12, iG13, iG14, iRecoveries, \
        iG16, iG17, iFinite \
        hlolli_wg_double_bass_test_gesture giBass, 1
  elseif $TEST_KIND == 4 then
    iP01, iP02, iP03, iP04, iP05, iP06, iP07, iP08, \
        iP09, iP10, iP11, iP12, iP13, iP14, iP15, iP16, \
        iP17, iP18, iP19, iP20, iP21, iP22, iP23, iP24, \
        iP25, iP26, iP27, iP28, iRecoveries, iP30, iFinite \
        hlolli_wg_double_bass_test_physics giBass, 1
  elseif $TEST_KIND == 5 then
    iS01, iS02, iS03, iS04, iS05, iS06, iS07, iS08, \
        iS09, iS10, iS11, iS12, iS13, iS14, iS15, \
        iRecoveries, iS17, iS18, iS19, iS20, iS21, iFinite \
        hlolli_wg_double_bass_test_strange giBass, iProbeString
  else
    iY01, iY02, iY03, iY04, iY05, iY06, iY07, iY08, \
        iY09, iY10, iY11, iY12, iY13, iY14, iY15, iY16, \
        iY17, iY18, iY19, iY20, iY21, iY22, iY23, iY24, \
        iRecoveries, iY26, iFinite \
        hlolli_wg_double_bass_test_body giBass
  endif
  printf_i "WG_RECOVERY %d %d %.17g %.17g\n", \
      1, $TEST_KIND, $TEST_MODE, iRecoveries, iFinite
endin

instr Schedule
  event_i "i", 1, 0, 0.14
  event_i "i", 2, 0, 0.14
  event_i "i", 3, 0.05, 0.001
  ; The probe runs 240 samples after the injected fault.
  event_i "i", 4, 0.055, 0.001
endin

</CsInstruments>
<CsScore>
i "Schedule" 0 0.001
e 0.14
</CsScore>
</CsoundSynthesizer>
