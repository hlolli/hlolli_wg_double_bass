<CsoundSynthesizer>
<CsOptions>
-n -d -m128
</CsOptions>
<CsInstruments>

; API smoke. Pitch, delay, and range are double-bass checked.
; Fixed acoustic data and sound remain violin-derived and unfitted.

sr = 48000
ksmps = 32
nchnls = 2
0dbfs = 1

giDoubleBassA hlolli_wg_double_bass_create
giDoubleBassB hlolli_wg_double_bass_create 442

instr Voice
  iDoubleBass = p4
  iString = p5
  kTrigger init 0.6
  kFrequency init cpsmidinn(p6)
  kForce init 0.45
  kSpeed init 0.55
  kPosition init 0.12
  kVibratoDepth init 16
  kVibratoRate init 5.1
  kArticulation init p7
  kHarmonic init p8
  kStrange init p9

  aLeft, aRight hlolli_wg_double_bass \
      kTrigger, kFrequency, kForce, kSpeed, kPosition, \
      kVibratoDepth, kVibratoRate, kArticulation, \
      kHarmonic, kStrange, iString, iDoubleBass
  outs aLeft, aRight
endin

instr BodyA
  aLeft, aRight hlolli_wg_double_bass_resonance \
      giDoubleBassA, 0.72, 0.55, 0
  outs aLeft, aRight
endin

instr BodyB
  aLeft, aRight hlolli_wg_double_bass_resonance \
      giDoubleBassB, 0.64, 0.35, 0.4
  outs aLeft, aRight
endin

instr StartVoices
  ; Pass the handles returned by the creator opcodes. Do not assume their IDs.
  event_i "i", "Voice", 0, 0.25, giDoubleBassA, 3, 38, 0, 0, 0
  event_i "i", "Voice", 0, 0.25, giDoubleBassA, 2, 33, 1, 0, -0.2
  event_i "i", "Voice", 0, 0.25, giDoubleBassB, 3, 43, 5, 0, 0.2
endin

</CsInstruments>
<CsScore>
i "BodyA" 0 0.5
i "BodyB" 0 0.5
i "StartVoices" 0 0.01
e 0.5
</CsScore>
</CsoundSynthesizer>
