<CsoundSynthesizer>
<CsOptions>
-d -m128
</CsOptions>
<CsInstruments>

#ifndef TEST_CASE
#define TEST_CASE #1#
#endif
#ifndef TEST_ORDER
#define TEST_ORDER #0#
#endif
#ifndef TEST_SECONDS
#define TEST_SECONDS #0.45#
#endif

sr = 48000
ksmps = 32
nchnls = 2
0dbfs = 1

; The fixed data and sound remain violin-derived. This score checks engine
; order, independent handles, worker use, and source-preparation parity.
giBass1 hlolli_wg_double_bass_create
giBass2 hlolli_wg_double_bass_create
giBass3 hlolli_wg_double_bass_create
giBass4 hlolli_wg_double_bass_create

opcode TestVoice, aa, iii
  iHandle, iString, iSeed xin
  kPhase phasor 0.7 + 0.11 * iSeed, 0.09 * iSeed
  kShape = 1 - abs(2 * kPhase - 1)
  kFrequency = (220 + 37 * iSeed) * (0.99 + 0.02 * kShape)
  kForce = 0.25 + 0.22 * kShape
  kSpeed = (0.31 + 0.17 * (1 - kShape)) * (kPhase < 0.5 ? 1 : -1)
  aLeft, aRight hlolli_wg_double_bass \
      1, kFrequency, kForce, kSpeed, 0.11 + 0.01 * iString, \
      9, 5.2, 0.15, 0, 0, iString, iHandle
  xout 0.22 * aLeft, 0.22 * aRight
endop

opcode TestBody, aa, i
  iHandle xin
  aLeft, aRight hlolli_wg_double_bass_resonance \
      iHandle, 0.68, 0.35, 0.08
  xout 0.36 * aLeft, 0.36 * aRight
endop

; These numeric instruments put the body before or after the voice.
instr 1
  aLeft, aRight TestBody giBass1
  outs aLeft, aRight
endin

instr 2
  aLeft, aRight TestVoice giBass1, 1, 1
  outs aLeft, aRight
endin

instr 3
  aLeft, aRight TestVoice giBass1, 1, 1
  outs aLeft, aRight
endin

instr 4
  aLeft, aRight TestBody giBass1
  outs aLeft, aRight
endin

instr MultiVoice
  aLeft, aRight TestVoice p4, p5, p6
  outs aLeft, aRight
endin

instr MultiBody
  aLeft, aRight TestBody p4
  outs aLeft, aRight
endin

instr Schedule
  if $TEST_CASE == 1 then
    event_i "i", 1, 0, $TEST_SECONDS
    event_i "i", 2, 0, $TEST_SECONDS
  elseif $TEST_CASE == 2 then
    if $TEST_ORDER == 0 then
      event_i "i", 1, 0, $TEST_SECONDS
      event_i "i", 2, 0, $TEST_SECONDS
    else
      event_i "i", 3, 0, $TEST_SECONDS
      event_i "i", 4, 0, $TEST_SECONDS
    endif
  else
    printf_i "WG_MULTI_HANDLES %.17g %.17g %.17g %.17g\n", \
        1, giBass1, giBass2, giBass3, giBass4
    event_i "i", "MultiVoice", 0, $TEST_SECONDS, giBass1, 1, 1
    event_i "i", "MultiVoice", 0, $TEST_SECONDS, giBass2, 2, 2
    event_i "i", "MultiVoice", 0, $TEST_SECONDS, giBass3, 3, 3
    event_i "i", "MultiVoice", 0, $TEST_SECONDS, giBass4, 4, 4
    event_i "i", "MultiBody", 0, $TEST_SECONDS, giBass1
    event_i "i", "MultiBody", 0, $TEST_SECONDS, giBass2
    event_i "i", "MultiBody", 0, $TEST_SECONDS, giBass3
    event_i "i", "MultiBody", 0, $TEST_SECONDS, giBass4
  endif
endin

</CsInstruments>
<CsScore>
i "Schedule" 0 0.001
e
</CsScore>
</CsoundSynthesizer>
