/* Check the portable feedback math against independent long-double libm.
   Including the canonical source keeps these static helpers out of the API. */
#include "../src/hlolli_wg_double_bass.c"
#include <stdio.h>

static int check_cos(double angle)
{
  const double actual = wg_double_bass_contact_cos(angle);
  const long double expected = cosl((long double)angle);
  if (!isfinite(actual) || actual < -1.0 || actual > 1.0 ||
      fabsl((long double)actual - expected) > 2.0L * DBL_EPSILON) {
    fprintf(stderr, "contact cos inaccurate at %.17g: %.17g vs %.21Lg\n",
            angle, actual, expected);
    return 0;
  }
  return 1;
}

static int check_exp(double argument)
{
  const double actual = wg_double_bass_bow_exp(argument);
  const long double expected = expl((long double)argument);
  const long double tolerance =
      2.0L * DBL_EPSILON * expected + DBL_TRUE_MIN;
  if (!isfinite(actual) || actual < 0.0 ||
      fabsl((long double)actual - expected) > tolerance) {
    fprintf(stderr, "bow exp inaccurate at %.17g: %.17g vs %.21Lg\n",
            argument, actual, expected);
    return 0;
  }
  return 1;
}

static int check_log(double argument)
{
  const double actual = wg_double_bass_bow_log(argument);
  const long double expected = logl((long double)argument);
  const long double tolerance = 2.0L * DBL_EPSILON * fabsl(expected);
  if (!isfinite(actual) ||
      fabsl((long double)actual - expected) > tolerance) {
    fprintf(stderr, "bow log inaccurate at %.17g: %.17g vs %.21Lg\n",
            argument, actual, expected);
    return 0;
  }
  return 1;
}

int main(void)
{
  const double pi = 0.5 * WG_DOUBLE_BASS_TWO_PI;
  double previous = 1.0;
  double previous_log = -INFINITY;
  uint32_t index;
  int exponent;

  for (index = 0U; index <= 32768U; index++) {
    const double fraction = (double)index / 32768.0;
    const double angle = pi * fraction;
    const double cosine = wg_double_bass_contact_cos(angle);
    if (!check_cos(angle) || cosine > previous ||
        !check_exp(-12.0 + 14.0 * fraction) ||
        !check_exp(-745.0 + 1454.0 * fraction)) {
      return 1;
    }
    /* Full clamped force-map endpoint range, independent of score controls. */
    const double force = 0.005 + (4.0 - 0.005) * fraction;
    const double logarithm = wg_double_bass_bow_log(force);
    if (!check_log(force) || logarithm < previous_log) {
      return 1;
    }
    previous_log = logarithm;
    previous = cosine;
  }
  /* Neighbors of range-reduction boundaries, plus the identified libm
     rounding cases. These are mathematical arguments, not audio fixtures. */
  for (index = 1U; index <= 3U; index++) {
    const double angle = 0.25 * pi * (double)index;
    if (!check_cos(nextafter(angle, 0.0)) || !check_cos(angle) ||
        !check_cos(nextafter(angle, pi))) {
      return 1;
    }
  }
  /* Powers and neighbors exercise normal/subnormal exponent reduction and
     the near-one branch; sqrt(2) spans the mantissa-reduction boundary. */
  for (exponent = -1074; exponent <= 1023; exponent++) {
    const double power = ldexp(1.0, exponent);
    if (!check_log(power) || !check_log(nextafter(power, INFINITY)) ||
        (power > DBL_TRUE_MIN && !check_log(nextafter(power, 0.0)))) {
      return 1;
    }
  }
  {
    const uint32_t boundaries[] = {
      0x3fe6a09cU, 0x3ff6147aU, 0x3ff6b851U, 0x3ff00001U
    };
    for (index = 0U; index < sizeof(boundaries) / sizeof(boundaries[0]); index++) {
      const double value = wg_double_bass_math_from_bits(
          (uint64_t)boundaries[index] << 32);
      if (!check_log(nextafter(value, 0.0)) || !check_log(value) ||
          !check_log(nextafter(value, INFINITY))) {
        return 1;
      }
    }
  }
  if (!check_log(0x1.66e4ab527777dp-4) || !check_log(DBL_MAX) ||
      wg_double_bass_math_bits(wg_double_bass_bow_log(0x1.66e4ab527777dp-4)) !=
          UINT64_C(0xc0037a5998fd5f4a) ||
      wg_double_bass_bow_log(1.0) != 0.0 ||
      signbit(wg_double_bass_bow_log(1.0)) ||
      wg_double_bass_bow_log(0.0) != -INFINITY ||
      wg_double_bass_bow_log(-0.0) != -INFINITY ||
      wg_double_bass_bow_log(INFINITY) != INFINITY ||
      !isnan(wg_double_bass_bow_log(-1.0)) ||
      !isnan(wg_double_bass_bow_log(-INFINITY)) ||
      !isnan(wg_double_bass_bow_log(NAN)) ||
      !check_cos(0x1.66adff283b0f9p+1) ||
      !check_exp(-0x1.255c5fff9722fp+0) ||
      !check_exp(1.0) || !check_exp(-0.0) ||
      wg_double_bass_contact_cos(0.0) != 1.0 ||
      wg_double_bass_contact_cos(pi) != -1.0 ||
      wg_double_bass_bow_exp(-INFINITY) != 0.0 ||
      wg_double_bass_bow_exp(INFINITY) != INFINITY ||
      !isnan(wg_double_bass_bow_exp(NAN)) ||
      !isinf(wg_double_bass_bow_exp(710.0)) ||
      wg_double_bass_bow_exp(-746.0) != 0.0) {
    fputs("portable bow math: traced value or IEEE edge case failed\n", stderr);
    return 1;
  }
  puts("portable bow math: bounded cosine, accurate exp and monotone log passed");
  return 0;
}
