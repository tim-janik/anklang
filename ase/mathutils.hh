// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_MATHUTILS_HH__
#define __ASE_MATHUTILS_HH__

#include <ase/cxxaux.hh>

namespace Ase {

/// Double round-off error at 1.0, equals 2^-53
constexpr const double DOUBLE_EPSILON = 1.1102230246251565404236316680908203125e-16;

/// Round float to int, using round-to-nearest
/// Fast version of `f < 0 ? int (f - 0.5) : int (f + 0.5)`.
extern inline ASE_CONST int irintf (float f) { return __builtin_rintf (f); }

/// Force number into double precision floating point format, even with `-ffast-math`.
extern inline double force_double (double d) { volatile double v = d; return v; }

/// Force number into single precision floating point format, even with `-ffast-math`.
extern inline float  force_float  (float  f) { volatile float v = f; return v; }

/** Union to compartmentalize an IEEE-754 float.
 * IEEE 754 single precision floating point layout:
 * ```
 *        31 30           23 22            0
 * +--------+---------------+---------------+
 * | s 1bit | e[30:23] 8bit | f[22:0] 23bit |
 * +--------+---------------+---------------+
 * B0------------------->B1------->B2-->B3-->
 * ```
 */
union FloatIEEE754 {
  float         v_float;
  struct {
#if   __BYTE_ORDER == __LITTLE_ENDIAN
    uint mantissa : 23, biased_exponent : 8, sign : 1;
#elif __BYTE_ORDER == __BIG_ENDIAN
    uint sign : 1, biased_exponent : 8, mantissa : 23;
#endif
  } mpn;
  char          chars[4];
  static constexpr const float EPSILON = 5.9604644775390625e-08; ///< 2^-24, round-off error at 1.0
  static constexpr const int   BIAS = 127;                       ///< Exponent bias.
  static constexpr const float FMAX = 3.40282347e+38;            ///< 0x7f7fffff, 2^128 * (1 - epsilon)
  static constexpr const float FMIN = 1.17549435e-38;            ///< 0x00800000 Minimum Normal
  static constexpr const float SMAX = 1.17549421e-38;            ///< 0x007fffff Maximum Subnormal
  static constexpr const float SMIN = 1.40129846e-45;            ///< 0x00000001 Minimum Subnormal
};

/** Fast approximation of 2 raised to the power of `x`.
 * The parameter `x` is the exponent within `[-127.0…+127.0]`.
 * Within `-1…+1`, the error stays below 4e-7 which corresponds to a sample
 * precision of 21 bit. For integer values of `x` (i.e. `x - floor (x) -> 0`),
 * the error approaches zero. With FMA instructions and `-ffast-math` enabled,
 * execution times should be below 10ns on 3GHz machines.
 */
extern inline float fast_exp2   (float x) ASE_CONST;

/** Fast approximation of logarithm to base 2.
 * The parameter `x` is the exponent within `[1.1e-38…2^127]`.
 * Within `1e-7…+1`, the error stays below 3.8e-6 which corresponds to a sample
 * precision of 18 bit. When `x` is an exact power of 2, the error approaches
 * zero. With FMA instructions and `-ffast-math enabled`, execution times should
 * be below 10ns on 3GHz machines.
 */
extern inline float fast_log2   (float x) ASE_CONST;

/// Convert synthesizer value (Voltage) to Hertz.
extern inline float value2hz    (float x) ASE_CONST;

/// Convert Hertz to synthesizer value (Voltage).
extern inline float hz2value    (float x) ASE_CONST;

/// Finetune factor table with 201 entries for `-100…0…+100` cent.
extern const float *const cent_table;

/// Musical Tuning Tables, to be indexed by `MusicalTuning`
extern const float *const semitone_tables_265[17];

// == Implementations ==
extern inline ASE_CONST float
fast_exp2 (float ex)
{
  FloatIEEE754 fp = { 0, };
  // const int i = ex < 0 ? int (ex - 0.5) : int (ex + 0.5);
  const int i = irintf (ex);
  fp.mpn.biased_exponent = fp.BIAS + i;
  const float x = ex - i;
  float r;
  // f=2^x; remez(1, 5, [-.5;.5], 1/f, 1e-16); // minimized relative error
  r = x *  0.0013276471992255f;
  r = x * (0.0096755413344448f + r);
  r = x * (0.0555071327349880f + r);
  r = x * (0.2402211972384019f + r);
  r = x * (0.6931469670647601f + r);
  r = fp.v_float * (1.0f + r);
  return r;
}

extern inline ASE_CONST float
fast_log2 (float value)
{
  // log2 (i*x) = log2 (i) + log2 (x)
  FloatIEEE754 u { value };                     // v_float = 2^(biased_exponent-127) * mantissa
  const int i = u.mpn.biased_exponent - FloatIEEE754::BIAS; // extract exponent without bias
  u.mpn.biased_exponent = FloatIEEE754::BIAS;   // reset to 2^0 so v_float is mantissa in [1..2]
  float r, x = u.v_float - 1.0f;                // x=[0..1]; r = log2 (x + 1);
  // h=0.0113916; // offset to reduce error at origin
  // f=(1/log(2)) * log(x+1); dom=[0-h;1+h]; p=remez(f, 6, dom, 1);
  // p = p - p(0); // discard non-0 offset
  // err=p-f; plot(err,[0;1]); plot(f,p,dom); // result in sollya
  r = x *  -0.0259366993544709205147977455165000143561553284592936f;
  r = x * (+0.122047857676447181074792747820717519424533931189428f + r);
  r = x * (-0.27814297685064327713977752916286528359628147166014f + r);
  r = x * (+0.45764712300320092992105460899527194244236573556309f + r);
  r = x * (-0.71816105664624015087225994551041120290062342459945f + r);
  r = x * (+1.44254540258782520489769598315182363877204824648687f + r);
  return i + r; // log2 (i) + log2 (x)
}

} // Ase

#endif // __ASE_MATHUTILS_HH__
