<CsoundSynthesizer>
<CsOptions>
-n -d -m128
</CsOptions>
<CsInstruments>

#ifndef TEST_A4
#define TEST_A4 #440#
#endif

sr = 48000
ksmps = 32
nchnls = 2
0dbfs = 1

; This score checks only the public numeric constructor's stated input range.
instr Create
  iHandle hlolli_wg_double_bass_create $TEST_A4
  printf_i "WG_CONSTRUCTOR_BOUND %.17g %.17g\n", \
      1, $TEST_A4, iHandle
endin

</CsInstruments>
<CsScore>
i "Create" 0 0.001
e 0.002
</CsScore>
</CsoundSynthesizer>
