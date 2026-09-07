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
#ifndef TEST_A4
#define TEST_A4 #440#
#endif
#ifndef TEST_CASE
#define TEST_CASE #1#
#endif
#ifndef TEST_STRING
#define TEST_STRING #1#
#endif
#ifndef TEST_PROBE_STRING
#define TEST_PROBE_STRING #1#
#endif
#ifndef TEST_DRIVE_STRING
#define TEST_DRIVE_STRING #1#
#endif
#ifndef TEST_FREQUENCY
#define TEST_FREQUENCY #41.20344461410875#
#endif
#ifndef TEST_FREQUENCY2
#define TEST_FREQUENCY2 #55#
#endif
#ifndef TEST_FREQUENCY1
#define TEST_FREQUENCY1 #41.20344461410875#
#endif
#ifndef TEST_FREQUENCY3
#define TEST_FREQUENCY3 #73.41619197935188#
#endif
#ifndef TEST_FREQUENCY4
#define TEST_FREQUENCY4 #97.99885899543733#
#endif
#ifndef TEST_POSITION
#define TEST_POSITION #0.12#
#endif
#ifndef TEST_HARMONIC
#define TEST_HARMONIC #0#
#endif
#ifndef TEST_SPLIT
#define TEST_SPLIT #0#
#endif
#ifndef TEST_SCORE_END
#define TEST_SCORE_END #0.3#
#endif

sr = $TEST_SR
ksmps = $TEST_KSMPS
nchnls = 2
0dbfs = 1
A4 = $TEST_A4

giDoubleBass hlolli_wg_double_bass_create $TEST_A4

instr Voice
  iStart = p6
  iEnd = p7
  iRamp = min(0.12, 0.5 * p3)
  if iStart == iEnd then
    kFrequency init iStart
  else
    kFrequency linseg iStart, iRamp, iEnd, p3 - iRamp, iEnd
  endif
  kTrigger init p8
  kHarmonic init p10
  aLeft, aRight hlolli_wg_double_bass \
      kTrigger, kFrequency, 0.42, -0.31, p9, \
      0, 0, 0, kHarmonic, 0, p5, p4
  outs 0.2 * aLeft, 0.2 * aRight
endin

instr Renderer
  aLeft, aRight hlolli_wg_double_bass_resonance \
      p4, 0.6, p5, 0
  outs 0.2 * aLeft, 0.2 * aRight
endin

instr KeepAlive
  aZero init 0
  outs aZero, aZero
endin

instr Probe
  iPhase, iVibratoPhase, iFrequency, iTrigger, iForce, iSpeed, \
      iActivity, iLastSample, iGapSamples, iOwner, iSuccessor \
      hlolli_wg_double_bass_test_string p4, p5
  printf_i \
      "WG_STRING %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n", \
      1, p6, p5, iPhase, iFrequency, iActivity, iLastSample, iGapSamples, \
      iOwner, iSuccessor

  iF01, iF02, iF03, iF04, iF05, iF06, iF07, iF08, iF09, \
      iF10, iF11, iF12, iF13, iF14, iF15, iF16, iF17, iF18 \
      hlolli_wg_double_bass_test_finger p4, p5
  printf_i \
      "WG_FINGER %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n", \
      1, p6, p5, iF01, iF02, iF03, iF08, iF18

  iW01, iW02, iW03, iW04, iW05, iW06, iW07, iW08, iW09, \
      iW10, iW11, iW12, iW13, iW14, iW15, iW16, iW17, iW18, \
      iW19, iW20, iW21, iW22, iW23, iW24, iW25 \
      hlolli_wg_double_bass_test_waveguide p4, p5
  printf_i \
      "WG_WAVE %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n", \
      1, p6, p5, iW01, iW02, iW03, iW04, iW05, iW19, iW25

  iB01, iB02, iB03, iB04, iB05, iB06, iB07, iB08, iB09, \
      iB10, iB11, iB12, iB13, iB14, iB15, iB16, iB17, iB18, \
      iB19, iB20, iB21, iB22, iB23, iB24, iB25, iB26, iB27, \
      iB28, iB29, iB30, iB31, iB32, iB33, iB34, iB35, iB36 \
      hlolli_wg_double_bass_test_bow p4, p5
  printf_i \
      "WG_BOW_GEOMETRY %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n", \
      1, p6, p5, iB06, iB07, iB08, iB09, iB36

  iH01, iH02, iH03, iH04, iH05, iH06, iH07, iH08, iH09, \
      iH10, iH11, iH12, iH13, iH14, iH15, iH16, iH17, iH18, iH19 \
      hlolli_wg_double_bass_test_harmonic p4, p5
  printf_i \
      "WG_HARMONIC %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n", \
      1, p6, p5, iH01, iH02, iH04, iH05, iH06, iH07, iH08, iH09, iH19

  iS01, iS02, iS03, iS04, iS05, iS06, iS07, iS08, iS09, \
      iS10, iS11, iS12, iS13, iS14, iS15, iS16, iS17, iS18, \
      iS19, iS20, iS21, iS22 \
      hlolli_wg_double_bass_test_strange p4, p5
  printf_i \
      "WG_PASSIVE %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n", \
      1, p6, p5, iS17, iS18, iS19, iS20, iS21, iS22
