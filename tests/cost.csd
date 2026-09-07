<CsoundSynthesizer>
<CsOptions>
-d -m128
</CsOptions>
<CsInstruments>

#ifndef TEST_HANDLES
#define TEST_HANDLES #32#
#endif
#ifndef TEST_SECONDS
#define TEST_SECONDS #2#
#endif

sr = 48000
ksmps = 32
nchnls = 2
0dbfs = 1

; Independent handles are intentional: this score measures the release build's
; many-instance and two-worker path rather than same-handle voice serialization.
giOpenFrequencies[] fillarray \
    41.20344461410875, 55, 73.41619197935188, 97.99885899543733
giBasses[] init $TEST_HANDLES
giCreateIndex = 0
while giCreateIndex < $TEST_HANDLES do
  giBasses[giCreateIndex] hlolli_wg_double_bass_create
  giCreateIndex += 1
od

instr Voice
  iIndex = p4
  iString = 1 + iIndex - 4 * int(iIndex / 4)
  iFrequency = giOpenFrequencies[iString - 1] * \
      (1 + 0.0125 * (iIndex - 5 * int(iIndex / 5)))
  iHandle = giBasses[iIndex]
  kPhase phasor 0.61 + 0.013 * iIndex, 0.019 * iIndex
  kShape = 1 - abs(2 * kPhase - 1)
  kFrequency = iFrequency * (1 + 0.01 * kShape)
  kForce = 0.35 + 0.20 * kShape
  kSpeed = (0.24 + 0.18 * (1 - kShape)) * (kPhase < 0.5 ? 1 : -1)
  aLeft, aRight hlolli_wg_double_bass \
      1, kFrequency, kForce, kSpeed, 0.10 + 0.01 * iString, \
      7, 4.8, 0, 0, 0.12, iString, iHandle
  outs 0.008 * aLeft, 0.008 * aRight
endin

instr Renderer
  iIndex = p4
  iHandle = giBasses[iIndex]
  aLeft, aRight hlolli_wg_double_bass_resonance \
      iHandle, 0.68, 0.35, 0.08
  outs 0.016 * aLeft, 0.016 * aRight
endin

instr Schedule
  printf_i "WG_COST_HANDLES %.17g\n", 1, $TEST_HANDLES
  iIndex = 0
  while iIndex < $TEST_HANDLES do
    event_i "i", "Voice", 0, $TEST_SECONDS, iIndex
    event_i "i", "Renderer", 0, $TEST_SECONDS, iIndex
    iIndex += 1
  od
endin

</CsInstruments>
<CsScore>
i "Schedule" 0 0.001
e
</CsScore>
</CsoundSynthesizer>
