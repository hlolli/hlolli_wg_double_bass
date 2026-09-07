<CsoundSynthesizer>
<CsOptions>
-n -d -m128
</CsOptions>
<CsInstruments>

#ifndef TEST_FORCE
#define TEST_FORCE #0.475#
#endif
#ifndef TEST_SPEED
#define TEST_SPEED #0.20#
#endif
#ifndef TEST_POSITION
#define TEST_POSITION #0.12#
#endif
#ifndef TEST_STRING
#define TEST_STRING #1#
#endif
#ifndef TEST_FREQUENCY
#define TEST_FREQUENCY #41.20344461410875#
#endif
#ifndef TEST_RELEASE_TIME
#define TEST_RELEASE_TIME #1.0#
#endif

sr = 48000
ksmps = 32
nchnls = 2
0dbfs = 1

giBass hlolli_wg_double_bass_create

instr Voice
  kGate = timeinsts() < $TEST_RELEASE_TIME ? 1 : 0
  aLeft, aRight hlolli_wg_double_bass \
      kGate, $TEST_FREQUENCY, $TEST_FORCE, $TEST_SPEED, $TEST_POSITION, \
      0, 0, 0, 0, 0, $TEST_STRING, giBass
  outs aLeft, aRight
endin

instr Probe
  iState, iRequested, iEffective, iBowSpeed, iContact, iTargetPosition, \
      iEffectivePosition, iBridgeDelay, iNutDelay, iImpedance, iFreeVelocity, \
      iStringVelocity, iRelativeVelocity, iFrictionForce, iJunctionIncrement, \
      iMinForce, iMaxForce, iResidual, iBracketWidth, iRootCount, \
      iIterationsLast, iIterationsMax, iSolverCalls, iSolverFailures, \
      iSolverFallbacks, iStickSamples, iSlipSamples, iScratchSamples, \
      iNoMotionSamples, iStateTransitions, iDirectionChanges, iAttacks, \
      iRecoveries, iScratchScore, iMaxAbsRelative, iBowFinite \
      hlolli_wg_double_bass_test_bow giBass, $TEST_STRING
  iArticulation, iActive, iPhase, iForceTarget, iSpeedTarget, iForceOutput, \
      iSpeedOutput, iContactOutput, iOnsetSamples, iStrokeSamples, iStrokes, \
      iBowChanges, iPresetChanges, iDirectOverrides, iGestureRecoveries, \
      iReleaseArticulation, iReleaseGainTarget, iGestureFinite \
      hlolli_wg_double_bass_test_gesture giBass, $TEST_STRING
  iP01, iP02, iP03, iP04, iP05, iP06, iP07, iP08, \
      iP09, iP10, iP11, iP12, iP13, iP14, iP15, iP16, \
      iP17, iP18, iP19, iP20, iP21, iP22, iP23, iP24, \
      iP25, iP26, iP27, iP28, iPhysicsRecoveries, \
      iSecondPolarization, iPhysicsFinite \
      hlolli_wg_double_bass_test_physics giBass, $TEST_STRING

  printf_i "WG_BOW_CONTROL %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n", \
      1, iRequested, iEffective, iBowSpeed, iContact, iTargetPosition, \
      iEffectivePosition, iImpedance, iMinForce, iMaxForce, iBowFinite, \
      iForceTarget, iSpeedTarget, iForceOutput, iSpeedOutput, iContactOutput, \
      iGestureRecoveries, iGestureFinite, iReleaseArticulation, \
      iReleaseGainTarget, iState, iActive
  printf_i "WG_POLARIZATION %.17g %.17g\n", \
      1, iSecondPolarization, iPhysicsFinite
endin

instr Schedule
  event_i "i", "Voice", 0, 0.24
  ; The 200 ms lead time is much longer than every direct-arco transition and
  ; smoothing interval, so the probe observes the steady control mapping.
  event_i "i", "Probe", 0.20, 0.001
endin

</CsInstruments>
<CsScore>
i "Schedule" 0 0.001
e
</CsScore>
</CsoundSynthesizer>
