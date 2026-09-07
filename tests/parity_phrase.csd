<CsoundSynthesizer>
<CsOptions>
--sample-accurate -d -m128
</CsOptions>
<CsInstruments>
; Native/browser regression: first three bars of Bach BWV 1007 Menuet I.
; Public-domain Mutopia edition 517, Andreas Scherer, Schirmer 1916.
; Cello sounding pitches lowered one octave, except C1/D1 raised to C2/D2.
; Notated slurs share a string and bow direction; bar 4 trill is unornamented.
; Sound remains violin-derived development data, not a validated bass fit.
sr = 48000
ksmps = 32
nchnls = 2
0dbfs = 1

giBass hlolli_wg_double_bass_create

instr Note
  ; p4 sounding MIDI; p5 string; p6 bow direction; p7 force;
  ; p8 gate fraction: 1 keeps contact into the next note of a slur.
  kTime timeinsts
  kGate = kTime < p3 * p8 ? 1 : 0
  aLeft, aRight hlolli_wg_double_bass \
      kGate, cpsmidinn(p4), p7, 0.45 * p6, 0.12, \
      0, 0, 0, 0, 0, p5, giBass
  outs 0.03 * aLeft, 0.03 * aRight
endin

instr Body
  aLeft, aRight hlolli_wg_double_bass_resonance giBass, 0.55, 0.6, 0
  outs 0.45 * aLeft, 0.45 * aRight
endin
</CsInstruments>
<CsScore>
; Quarter = 96. One beat lead-in plus three bars: 6.25 seconds total.
; Includes slurs, gate-off contact decay, stopped tails, and bow reversals.
; The short continuous-note parity score did not expose libm feedback drift.
; Times and durations below are quarter-note beats, not seconds.
t 0 96
i "Body" 0 10
;           beat    length MIDI string bow force gate
; A
; Source bar 1
i "Note"       1     0.5  31   1   1 0.52 1
i "Note"     1.5     0.5  38   1   1 0.48 0.94
i "Note"       2       1  47   4  -1 0.48 0.94
i "Note"       3     0.5  45   4   1 0.48 1
i "Note"     3.5    0.25  47   4   1 0.48 1
i "Note"    3.75    0.25  48   4   1 0.48 0.94
; Source bar 2
i "Note"       4     0.5  47   4  -1 0.52 1
i "Note"     4.5     0.5  45   4  -1 0.48 0.94
i "Note"       5     0.5  43   3   1 0.48 1
i "Note"     5.5     0.5  42   3   1 0.48 0.94
i "Note"       6     0.5  43   3  -1 0.48 1
i "Note"     6.5     0.5  38   3  -1 0.48 0.94
; Source bar 3
i "Note"       7     0.5  40   3   1 0.52 1
i "Note"     7.5     0.5  43   3   1 0.48 0.94
i "Note"       8     0.5  48   4  -1 0.48 1
i "Note"     8.5     0.5  45   4  -1 0.48 0.94
i "Note"       9     0.5  42   3   1 0.48 1
i "Note"     9.5     0.5  50   3   1 0.48 0.94
e
</CsScore>
</CsoundSynthesizer>
