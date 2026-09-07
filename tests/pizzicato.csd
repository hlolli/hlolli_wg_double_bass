<CsoundSynthesizer>
<CsOptions>
-n -d -m128
</CsOptions>
<CsInstruments>

#ifndef TEST_FORCE
#define TEST_FORCE #0.5#
#endif
#ifndef TEST_POSITION
#define TEST_POSITION #0.10#
#endif
#ifndef TEST_SPEED
#define TEST_SPEED #0.66#
#endif
#ifndef TEST_TRIGGER_LEVEL
#define TEST_TRIGGER_LEVEL #1#
#endif
#ifndef TEST_RETRIGGER
#define TEST_RETRIGGER #0#
#endif

sr = 48000
ksmps = 32
nchnls = 2
0dbfs = 1

giBass hlolli_wg_double_bass_create

instr Voice
  kTime timeinsts
  if $TEST_RETRIGGER == 0 then
    kTrigger = kTime < 0.08 ? $TEST_TRIGGER_LEVEL : 0
  else
    kTrigger = (kTime < 0.08 || (kTime >= 0.12 && kTime < 0.20)) \
        ? $TEST_TRIGGER_LEVEL : 0
  endif
  aLeft, aRight hlolli_wg_double_bass \
      kTrigger, 41.20344461410875, $TEST_FORCE, $TEST_SPEED, \
      $TEST_POSITION, 0, 0, 5, 0, 0, 1, giBass
  outs aLeft, aRight
endin

instr ExciterProbe
  iArticulation, iMode, iArmed, iActive, iAge, iTotal, iRelease, \
      iNotch, iPosition, iContactTarget, iContact, iSpeedTarget, iSpeed, \
      iLastOutput, iEnergy, iPeak, iRng, iPendingImpact, iNormalAttacks, \
      iLeftAttacks, iBartokAttacks, iFingerboardImpacts, iWoodStrikes, \
      iTrattoAttacks, iTrattoSamples, iCollisionSamples, \
      iCollisionCompression, iCollisionForce, iExciterSamples, \
      iRecoveries, iFinite \
      hlolli_wg_double_bass_test_exciter giBass, 1
  printf_i "WG_PIZZICATO_EXCITER %d %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n", \
      1, p4, iActive, iLastOutput, iNormalAttacks, iExciterSamples, \
      iRecoveries, iFinite, iPeak
endin

instr Schedule
  event_i "i", "Voice", 0, 0.48
  event_i "i", "ExciterProbe", 0.020, 0.001, 1
  if $TEST_RETRIGGER == 1 then
    event_i "i", "ExciterProbe", 0.140, 0.001, 2
  endif
endin

</CsInstruments>
<CsScore>
i "Schedule" 0 0.001
e
</CsScore>
</CsoundSynthesizer>
