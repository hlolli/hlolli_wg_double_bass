<CsoundSynthesizer>
<CsOptions>
-n -d -m128
</CsOptions>
<CsInstruments>

#ifndef TEST_TARGET
#define TEST_TARGET #1#
#endif
#ifndef TEST_SPLIT
#define TEST_SPLIT #0#
#endif
#ifndef TEST_SPLIT_TIME
#define TEST_SPLIT_TIME #0.016#
#endif
#ifndef TEST_OVERLAP
#define TEST_OVERLAP #0#
#endif
#ifndef TEST_RESTART
#define TEST_RESTART #0#
#endif
#ifndef TEST_SAME_BLOCK_GAP
#define TEST_SAME_BLOCK_GAP #0#
#endif
#ifndef TEST_QUEUE_SPANS
#define TEST_QUEUE_SPANS #0#
#endif
#ifndef TEST_SAMPLE_SPAN
#define TEST_SAMPLE_SPAN #0#
#endif
#ifndef TEST_SCORE_END
#define TEST_SCORE_END #0.06#
#endif

sr = 48000
ksmps = 32
nchnls = 2
0dbfs = 1

giDoubleBass hlolli_wg_double_bass_create

instr Voice
  kFrequency init p6
  kTrigger init p7
  kForce init p8
  kSpeed init p9

  aLeft, aRight hlolli_wg_double_bass \
      kTrigger, kFrequency, kForce, kSpeed, 0.14, \
      17, 5.5, 0, 0, 0, p5, p4
  outs aLeft, aRight
endin

instr Renderer
  aLeft, aRight hlolli_wg_double_bass_resonance \
      p4, 0.37, 0.63, 0.21
  outs aLeft, aRight
endin

instr ProbeString
  iPhase, iVibratoPhase, iFrequency, iTrigger, iForce, iSpeed, \
      iActivity, iLastSample, iGapSamples, iOwner, iSuccessor \
      hlolli_wg_double_bass_test_string p4, 1
  printf_i \
      "WG_STRING 1 %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n", \
      1, iPhase, iVibratoPhase, iFrequency, iTrigger, iForce, \
      iSpeed, iActivity, iLastSample, iGapSamples, iOwner, iSuccessor
endin

instr ProbeRenderer
  iBody, iSympathetic, iMute, iActivity, iLastSample, iGapSamples, \
      iOwner, iSuccessor \
      hlolli_wg_double_bass_test_renderer p4
  printf_i \
      "WG_RENDERER 1 %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n", \
      1, iBody, iSympathetic, iMute, iActivity, iLastSample, \
      iGapSamples, iOwner, iSuccessor
endin

instr KeepAlive
  aZero init 0
  outs aZero, aZero
endin

