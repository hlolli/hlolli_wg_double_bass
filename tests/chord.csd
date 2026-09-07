<CsoundSynthesizer>
<CsOptions>
-d -m128
</CsOptions>
<CsInstruments>

#ifndef TEST_SR
#define TEST_SR #48000#
#endif
#ifndef TEST_KSMPS
#define TEST_KSMPS #32#
#endif
#ifndef TEST_CASE
#define TEST_CASE #1#
#endif
#ifndef TEST_HARMONIC
#define TEST_HARMONIC #0#
#endif
#ifndef TEST_REVERSE
#define TEST_REVERSE #0#
#endif
#ifndef TEST_BODY
#define TEST_BODY #0.68#
#endif
#ifndef TEST_SYMPATHETIC
#define TEST_SYMPATHETIC #0.7#
#endif
#ifndef TEST_DRY
#define TEST_DRY #0.03#
#endif
#ifndef TEST_WET
#define TEST_WET #0.45#
#endif

sr = $TEST_SR
ksmps = $TEST_KSMPS
nchnls = 2
0dbfs = 1

giBass hlolli_wg_double_bass_create

instr Voice
  kTrigger init 0.58
  kFrequency init p5
  kForce init p6
  kSpeed init p7
  aLeft, aRight hlolli_wg_double_bass \
      kTrigger, kFrequency, kForce, kSpeed, p8, \
      0, 0, 0, 0, 0, p4, giBass
  outs $TEST_DRY * aLeft, $TEST_DRY * aRight
endin

instr Renderer
  aLeft, aRight hlolli_wg_double_bass_resonance \
      giBass, $TEST_BODY, $TEST_SYMPATHETIC, 0
  outs $TEST_WET * aLeft, $TEST_WET * aRight
endin

instr KeepAlive
  aZero init 0
  outs aZero, aZero
endin

instr Probe
  iP01, iP02, iP03, iP04, iP05, iP06, iP07, iP08, \
      iP09, iP10, iP11, iP12, iP13, iP14, iP15, iP16, \
      iP17, iP18, iP19, iP20, iP21, iP22, iP23, iP24, \
      iP25, iP26, iP27, iP28, iP29, iP30, iP31 \
      hlolli_wg_double_bass_test_physics giBass, p4
  printf_i \
      "WG_CHORD %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n", \
      1, p4, iP07, iP25, iP26, iP27, iP28, iP29, iP30, iP31

  iS01, iS02, iS03, iS04, iS05, iS06, iS07, iS08, \
      iS09, iS10, iS11, iS12, iS13, iS14, iS15, iS16, \
      iS17, iS18, iS19, iS20, iS21, iS22 \
      hlolli_wg_double_bass_test_strange giBass, p4
  printf_i \
      "WG_SYMPATHY %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n", \
      1, p4, iS02, iS03, iS16, iS17, iS19, iS20, iS21, iS22
endin


; Ending an opcode is not a left-hand open-string command.
instr TailVoice
  aLeft, aRight hlolli_wg_double_bass \
      1, p5 * ($TEST_HARMONIC == 0 ? 1 : $TEST_HARMONIC), 0.48, -0.34, 0.12, \
      0, 0, 0, $TEST_HARMONIC, 0, p4, giBass
  outs $TEST_DRY * aLeft, $TEST_DRY * aRight
endin

instr TailProbe
  iF01, iF02, iF03, iF04, iF05, iF06, iF07, iF08, \
      iF09, iF10, iF11, iF12, iF13, iF14, iF15, iF16, \
      iF17, iF18 \
      hlolli_wg_double_bass_test_finger giBass, p4
  iH01, iH02, iH03, iH04, iH05, iH06, iH07, iH08, \
      iH09, iH10, iH11, iH12, iH13, iH14, iH15, iH16, \
      iH17, iH18, iH19 \
      hlolli_wg_double_bass_test_harmonic giBass, p4
  iW01, iW02, iW03, iW04, iW05, iW06, iW07, iW08, \
      iW09, iW10, iW11, iW12, iW13, iW14, iW15, iW16, \
      iW17, iW18, iW19, iW20, iW21, iW22, iW23, iW24, \
      iW25 \
      hlolli_wg_double_bass_test_waveguide giBass, p4
  printf_i "WG_TAIL_%d %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n", \
      1, p5, p4, iW04, iF02, iF14, iH02, iW18, iW06 + iW07, \
      iW21, iW25, iF18, iH19
endin

instr Schedule
  event_i "i", "KeepAlive", 0, 0.8
  if $TEST_CASE == 1 then
    if $TEST_REVERSE == 0 then
      event_i "i", "Voice", 0, 0.65, 1, 41.20344461410875, 0.44, -0.30, 0.11
      event_i "i", "Voice", 0, 0.65, 2, 55, 0.46, -0.32, 0.12
      event_i "i", "Voice", 0, 0.65, 3, 73.41619197935188, 0.48, -0.34, 0.13
      event_i "i", "Voice", 0, 0.65, 4, 97.99885899543733, 0.50, -0.36, 0.14
    else
      event_i "i", "Voice", 0, 0.65, 4, 97.99885899543733, 0.50, -0.36, 0.14
      event_i "i", "Voice", 0, 0.65, 3, 73.41619197935188, 0.48, -0.34, 0.13
      event_i "i", "Voice", 0, 0.65, 2, 55, 0.46, -0.32, 0.12
      event_i "i", "Voice", 0, 0.65, 1, 41.20344461410875, 0.44, -0.30, 0.11
    endif
    event_i "i", "Renderer", 0, 0.8
  elseif $TEST_CASE == 2 then
    ; Drive a stopped A string while E, D, and G remain unowned and open.
    ; Body output is disabled by the runner so this isolates string coupling.
    event_i "i", "Voice", 0, 0.65, 2, 110, 0.48, -0.34, 0.12
    event_i "i", "Renderer", 0, 0.8
  else
    event_i "i", "TailVoice", 0, 0.3, 1, 1.5 * 41.20344461410875
    event_i "i", "TailVoice", 0, 0.3, 2, 1.5 * 55
    event_i "i", "TailVoice", 0, 0.3, 3, 1.5 * 73.41619197935188
    event_i "i", "TailVoice", 0, 0.3, 4, 1.5 * 97.99885899543733
    event_i "i", "Renderer", 0, 0.8
    iString = 1
    while iString <= 4 do
      event_i "i", "TailProbe", 0.25, 0.001, iString, 1
      event_i "i", "TailProbe", 0.45, 0.001, iString, 2
      iString += 1
    od
  endif
  event_i "i", "Probe", 0.55, 0.001, 1
  event_i "i", "Probe", 0.55, 0.001, 2
  event_i "i", "Probe", 0.55, 0.001, 3
  event_i "i", "Probe", 0.55, 0.001, 4
endin

</CsInstruments>
<CsScore>
i "Schedule" 0 0.001
e 0.8
</CsScore>
</CsoundSynthesizer>
