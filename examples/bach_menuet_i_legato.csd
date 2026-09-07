<CsoundSynthesizer>
<CsOptions>
--sample-accurate -d -m128
</CsOptions>
<CsInstruments>
; J. S. Bach: Menuet I, Suite No. 1, BWV 1007. Both repeats, G major.
; Public-domain Mutopia edition 517, Andreas Scherer, Schirmer 1916.
; Cello sounding pitches lowered one octave, except C1/D1 raised to C2/D2.
; Four-bar infinite-bow phrases; bar 4 trill is unornamented.
; Load only libhlolli_wg_double_bass_legato, not the standard module.
; Sound remains violin-derived development data, not a validated bass fit.
sr = 48000
ksmps = 32
nchnls = 2
0dbfs = 1

giBass hlolli_wg_double_bass_create

instr Note
  ; p4 sounding MIDI; p5 string; p6 bow direction; p7 force;
  ; p8 gate fraction: 1 keeps contact into the next note of a slur.
  ; p9: new bow attack (0 for a same-string slur continuation).
  ; Retain the original body/sympathy balance.
  ; Share the native cpsmidinn evaluation order explicitly. Division by 12
  ; differs from multiplication by its binary64 reciprocal in this host build.
  ; This preserves every native pitch bit; it does not quantize input or PCM.
  ; A4=440 fixture only.
  iFrequency = 440 * (2 ^ ((p4 - 69) * 0.08333333333333333))
  kForce linseg p7 + 0.135 * p9, 0.03, p7
  kTime timeinsts
  kGate = kTime < p3 * p8 ? 1 : 0
  aLeft, aRight hlolli_wg_double_bass \
      kGate, iFrequency, kForce, 0.45 * p6, 0.12, \
      0, 0, 0, 0, 0, p5, giBass
  outs 0.03 * aLeft, 0.03 * aRight
endin

instr Body
  aLeft, aRight hlolli_wg_double_bass_resonance giBass, 0.55, 0.6, 0
  outs 0.45 * aLeft, 0.45 * aRight
