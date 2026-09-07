<CsoundSynthesizer>
<CsOptions>
-d -m128
</CsOptions>
<CsInstruments>
; Three players: independent handles, each with exactly one body renderer.
; All use numeric A4 = 442 Hz. Score pitches must use the same tuning.
; This original short study adds no room effect or runtime model profile.
; Sound remains violin-derived development data.
sr = 48000
ksmps = 32
nchnls = 2
0dbfs = 1

giA4 = 442
giBass1 hlolli_wg_double_bass_create giA4
giBass2 hlolli_wg_double_bass_create giA4
giBass3 hlolli_wg_double_bass_create giA4

instr Voice
  ; p4: handle; p5: sounding MIDI; p6: physical string; p7: bow direction.
  kTime timeinsts
  kGate = kTime < p3 - 0.8 ? 1 : 0
  iFrequency = cpsmidinn(p5) * giA4 / 440
  aLeft, aRight hlolli_wg_double_bass \
      kGate, iFrequency, 0.45, 0.45 * p7, 0.12, \
      0, 0, 0, 0, 0, p6, p4
  ; Divide the demo output gain by three, not the excitation force.
  outs 0.01 * aLeft, 0.01 * aRight
endin

instr Body
  aLeft, aRight hlolli_wg_double_bass_resonance p4, 0.55, 0.6, 0
  outs 0.15 * aLeft, 0.15 * aRight
endin

instr Schedule
  event_i "i", "Body", 0, 10, giBass1
  event_i "i", "Body", 0, 10, giBass2
  event_i "i", "Body", 0, 10, giBass3
  ; Small entrance offsets separate the three players.
  event_i "i", "Voice", 0.20, 2.6, giBass1, 28, 1, 1
  event_i "i", "Voice", 0.24, 2.6, giBass2, 35, 2, -1
  event_i "i", "Voice", 0.28, 2.6, giBass3, 43, 4, 1
  event_i "i", "Voice", 3.20, 2.6, giBass1, 33, 2, -1
  event_i "i", "Voice", 3.24, 2.6, giBass2, 40, 3, 1
  event_i "i", "Voice", 3.28, 2.6, giBass3, 45, 4, -1
  event_i "i", "Voice", 6.20, 2.6, giBass1, 28, 1, 1
  event_i "i", "Voice", 6.24, 2.6, giBass2, 35, 2, -1
  event_i "i", "Voice", 6.28, 2.6, giBass3, 43, 4, 1
endin
</CsInstruments>
<CsScore>
i "Schedule" 0 0.001
e
</CsScore>
</CsoundSynthesizer>
