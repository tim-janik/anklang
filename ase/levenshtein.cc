// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "levenshtein.hh"
#include "internal.hh"
#include <string_view>
#include <vector>

// https://dl.acm.org/doi/10.1145/1963190.1963191 — Indexing methods for approximate dictionary searching: Comparative analysis: ACM Journal of Experimental Algorithmics: Vol 16

namespace Ase {

/** Damerau-Levenshtein Distance with restricted transposition.
 * Calculate the restricted Damerau-Levenshtein string distance with quadratic time complexity and
 * linear memory requirement. Memory: 12 * max(|source|,|target|) + constant.
 */
float
damerau_levenshtein_restricted (const std::string &source, const std::string &target,
                                const float ci, const float cd, const float cs, const float ct)
{
  // Compute optimal string alignment distance or restricted edit distance:
  // https://en.wikipedia.org/wiki/Damerau-Levenshtein_distance
  auto min3 = [] (float a, float b, float c) { return std::min (a, std::min (b, c)); };
  ssize_t n = source.size(), m = target.size();
  // increase cache locality
  if (m > n)
    return damerau_levenshtein_restricted (target, source, cd, ci, cs, ct);
  const char *a = source.data(), *b = target.data();
  // remove common prefix
  while (n && m && *a == *b)
    a++, b++, n--, m--;
  // remove common suffix
  while (n && m && a[n-1] == b[m-1])
    n--, m--;
  // handle zero length strings
  if (!n)
    return m * ci;
  if (!m)
    return n * cd;
  // allocate 3 rows: row2=row[i-2], row1=row[i-1], row0=row[i]
  std::vector<float> rowmem (3 * (m+1));
  float *row2 = rowmem.data(), *row1 = row2 + m+1, *row0 = row1 + m+1;
  // the last row corresponds to source + insertions -> target
  for (ssize_t j = 0; j <= m; j++)
    row1[j] = j * ci;
  // fill the matrix, note that a and b are 1-indexed
  for (ssize_t i = 1; i <= n; i++) {
    row0[0] = i * cd; // i && !j corresponds to source + deletion -> target
    for (ssize_t j = 1; j <= m; j++) {
      const bool ue = a[i-1] != b[j-1];
      row0[j] = min3 (row1[j] + cd,             // deletion
                      row0[j-1] + ci,           // insertion
                      row1[j-1] + cs * ue);     // substitution
      if (ue && i > 1 && j > 1 && a[i-1] == b[j-2] && a[i-2] == b[j-1])
        row0[j] = std::min (row0[j], row2[j-2] + ct);
    }
    // shift rows along
    float *const next = row2;
    row2 = row1;
    row1 = row0;
    row0 = next;
  }
  return row1[m];
}

template<typename T>
struct L2DMatrix {
  std::vector<T> v;
  const size_t sa, sb;
  L2DMatrix (size_t a, size_t b, T init = {}) :
    sa (a), sb (b)
  {
    v.resize (sa * sb, init);
  }
  T&
  operator() (size_t a, size_t b)
  {
    static T tmp{};
    assert_return (a < sa, tmp);
    assert_return (b < sb, tmp);
    return v[a * sb + b];
  }
};

// Damerau-Levenshtein Distance with edited transpositions, quadratic memory requirement
static float
damerau_levenshtein_unrestricted (const std::string_view &source, const std::string_view &target,
                                  const float ci, const float cd, const float cs, const float ct)
{
  auto u = [] (char c) { return uint8_t (c); };
  const size_t lp = source.size(), ls = target.size();
  const char *p = source.data()-1, *s = target.data()-1; // strings p,s are 1-indexed
  // Computation of the unrestricted Damerau-Levenshtein distance between strings p and s
  L2DMatrix<float> C (lp+1, ls+1);      // C: [0..|p|,0..|s|]
  const int S = 256;                    // |Σ| for unsigned char
  std::vector<int> CP (S, 0);           // CP: [1..|Σ|]; we use 0…255 however
  for (int i = 0; i <= lp; i++)
    C(i,0) = i * ci;
  for (int j = 0; j <= ls; j++)
    C(0,j) = j * cd;
  // CP[0…255] is 0-initialized; // CP[1…|Σ|]:=0
  for (int i = 1; i <= lp; i++) {
    int CS = 0;
    for (int j = 1; j <= ls; j++) {
      const float d = p[i] == s[j] ? 0 : cs;    // if p[i]=s[j] then d:=0 else d:=1
      C(i,j) = std::min (C(i-1,j) + ci,         // insertion cost
                         C(i,j-1) + cd);        // deletion cost
      C(i,j) = std::min (C(i,j),
                         C(i-1,j-1) + d);       // substitution cost
      const int i_ = CP[u(s[j])];               // CP[c] stores the largest index i_<i such that p[i_]=c
      const int j_ = CS;                        // CS stores the largest index j_<j such that s[j_] = p[i]
      if (i_ > 0 and j_ > 0)
        C(i,j) = std::min (C(i,j), C(i_-1,j_-1) + (i-i_)-1 + (j-j_)-1 + ct);
      if (p[i] == s[j])
        CS = j;
    }
    CP[u(p[i])] = i;
  }
  return C(lp,ls);
}

/** Damerau-Levenshtein Distance with unrestricted transpositions.
 * Calculate the unrestricted Damerau-Levenshtein string distance with quadratic time complexity and
 * quadratic memory requirement. Memory: 4 * |source|*|target| + constant.
 */
float
damerau_levenshtein_distance (const std::string &source, const std::string &target,
                              const float ci, const float cd, const float cs, const float ct)
{
  size_t n = source.size(), m = target.size();
  const char *a = source.data(), *b = target.data();
  // remove common prefix
  while (n && m && *a == *b)
    a++, b++, n--, m--;
  // remove common suffix
  while (n && m && a[n-1] == b[m-1])
    n--, m--;
  // handle zero length strings
  if (!n)
    return m;
  if (!m)
    return n;
  // calc Damerau-Levenshtein Distance on differing fragments only
  if (m <= n)
    return damerau_levenshtein_unrestricted ({a, n}, {b, m}, ci, cd, cs, ct);
  else // swap strings to increase cache locality
    return damerau_levenshtein_unrestricted ({b, m}, {a, n}, cd, ci, cs, ct);
}

} // Ase


