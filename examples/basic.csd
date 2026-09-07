<CsoundSynthesizer>
<CsOptions>
-d -m128
</CsOptions>
<CsInstruments>
; Four open strings, one bass handle, one shared body.
; Sound remains violin-derived development data, not a validated bass fit.
sr = 48000
ksmps = 32
nchnls = 2
0dbfs = 1

giBass hlolli_wg_double_bass_create

instr Note
  ; p4: sounding MIDI pitch; p5: physical string (E/A/D/G = 1/2/3/4).
  ; Keep processing for the final second with bow contact removed so the
  ; passive string tail reaches both the direct output and shared body.
  kTime timeinsts
  kGate = kTime < p3 - 1 ? 1 : 0
  aLeft, aRight hlolli_wg_double_bass \
      kGate, cpsmidinn(p4), 0.50, 0.45, 0.12, \
      0, 0, 0, 0, 0, p5, giBass
  outs 0.03 * aLeft, 0.03 * aRight
endin

instr Body
  aLeft, aRight hlolli_wg_double_bass_resonance giBass, 0.55, 0.6, 0
  outs 0.45 * aLeft, 0.45 * aRight
endin
</CsInstruments>
<CsScore>
; Sounding E1, A1, D2, G2 (no written-octave transposition).
i "Body" 0 12.5
i "Note" 0.2  2.5 28 1
i "Note" 3.0  2.5 33 2
i "Note" 5.8  2.5 38 3
i "Note" 8.6  2.5 43 4
e
</CsScore>
</CsoundSynthesizer>