instr ScheduleTest
  event_i "i", "KeepAlive", 0, $TEST_SCORE_END
  iTotal = 0.048
  iFirstDuration = $TEST_SPLIT_TIME
  iOffset2 = 0.0160416666666667
  iOffset6 = 0.016125
  iOffset10 = 0.016208333334
  iOffset14 = 0.016291666667
  iFourSamples = 0.000083333334
  iGapSecondDuration = 0.03
  if $TEST_OVERLAP != 0 then
    iFirstDuration += 0.000125
  endif

  if $TEST_SAMPLE_SPAN == 1 then
    ; The first span ends at sample 864. Binary 0.018 starts the second at
    ; sample 863, so these spans overlap by one sample even though p2+p3
    ; compares as exact and the first no_end value is zero.
    iFirstStart = 0.007604166666666667
    iFirstSpan = 0.010395833333333333
    iSecondStart = 0.018
    if $TEST_TARGET == 1 then
      event_i "i", "Voice", iFirstStart, iFirstSpan, \
          giDoubleBass, 1, 333, 0.8, 0.42, -0.35
      event_i "i", "Voice", iSecondStart, 0.01, \
          giDoubleBass, 1, 333, 0.8, 0.42, -0.35
    else
      event_i "i", "Renderer", iFirstStart, iFirstSpan, giDoubleBass
      event_i "i", "Renderer", iSecondStart, 0.01, giDoubleBass
    endif
  elseif $TEST_SAMPLE_SPAN == 2 then
    ; Both float and double engines truncate the first duration to 60
    ; samples. Its successor starts at that exact sample, while p2+p3 is
    ; almost one sample late and must not decide admission.
    iLongStart = 128
    iLongSpan = 0.0012708333333333332
    iLongNext = 128.00125
    if $TEST_TARGET == 1 then
      event_i "i", "Voice", iLongStart, iLongSpan, \
          giDoubleBass, 1, 333, 0.8, 0.42, -0.35
      event_i "i", "Voice", iLongNext, 0.002, \
          giDoubleBass, 1, 333, 0.8, 0.42, -0.35
    else
      event_i "i", "Renderer", iLongStart, iLongSpan, giDoubleBass
      event_i "i", "Renderer", iLongNext, 0.002, giDoubleBass
    endif
  elseif $TEST_SAME_BLOCK_GAP != 0 then
    if $TEST_TARGET == 1 then
      event_i "i", "Voice", 0, iOffset6, \
          giDoubleBass, 1, 333, 0.8, 0.42, -0.35
      event_i "i", "Voice", iOffset10, iGapSecondDuration, \
          giDoubleBass, 1, 333, 0, 0.42, -0.35
      event_i "i", "ProbeString", 0.05, 0.001, giDoubleBass
    else
      event_i "i", "Voice", 0, 0.0153333333333333, \
          giDoubleBass, 1, 333, 0.6, 0.42, -0.35
      event_i "i", "Renderer", 0, iOffset6, giDoubleBass
      event_i "i", "Renderer", iOffset10, iGapSecondDuration, giDoubleBass
      event_i "i", "ProbeRenderer", 0.05, 0.001, giDoubleBass
    endif
  elseif $TEST_QUEUE_SPANS != 0 then
    if $TEST_TARGET == 1 then
      if $TEST_QUEUE_SPANS == 1 then
        event_i "i", "Voice", iOffset2, iOffset14 - iOffset2, \
            giDoubleBass, 1, 333, 0.8, 0.42, -0.35
      else
        event_i "i", "Voice", iOffset2, iFourSamples, \
            giDoubleBass, 1, 333, 0.8, 0.42, -0.35
        event_i "i", "Voice", iOffset6, iFourSamples, \
            giDoubleBass, 1, 333, 0.8, 0.42, -0.35
        event_i "i", "Voice", iOffset10, iFourSamples, \
            giDoubleBass, 1, 333, 0.8, 0.42, -0.35
      endif
      event_i "i", "ProbeString", 0.018, 0.001, giDoubleBass
    else
      event_i "i", "Voice", iOffset2, iOffset14 - iOffset2, \
          giDoubleBass, 1, 333, 0.6, 0.42, -0.35
      if $TEST_QUEUE_SPANS == 1 then
        event_i "i", "Renderer", iOffset2, iOffset14 - iOffset2, giDoubleBass
      else
        event_i "i", "Renderer", iOffset2, iFourSamples, giDoubleBass
        event_i "i", "Renderer", iOffset6, iFourSamples, giDoubleBass
        event_i "i", "Renderer", iOffset10, iFourSamples, giDoubleBass
      endif
      event_i "i", "ProbeRenderer", 0.018, 0.001, giDoubleBass
    endif
  elseif $TEST_RESTART != 0 then
    if $TEST_TARGET == 1 then
      event_i "i", "Voice", 0, 0.016, giDoubleBass, 1, 333, 0.8, 0.42, -0.35
      event_i "i", "Voice", 0.032, 0.016, giDoubleBass, 1, 333, 0, 0.42, -0.35
      event_i "i", "ProbeString", 0.05, 0.001, giDoubleBass
    else
      event_i "i", "Voice", 0, 0.016, giDoubleBass, 1, 333, 0.6, 0.42, -0.35
      event_i "i", "Renderer", 0, 0.016, giDoubleBass
      event_i "i", "Renderer", 0.032, 0.016, giDoubleBass
      event_i "i", "ProbeRenderer", 0.05, 0.001, giDoubleBass
    endif
  elseif $TEST_TARGET == 1 then
    if $TEST_SPLIT == 0 then
      event_i "i", "Voice", 0, iTotal, giDoubleBass, 1, 333, 0.8, 0.42, -0.35
    else
      event_i "i", "Voice", 0, iFirstDuration, \
          giDoubleBass, 1, 333, 0.8, 0.42, -0.35
      event_i "i", "Voice", $TEST_SPLIT_TIME, \
          iTotal - $TEST_SPLIT_TIME, \
          giDoubleBass, 1, 333, 0.8, 0.42, -0.35
    endif
    event_i "i", "ProbeString", 0.05, 0.001, giDoubleBass
  else
    event_i "i", "Voice", 0, iTotal, giDoubleBass, 1, 333, 0.6, 0.42, -0.35
    if $TEST_SPLIT == 0 then
      event_i "i", "Renderer", 0, iTotal, giDoubleBass
    else
      event_i "i", "Renderer", 0, iFirstDuration, giDoubleBass
      event_i "i", "Renderer", $TEST_SPLIT_TIME, \
          iTotal - $TEST_SPLIT_TIME, giDoubleBass
    endif
    event_i "i", "ProbeRenderer", 0.05, 0.001, giDoubleBass
  endif
endin

</CsInstruments>
<CsScore>
i "ScheduleTest" 0 0.001
e
</CsScore>
</CsoundSynthesizer>
