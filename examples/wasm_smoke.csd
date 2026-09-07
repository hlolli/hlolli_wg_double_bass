<CsoundSynthesizer>
<CsOptions>
-n -d -m0
</CsOptions>
<CsInstruments>

; Browser smoke. Pitch, delay, and range are double-bass checked.
; Fixed acoustic data and sound remain violin-derived and unfitted.

sr = 48000
ksmps = 32
nchnls = 2
0dbfs = 1

giDoubleBass hlolli_wg_double_bass_create
gaLeft init 0
gaRight init 0

instr Voice
  kFrequency linseg 73.41619197935188, p3, 82.4068892282175
  kForce linseg 0.32, p3 * 0.3, 0.58, p3 * 0.7, 0.44
  kSpeed linseg 0.42, p3 * 0.55, -0.38, p3 * 0.45, -0.31
  aLeft, aRight hlolli_wg_double_bass \
      1, kFrequency, kForce, kSpeed, 0.16, \
      10, 5.1, 0, 0, 0, 3, giDoubleBass
  gaLeft += aLeft
  gaRight += aRight
endin

instr Body
  aLeft, aRight hlolli_wg_double_bass_resonance \
      giDoubleBass, 0.72, 0.45, 0
  gaLeft += aLeft
  gaRight += aRight
endin

instr Output
  outs gaLeft, gaRight
  clear gaLeft, gaRight
endin

</CsInstruments>
<CsScore>
i "Voice" 0 0.75
i "Body" 0 1.0
i "Output" 0 1.0
e
</CsScore>
</CsoundSynthesizer>