endin
</CsInstruments>
<CsScore>
; Selected legato performance, with an explicitly loaded mechanical-off module.
; Ongoing friction and random-number advancement are retained.
; Four-bar idealized bow phrases; all 268 original audition events retained.
t 0 96
i "Body" 0 150
i "Note" 1 0.5 31 1 1 0.52 1 1
i "Note" 1.5 0.5 38 1 1 0.48 0.94 0
i "Note" 2 1 47 4 1 0.48 1 1
i "Note" 3 0.5 45 4 1 0.48 1 0
i "Note" 3.5 0.25 47 4 1 0.48 1 0
i "Note" 3.75 0.25 48 4 1 0.48 1 0
i "Note" 4 0.5 47 4 1 0.52 1 0
i "Note" 4.5 0.5 45 4 1 0.48 0.94 0
i "Note" 5 0.5 43 3 1 0.48 1 1
i "Note" 5.5 0.5 42 3 1 0.48 1 0
i "Note" 6 0.5 43 3 1 0.48 1 0
i "Note" 6.5 0.5 38 3 1 0.48 1 0
i "Note" 7 0.5 40 3 1 0.52 1 0
i "Note" 7.5 0.5 43 3 1 0.48 1 0
i "Note" 8 0.5 48 3 1 0.48 1 0
i "Note" 8.5 0.5 45 3 1 0.48 1 0
i "Note" 9 0.5 42 3 1 0.48 1 0
i "Note" 9.5 0.5 50 3 1 0.48 1 0
i "Note" 10 2 31 1 1 0.44 0.94 1
i "Note" 10 2 38 3 1 0.44 1 0
i "Note" 10 2 47 4 1 0.52 1 1
i "Note" 12 1 38 3 1 0.44 0.94 0
i "Note" 12 1 45 4 1 0.48 0.94 0
i "Note" 13 0.5 33 2 1 0.52 1 1
i "Note" 13.5 0.5 42 2 1 0.48 0.94 0
i "Note" 14 1 48 4 1 0.48 1 1
i "Note" 15 0.5 47 4 1 0.48 1 0
i "Note" 15.5 0.25 48 4 1 0.48 1 0
i "Note" 15.75 0.25 50 4 1 0.48 1 0
i "Note" 16 0.5 48 4 1 0.52 1 0
i "Note" 16.5 0.5 47 4 1 0.48 1 0
i "Note" 17 0.5 45 4 1 0.48 1 0
i "Note" 17.5 0.5 43 4 1 0.48 0.94 0
i "Note" 18 0.5 42 3 1 0.48 1 1
i "Note" 18.5 0.5 40 3 1 0.48 1 0
i "Note" 19 0.5 42 3 1 0.52 1 0
i "Note" 19.5 0.25 43 3 1 0.48 1 0
i "Note" 19.75 0.25 45 3 1 0.48 1 0
i "Note" 20 0.5 43 3 1 0.48 1 0
i "Note" 20.5 0.5 42 3 1 0.48 1 0
i "Note" 21 0.5 40 3 1 0.48 1 0
i "Note" 21.5 0.5 42 3 1 0.48 1 0
i "Note" 22 1 38 3 1 0.52 0.94 0
i "Note" 23 1 33 2 1 0.48 0.94 1
i "Note" 24 1 38 3 1 0.48 0.94 1
i "Note" 25 0.5 31 1 1 0.52 1 1
i "Note" 25.5 0.5 38 1 1 0.48 0.94 0
i "Note" 26 1 47 4 1 0.48 1 1
i "Note" 27 0.5 45 4 1 0.48 1 0
i "Note" 27.5 0.25 47 4 1 0.48 1 0
i "Note" 27.75 0.25 48 4 1 0.48 1 0
i "Note" 28 0.5 47 4 1 0.52 1 0
i "Note" 28.5 0.5 45 4 1 0.48 0.94 0
i "Note" 29 0.5 43 3 1 0.48 1 1
i "Note" 29.5 0.5 42 3 1 0.48 1 0
i "Note" 30 0.5 43 3 1 0.48 1 0
i "Note" 30.5 0.5 38 3 1 0.48 1 0
i "Note" 31 0.5 40 3 1 0.52 1 0
i "Note" 31.5 0.5 43 3 1 0.48 1 0
i "Note" 32 0.5 48 3 1 0.48 1 0
i "Note" 32.5 0.5 45 3 1 0.48 1 0
i "Note" 33 0.5 42 3 1 0.48 1 0
i "Note" 33.5 0.5 50 3 1 0.48 1 0
i "Note" 34 2 31 1 1 0.44 0.94 1
i "Note" 34 2 38 3 1 0.44 1 0
i "Note" 34 2 47 4 1 0.52 1 1
i "Note" 36 1 38 3 1 0.44 0.94 0
i "Note" 36 1 45 4 1 0.48 0.94 0
i "Note" 37 0.5 33 2 1 0.52 1 1
i "Note" 37.5 0.5 42 2 1 0.48 0.94 0
i "Note" 38 1 48 4 1 0.48 1 1
i "Note" 39 0.5 47 4 1 0.48 1 0
i "Note" 39.5 0.25 48 4 1 0.48 1 0
i "Note" 39.75 0.25 50 4 1 0.48 1 0
i "Note" 40 0.5 48 4 1 0.52 1 0
i "Note" 40.5 0.5 47 4 1 0.48 1 0
i "Note" 41 0.5 45 4 1 0.48 1 0
i "Note" 41.5 0.5 43 4 1 0.48 0.94 0
i "Note" 42 0.5 42 3 1 0.48 1 1
i "Note" 42.5 0.5 40 3 1 0.48 1 0
i "Note" 43 0.5 42 3 1 0.52 1 0
i "Note" 43.5 0.25 43 3 1 0.48 1 0
i "Note" 43.75 0.25 45 3 1 0.48 1 0
i "Note" 44 0.5 43 3 1 0.48 1 0
i "Note" 44.5 0.5 42 3 1 0.48 1 0
i "Note" 45 0.5 40 3 1 0.48 1 0
i "Note" 45.5 0.5 42 3 1 0.48 1 0
i "Note" 46 1 38 3 1 0.52 0.94 0
i "Note" 47 1 33 2 1 0.48 0.94 1
i "Note" 48 1 38 3 1 0.48 0.94 1
i "Note" 49 0.5 38 3 1 0.52 1 1
i "Note" 49.5 0.5 42 3 1 0.48 0.94 0
i "Note" 50 1 45 4 1 0.48 1 1
i "Note" 51 0.5 43 4 1 0.48 1 0
i "Note" 51.5 0.25 45 4 1 0.48 1 0
i "Note" 51.75 0.25 47 4 1 0.48 1 0
i "Note" 52 0.5 45 4 1 0.52 1 0
i "Note" 52.5 0.5 43 4 1 0.48 0.94 0
i "Note" 53 0.5 42 3 1 0.48 1 1
i "Note" 53.5 0.5 40 3 1 0.48 1 0
i "Note" 54 0.5 38 3 1 0.48 1 0
i "Note" 54.5 0.5 42 3 1 0.48 0.94 0
i "Note" 55 0.5 35 2 1 0.52 1 1
i "Note" 55.5 0.5 38 2 1 0.48 1 0
i "Note" 56 0.5 44 2 1 0.48 0.94 0
i "Note" 56.5 0.5 45 4 1 0.48 1 1
i "Note" 57 0.5 47 4 1 0.48 1 0
i "Note" 57.5 0.5 50 4 1 0.48 0.94 0
i "Note" 58 0.5 33 2 1 0.52 0.94 1
i "Note" 58.5 0.5 50 4 1 0.48 1 1
i "Note" 59 0.5 48 4 1 0.48 1 0
i "Note" 59.5 0.5 47 4 1 0.48 1 0
i "Note" 60 1 48 4 1 0.48 0.94 0
i "Note" 61 0.5 39 3 1 0.52 1 1
i "Note" 61.5 0.5 42 3 1 0.48 1 0
i "Note" 62 0.5 45 3 1 0.48 0.94 0
i "Note" 62.5 0.5 48 4 1 0.48 1 1
i "Note" 63 0.5 47 4 1 0.48 1 0
i "Note" 63.5 0.5 45 4 1 0.48 0.94 0
i "Note" 64 0.5 47 1 1 0.52 1 1
i "Note" 64.5 0.5 40 1 1 0.48 1 0
i "Note" 65 0.5 31 1 1 0.48 0.94 0
i "Note" 65.5 0.5 45 4 1 0.48 1 1
i "Note" 66 0.5 48 4 1 0.48 1 0
i "Note" 66.5 0.5 47 4 1 0.48 0.94 0
i "Note" 67 0.5 45 3 1 0.52 1 1
i "Note" 67.5 0.5 43 3 1 0.48 1 0
i "Note" 68 0.5 42 3 1 0.48 1 0
i "Note" 68.5 0.5 40 3 1 0.48 0.94 0
i "Note" 69 0.5 35 2 1 0.48 0.94 1
i "Note" 69.5 0.5 39 3 1 0.48 0.94 1
i "Note" 70 1.5 28 1 1 0.52 0.94 1
i "Note" 71.5 0.5 40 3 1 0.48 1 1
i "Note" 72 0.5 38 3 1 0.48 0.94 0
i "Note" 72.5 0.5 36 2 1 0.48 0.94 1
i "Note" 73 0.5 35 2 1 0.52 1 1
i "Note" 73.5 0.5 38 2 1 0.48 0.94 0
i "Note" 74 1 43 4 1 0.48 0.94 1
i "Note" 75 0.5 38 3 1 0.48 1 1
i "Note" 75.5 0.25 40 3 1 0.48 1 0
i "Note" 75.75 0.25 41 3 1 0.48 1 0
i "Note" 76 0.5 41 3 1 0.52 1 0
i "Note" 76.5 0.5 38 3 1 0.48 1 0
i "Note" 77 0.5 40 3 1 0.48 0.94 0
i "Note" 77.5 0.5 36 2 1 0.48 1 1
i "Note" 78 0.5 36 2 1 0.48 1 0
i "Note" 78.5 0.5 35 2 1 0.48 1 0
i "Note" 79 0.5 37 2 1 0.52 1 0
i "Note" 79.5 0.5 40 2 1 0.48 0.94 0
i "Note" 80 1 45 4 1 0.48 0.94 1
i "Note" 81 0.5 40 3 1 0.48 1 1
i "Note" 81.5 0.25 42 3 1 0.48 1 0
i "Note" 81.75 0.25 43 3 1 0.48 1 0
i "Note" 82 0.5 43 3 1 0.52 1 0
i "Note" 82.5 0.5 40 3 1 0.48 1 0
i "Note" 83 0.5 42 3 1 0.48 1 0
i "Note" 83.5 0.5 38 3 1 0.48 1 0
i "Note" 84 0.5 38 3 1 0.48 0.94 0
i "Note" 84.5 0.5 33 2 1 0.48 0.94 1
i "Note" 85 0.5 38 3 1 0.52 1 1
i "Note" 85.5 0.5 42 3 1 0.48 1 0
i "Note" 86 0.5 45 3 1 0.48 0.94 0
i "Note" 86.5 0.5 48 4 1 0.48 1 1
i "Note" 87 0.5 47 4 1 0.48 1 0
i "Note" 87.5 0.5 50 4 1 0.48 0.94 0
i "Note" 88 0.5 40 3 1 0.52 1 1
i "Note" 88.5 0.5 43 3 1 0.48 1 0
i "Note" 89 0.5 47 3 1 0.48 0.94 0
i "Note" 89.5 0.5 50 4 1 0.48 1 1
i "Note" 90 0.5 48 4 1 0.48 1 0
i "Note" 90.5 0.5 52 4 1 0.48 1 0
i "Note" 91 0.5 50 4 1 0.52 0.94 0
i "Note" 91.5 0.5 42 3 1 0.48 0.94 1
i "Note" 92 0.5 43 4 1 0.48 0.94 1
i "Note" 92.5 0.5 35 2 1 0.48 0.94 1
i "Note" 93 0.5 38 3 1 0.48 1 1
i "Note" 93.5 0.5 42 3 1 0.48 0.94 0
i "Note" 94 3 31 1 1 0.44 0.94 1
i "Note" 94 3 43 4 1 0.52 0.94 1
i "Note" 97 0.5 38 3 1 0.52 1 1
i "Note" 97.5 0.5 42 3 1 0.48 0.94 0
i "Note" 98 1 45 4 1 0.48 1 1
i "Note" 99 0.5 43 4 1 0.48 1 0
i "Note" 99.5 0.25 45 4 1 0.48 1 0
i "Note" 99.75 0.25 47 4 1 0.48 1 0
i "Note" 100 0.5 45 4 1 0.52 1 0
i "Note" 100.5 0.5 43 4 1 0.48 0.94 0
i "Note" 101 0.5 42 3 1 0.48 1 1
i "Note" 101.5 0.5 40 3 1 0.48 1 0
i "Note" 102 0.5 38 3 1 0.48 1 0
i "Note" 102.5 0.5 42 3 1 0.48 0.94 0
i "Note" 103 0.5 35 2 1 0.52 1 1
i "Note" 103.5 0.5 38 2 1 0.48 1 0
i "Note" 104 0.5 44 2 1 0.48 0.94 0
i "Note" 104.5 0.5 45 4 1 0.48 1 1
i "Note" 105 0.5 47 4 1 0.48 1 0
i "Note" 105.5 0.5 50 4 1 0.48 0.94 0
i "Note" 106 0.5 33 2 1 0.52 0.94 1
i "Note" 106.5 0.5 50 4 1 0.48 1 1
i "Note" 107 0.5 48 4 1 0.48 1 0
i "Note" 107.5 0.5 47 4 1 0.48 1 0
i "Note" 108 1 48 4 1 0.48 0.94 0
i "Note" 109 0.5 39 3 1 0.52 1 1
i "Note" 109.5 0.5 42 3 1 0.48 1 0
i "Note" 110 0.5 45 3 1 0.48 0.94 0
i "Note" 110.5 0.5 48 4 1 0.48 1 1
i "Note" 111 0.5 47 4 1 0.48 1 0
i "Note" 111.5 0.5 45 4 1 0.48 0.94 0
i "Note" 112 0.5 47 1 1 0.52 1 1
i "Note" 112.5 0.5 40 1 1 0.48 1 0
i "Note" 113 0.5 31 1 1 0.48 0.94 0
i "Note" 113.5 0.5 45 4 1 0.48 1 1
i "Note" 114 0.5 48 4 1 0.48 1 0
i "Note" 114.5 0.5 47 4 1 0.48 0.94 0
i "Note" 115 0.5 45 3 1 0.52 1 1
i "Note" 115.5 0.5 43 3 1 0.48 1 0
i "Note" 116 0.5 42 3 1 0.48 1 0
i "Note" 116.5 0.5 40 3 1 0.48 0.94 0
i "Note" 117 0.5 35 2 1 0.48 0.94 1
i "Note" 117.5 0.5 39 3 1 0.48 0.94 1
i "Note" 118 1.5 28 1 1 0.52 0.94 1
i "Note" 119.5 0.5 40 3 1 0.48 1 1
i "Note" 120 0.5 38 3 1 0.48 0.94 0
i "Note" 120.5 0.5 36 2 1 0.48 0.94 1
i "Note" 121 0.5 35 2 1 0.52 1 1
i "Note" 121.5 0.5 38 2 1 0.48 0.94 0
i "Note" 122 1 43 4 1 0.48 0.94 1
i "Note" 123 0.5 38 3 1 0.48 1 1
i "Note" 123.5 0.25 40 3 1 0.48 1 0
i "Note" 123.75 0.25 41 3 1 0.48 1 0
i "Note" 124 0.5 41 3 1 0.52 1 0
i "Note" 124.5 0.5 38 3 1 0.48 1 0
i "Note" 125 0.5 40 3 1 0.48 0.94 0
i "Note" 125.5 0.5 36 2 1 0.48 1 1
i "Note" 126 0.5 36 2 1 0.48 1 0
i "Note" 126.5 0.5 35 2 1 0.48 1 0
i "Note" 127 0.5 37 2 1 0.52 1 0
i "Note" 127.5 0.5 40 2 1 0.48 0.94 0
i "Note" 128 1 45 4 1 0.48 0.94 1
i "Note" 129 0.5 40 3 1 0.48 1 1
i "Note" 129.5 0.25 42 3 1 0.48 1 0
i "Note" 129.75 0.25 43 3 1 0.48 1 0
i "Note" 130 0.5 43 3 1 0.52 1 0
i "Note" 130.5 0.5 40 3 1 0.48 1 0
i "Note" 131 0.5 42 3 1 0.48 1 0
i "Note" 131.5 0.5 38 3 1 0.48 1 0
i "Note" 132 0.5 38 3 1 0.48 0.94 0
i "Note" 132.5 0.5 33 2 1 0.48 0.94 1
i "Note" 133 0.5 38 3 1 0.52 1 1
i "Note" 133.5 0.5 42 3 1 0.48 1 0
i "Note" 134 0.5 45 3 1 0.48 0.94 0
i "Note" 134.5 0.5 48 4 1 0.48 1 1
i "Note" 135 0.5 47 4 1 0.48 1 0
i "Note" 135.5 0.5 50 4 1 0.48 0.94 0
i "Note" 136 0.5 40 3 1 0.52 1 1
i "Note" 136.5 0.5 43 3 1 0.48 1 0
i "Note" 137 0.5 47 3 1 0.48 0.94 0
i "Note" 137.5 0.5 50 4 1 0.48 1 1
i "Note" 138 0.5 48 4 1 0.48 1 0
i "Note" 138.5 0.5 52 4 1 0.48 1 0
i "Note" 139 0.5 50 4 1 0.52 0.94 0
i "Note" 139.5 0.5 42 3 1 0.48 0.94 1
i "Note" 140 0.5 43 4 1 0.48 0.94 1
i "Note" 140.5 0.5 35 2 1 0.48 0.94 1
i "Note" 141 0.5 38 3 1 0.48 1 1
i "Note" 141.5 0.5 42 3 1 0.48 0.94 0
i "Note" 142 7 31 1 1 0.44 [3*0.94/7] 1
i "Note" 142 7 43 4 1 0.52 [3*0.94/7] 1
e
</CsScore>
</CsoundSynthesizer>
