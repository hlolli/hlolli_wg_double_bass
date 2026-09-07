<CsoundSynthesizer>
<CsOptions>
-n -d -m128
</CsOptions>
<CsInstruments>

sr = 48000
ksmps = 32
nchnls = 2
0dbfs = 1

; Check both public constructor forms, exact open-string pitches, and A4
; scaling. The geometry suite checks the rest of the playing range.
giDefault hlolli_wg_double_bass_create
giA440 hlolli_wg_double_bass_create 440
giA442 hlolli_wg_double_bass_create 442

opcode StringFrequency, i, ii
  iHandle, iString xin
  iOpenFrequency, iTargetPosition, iPosition, iTargetPressure, iPressure, \
      iVelocity, iDelayVelocity, iEffectiveFrequency, iLastNoise, \
      iNoiseEnergy, iNoisePeak, iNoiseState, iArrivals, iReleases, iShifts, \
      iMovementSamples, iNoiseSamples, iFinite \
      hlolli_wg_double_bass_test_finger iHandle, iString
  xout iOpenFrequency
endop

instr Probe
  iDefault1 StringFrequency giDefault, 1
  iDefault2 StringFrequency giDefault, 2
  iDefault3 StringFrequency giDefault, 3
  iDefault4 StringFrequency giDefault, 4
  iA440_1 StringFrequency giA440, 1
  iA440_2 StringFrequency giA440, 2
  iA440_3 StringFrequency giA440, 3
  iA440_4 StringFrequency giA440, 4
  iA442_1 StringFrequency giA442, 1
  iA442_2 StringFrequency giA442, 2
  iA442_3 StringFrequency giA442, 3
  iA442_4 StringFrequency giA442, 4
  printf_i \
      "WG_CONSTRUCTORS %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n", \
      1, giDefault, giA440, giA442, \
      iDefault1, iDefault2, iDefault3, iDefault4, \
      iA440_1, iA440_2, iA440_3, iA440_4, \
      iA442_1, iA442_2, iA442_3, iA442_4
endin

</CsInstruments>
<CsScore>
i "Probe" 0 0.001
e 0.002
</CsScore>
</CsoundSynthesizer>