endin

instr ScheduleTest
  event_i "i", "KeepAlive", 0, $TEST_SCORE_END
  if $TEST_CASE == 1 then
    event_i "i", "Voice", 0, 0.08, giDoubleBass, $TEST_STRING, \
        $TEST_FREQUENCY, $TEST_FREQUENCY, 0.75, $TEST_POSITION, $TEST_HARMONIC
    event_i "i", "Renderer", 0, 0.08, giDoubleBass, 0.7
    event_i "i", "Probe", 0.04, 0.001, \
        giDoubleBass, $TEST_PROBE_STRING, 1
  elseif $TEST_CASE == 2 then
    event_i "i", "Voice", 0, 0.08, giDoubleBass, 1, \
        $TEST_FREQUENCY1, $TEST_FREQUENCY1, 0.75, $TEST_POSITION, 0
    event_i "i", "Voice", 0, 0.08, giDoubleBass, 2, \
        $TEST_FREQUENCY2, $TEST_FREQUENCY2, 0.75, $TEST_POSITION, 0
    event_i "i", "Voice", 0, 0.08, giDoubleBass, 3, \
        $TEST_FREQUENCY3, $TEST_FREQUENCY3, 0.75, $TEST_POSITION, 0
    event_i "i", "Voice", 0, 0.08, giDoubleBass, 4, \
        $TEST_FREQUENCY4, $TEST_FREQUENCY4, 0.75, $TEST_POSITION, 0
    event_i "i", "Renderer", 0, 0.08, giDoubleBass, 0.7
    event_i "i", "Probe", 0.04, 0.001, giDoubleBass, 1, 1
    event_i "i", "Probe", 0.04, 0.001, giDoubleBass, 2, 2
    event_i "i", "Probe", 0.04, 0.001, giDoubleBass, 3, 3
    event_i "i", "Probe", 0.04, 0.001, giDoubleBass, 4, 4
  elseif $TEST_CASE == 3 then
    event_i "i", "Voice", 0, 0.08, giDoubleBass, 0, \
        $TEST_FREQUENCY, $TEST_FREQUENCY, 0.75, $TEST_POSITION, $TEST_HARMONIC
    event_i "i", "Renderer", 0, 0.08, giDoubleBass, 0.7
    event_i "i", "Probe", 0.04, 0.001, giDoubleBass, 1, 1
    event_i "i", "Probe", 0.04, 0.001, giDoubleBass, 2, 2
    event_i "i", "Probe", 0.04, 0.001, giDoubleBass, 3, 3
    event_i "i", "Probe", 0.04, 0.001, giDoubleBass, 4, 4
  elseif $TEST_CASE == 4 then
    event_i "i", "Voice", 0, 0.16, giDoubleBass, $TEST_DRIVE_STRING, \
        $TEST_FREQUENCY, $TEST_FREQUENCY, 0.9, $TEST_POSITION, 0
    event_i "i", "Renderer", 0, 0.16, giDoubleBass, 1
    event_i "i", "Probe", 0.12, 0.001, \
        giDoubleBass, $TEST_PROBE_STRING, 1
  elseif $TEST_CASE == 5 then
    if $TEST_SPLIT == 0 then
      event_i "i", "Voice", 0, 0.25, giDoubleBass, $TEST_STRING, \
          $TEST_FREQUENCY, $TEST_FREQUENCY, 0.75, $TEST_POSITION, 0
    else
      event_i "i", "Voice", 0, 0.125, giDoubleBass, $TEST_STRING, \
          $TEST_FREQUENCY, $TEST_FREQUENCY, 0.75, $TEST_POSITION, 0
      event_i "i", "Voice", 0.125, 0.125, giDoubleBass, $TEST_STRING, \
          $TEST_FREQUENCY, $TEST_FREQUENCY, 0.75, $TEST_POSITION, 0
    endif
    event_i "i", "Renderer", 0, 0.25, giDoubleBass, 0.7
    event_i "i", "Probe", 0.22, 0.001, \
        giDoubleBass, $TEST_PROBE_STRING, 1
  elseif $TEST_CASE == 6 then
    event_i "i", "Voice", 0, 0.24, giDoubleBass, $TEST_STRING, \
        $TEST_FREQUENCY, $TEST_FREQUENCY2, 0.75, $TEST_POSITION, 0
    event_i "i", "Renderer", 0, 0.24, giDoubleBass, 0.7
    event_i "i", "Probe", 0.20, 0.001, \
        giDoubleBass, $TEST_STRING, 1
  elseif $TEST_CASE == 7 then
    event_i "i", "Voice", 0, 0.08, giDoubleBass, 1, \
        $TEST_FREQUENCY1, $TEST_FREQUENCY1, 0.75, $TEST_POSITION, 0
    event_i "i", "Voice", 0, 0.08, giDoubleBass, 2, \
        $TEST_FREQUENCY2, $TEST_FREQUENCY2, 0.75, $TEST_POSITION, 0
    event_i "i", "Voice", 0, 0.08, giDoubleBass, 3, \
        $TEST_FREQUENCY3, $TEST_FREQUENCY3, 0.75, $TEST_POSITION, 0
    event_i "i", "Voice", 0, 0.08, giDoubleBass, 4, \
        $TEST_FREQUENCY4, $TEST_FREQUENCY4, 0.75, $TEST_POSITION, 0
    event_i "i", "Renderer", 0, 0.08, giDoubleBass, 0.7
    event_i "i", "Voice", 6.2, 0.12, giDoubleBass, 1, \
        $TEST_FREQUENCY1, $TEST_FREQUENCY1, 0.75, $TEST_POSITION, 0
    event_i "i", "Voice", 6.2, 0.12, giDoubleBass, 2, \
        $TEST_FREQUENCY2, $TEST_FREQUENCY2, 0.75, $TEST_POSITION, 0
    event_i "i", "Voice", 6.2, 0.12, giDoubleBass, 3, \
        $TEST_FREQUENCY3, $TEST_FREQUENCY3, 0.75, $TEST_POSITION, 0
    event_i "i", "Voice", 6.2, 0.12, giDoubleBass, 4, \
        $TEST_FREQUENCY4, $TEST_FREQUENCY4, 0.75, $TEST_POSITION, 0
    event_i "i", "Renderer", 6.2, 0.12, giDoubleBass, 0.7
    event_i "i", "Probe", 6.28, 0.001, giDoubleBass, 1, 1
    event_i "i", "Probe", 6.28, 0.001, giDoubleBass, 2, 2
    event_i "i", "Probe", 6.28, 0.001, giDoubleBass, 3, 3
    event_i "i", "Probe", 6.28, 0.001, giDoubleBass, 4, 4
  elseif $TEST_CASE == 8 then
    event_i "i", "Voice", 0, 0.125, giDoubleBass, 0, \
        $TEST_FREQUENCY, $TEST_FREQUENCY, 0.75, $TEST_POSITION, 0
    event_i "i", "Voice", 0.125, 0.125, giDoubleBass, 0, \
        $TEST_FREQUENCY2, $TEST_FREQUENCY2, 0.75, $TEST_POSITION, 0
    event_i "i", "Renderer", 0, 0.25, giDoubleBass, 0.7
    event_i "i", "Probe", 0.22, 0.001, giDoubleBass, 1, 1
    event_i "i", "Probe", 0.22, 0.001, giDoubleBass, 2, 2
    event_i "i", "Probe", 0.22, 0.001, giDoubleBass, 3, 3
    event_i "i", "Probe", 0.22, 0.001, giDoubleBass, 4, 4
  elseif $TEST_CASE == 9 then
    event_i "i", "Voice", 0, 0.125, giDoubleBass, 0, \
        $TEST_FREQUENCY, $TEST_FREQUENCY, 0.75, $TEST_POSITION, $TEST_HARMONIC
    event_i "i", "Voice", 0.125, 0.125, giDoubleBass, 0, \
        $TEST_FREQUENCY2, $TEST_FREQUENCY2, 0.75, $TEST_POSITION, $TEST_HARMONIC
    event_i "i", "Renderer", 0, 0.25, giDoubleBass, 0.7
    event_i "i", "Probe", 0.22, 0.001, giDoubleBass, 1, 1
    event_i "i", "Probe", 0.22, 0.001, giDoubleBass, 2, 2
    event_i "i", "Probe", 0.22, 0.001, giDoubleBass, 3, 3
    event_i "i", "Probe", 0.22, 0.001, giDoubleBass, 4, 4
  elseif $TEST_CASE == 10 then
    event_i "i", "Voice", 0, 0.125, giDoubleBass, 0, \
        $TEST_FREQUENCY1, $TEST_FREQUENCY1, 0.75, $TEST_POSITION, 0
    event_i "i", "Voice", 0, 0.0625, giDoubleBass, 0, \
        $TEST_FREQUENCY2, $TEST_FREQUENCY2, 0.75, $TEST_POSITION, 0
    event_i "i", "Voice", 0.0625, 0.0625, giDoubleBass, 0, \
        $TEST_FREQUENCY3, $TEST_FREQUENCY3, 0.75, $TEST_POSITION, 0
    event_i "i", "Voice", 0.125, 0.125, giDoubleBass, 0, \
        $TEST_FREQUENCY4, $TEST_FREQUENCY4, 0.75, $TEST_POSITION, 0
    event_i "i", "Renderer", 0, 0.25, giDoubleBass, 0.7
    event_i "i", "Probe", 0.22, 0.001, giDoubleBass, 1, 1
    event_i "i", "Probe", 0.22, 0.001, giDoubleBass, 2, 2
    event_i "i", "Probe", 0.22, 0.001, giDoubleBass, 3, 3
    event_i "i", "Probe", 0.22, 0.001, giDoubleBass, 4, 4
  endif
endin

</CsInstruments>
<CsScore>
i "ScheduleTest" 0 0.001
e $TEST_SCORE_END
</CsScore>
</CsoundSynthesizer>