#include "testing.hh"

namespace { // Anon
using namespace Ase;

TEST_INTEGRITY (levenshtein_tests);
static void
levenshtein_tests()
{
  // damerau_levenshtein_restricted - no editing of transposed character pairs
  TCMP (0, ==, damerau_levenshtein_restricted ("", ""));
  TCMP (0, ==, damerau_levenshtein_restricted ("A", "A"));
  TCMP (0, ==, damerau_levenshtein_restricted ("AZ", "AZ"));
  TCMP (1, ==, damerau_levenshtein_restricted ("", "1"));
  TCMP (2, ==, damerau_levenshtein_restricted ("", "12"));
  TCMP (3, ==, damerau_levenshtein_restricted ("", "123"));
  TCMP (1, ==, damerau_levenshtein_restricted ("1", ""));
  TCMP (2, ==, damerau_levenshtein_restricted ("12", ""));
  TCMP (3, ==, damerau_levenshtein_restricted ("123", ""));
  TCMP (1, ==, damerau_levenshtein_restricted ("A", "B"));
  TCMP (1, ==, damerau_levenshtein_restricted ("AB", "BA"));
  TCMP (3, ==, damerau_levenshtein_restricted ("ABC", "CA"));         // restricted edit distance: CA -> A -> AB -> ABC
  TCMP (3, ==, damerau_levenshtein_restricted ("123", "abc"));
  TCMP (5, ==, damerau_levenshtein_restricted ("12345", "abc"));
  TCMP (4, ==, damerau_levenshtein_restricted ("123", "abcd"));
  TCMP (1, ==, damerau_levenshtein_restricted ("AaaaB", "AaaaC"));
  TCMP (2, ==, damerau_levenshtein_restricted ("Aa_aB", "AaaaC"));
  TCMP (2, ==, damerau_levenshtein_restricted ("aAaaB", "AaaaC"));
  TCMP (3, ==, damerau_levenshtein_restricted ("___Ab#-##^^^", "___bA##+#^^^"));
  TCMP (1, ==, damerau_levenshtein_restricted ("_ABC", "ABC"));
  TCMP (1, ==, damerau_levenshtein_restricted ("ABCD", "BCD"));
  TCMP (3, ==, damerau_levenshtein_restricted ("BADCFE", "ABCDEF"));
  TCMP (2, ==, damerau_levenshtein_restricted ("AAAArzxyAzxy", "AArzxyAzxy"));
  TCMP (3, ==, damerau_levenshtein_restricted ("ab+cd+ef", "ba+dc+fe"));
  TCMP (5, ==, damerau_levenshtein_restricted ("ab+cd+ef", "ba_dc_fe"));
  TCMP (3, ==, damerau_levenshtein_restricted ("kitten", "sitting"));
  TCMP (4, ==, damerau_levenshtein_restricted ("AGTACGCA", "TATGC"));  // -A -G C2T -A
  TCMP (2, ==, damerau_levenshtein_restricted ("a cat", "an act"));
  TCMP (4, ==, damerau_levenshtein_restricted ("a cat", "an abct"));   // +n -c +b +c

  // damerau_levenshtein_distance - allows insert/delete between transposed character pair
  TCMP (0, ==, damerau_levenshtein_distance ("", ""));
  TCMP (0, ==, damerau_levenshtein_distance ("A", "A"));
  TCMP (0, ==, damerau_levenshtein_distance ("AZ", "AZ"));
  TCMP (1, ==, damerau_levenshtein_distance ("", "1"));
  TCMP (2, ==, damerau_levenshtein_distance ("", "12"));
  TCMP (3, ==, damerau_levenshtein_distance ("", "123"));
  TCMP (1, ==, damerau_levenshtein_distance ("1", ""));
  TCMP (2, ==, damerau_levenshtein_distance ("12", ""));
  TCMP (3, ==, damerau_levenshtein_distance ("123", ""));
  TCMP (1, ==, damerau_levenshtein_distance ("A", "B"));
  TCMP (1, ==, damerau_levenshtein_distance ("AB", "BA"));
  TCMP (2, ==, damerau_levenshtein_distance ("ABC", "CA"));     // edits in adjacent transpositions: CA -> AC -> ABC
  TCMP (3, ==, damerau_levenshtein_distance ("123", "abc"));
  TCMP (5, ==, damerau_levenshtein_distance ("12345", "abc"));
  TCMP (4, ==, damerau_levenshtein_distance ("123", "abcd"));
  TCMP (1, ==, damerau_levenshtein_distance ("AaaaB", "AaaaC"));
  TCMP (2, ==, damerau_levenshtein_distance ("Aa_aB", "AaaaC"));
  TCMP (2, ==, damerau_levenshtein_distance ("aAaaB", "AaaaC"));
  TCMP (3, ==, damerau_levenshtein_distance ("___Ab#-##^^^", "___bA##+#^^^"));
  TCMP (1, ==, damerau_levenshtein_distance ("_ABC", "ABC"));
  TCMP (1, ==, damerau_levenshtein_distance ("ABCD", "BCD"));
  TCMP (3, ==, damerau_levenshtein_distance ("BADCFE", "ABCDEF"));
  TCMP (2, ==, damerau_levenshtein_distance ("AAAArzxyAzxy", "AArzxyAzxy"));
  TCMP (3, ==, damerau_levenshtein_distance ("ab+cd+ef", "ba+dc+fe"));
  TCMP (5, ==, damerau_levenshtein_distance ("ab+cd+ef", "ba_dc_fe"));
  TCMP (3, ==, damerau_levenshtein_distance ("kitten", "sitting"));
  TCMP (4, ==, damerau_levenshtein_distance ("AGTACGCA", "TATGC"));  // -A -G C2T -A
  TCMP (2, ==, damerau_levenshtein_distance ("a cat", "an act"));
  TCMP (3, ==, damerau_levenshtein_distance ("a cat", "an abct"));   // +n ca2ac +b
}

} // Anon
